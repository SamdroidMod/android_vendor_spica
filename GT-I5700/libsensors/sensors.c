/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Sensors"

#define LOG_NDEBUG 1

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <sys/select.h>

#include <linux/input.h>
#include "ak8973b.h"

#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>

#define __MAX(a,b) ((a)>=(b)?(a):(b))

/*****************************************************************************/

#define MAX_NUM_SENSORS 4

#define SUPPORTED_SENSORS  ((1<<MAX_NUM_SENSORS)-1)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define ID_A  (0)
#define ID_M  (1)
#define ID_O  (2)
#define ID_T  (3)

static int id_to_sensor[MAX_NUM_SENSORS] = {
    [ID_A] = SENSOR_TYPE_ACCELEROMETER,
    [ID_M] = SENSOR_TYPE_MAGNETIC_FIELD,
    [ID_O] = SENSOR_TYPE_ORIENTATION,
    [ID_T] = SENSOR_TYPE_TEMPERATURE,
};

#define SENSORS_ACCELERATION   (1<<ID_A)
#define SENSORS_MAGNETIC_FIELD (1<<ID_M)
#define SENSORS_ORIENTATION    (1<<ID_O)
#define SENSORS_TEMPERATURE    (1<<ID_T)
#define SENSORS_GROUP          ((1<<ID_A)|(1<<ID_M)|(1<<ID_O)|(1<<ID_T))

/*****************************************************************************/

struct sensors_control_context_t {
    struct sensors_control_device_t device; // must be first
    int akmd_fd;
    uint32_t active_sensors;
};

struct sensors_data_context_t {
    struct sensors_data_device_t device; // must be first
    int events_fd;
    sensors_data_t sensors[MAX_NUM_SENSORS];
    uint32_t pendingSensors;
};

/*
 * The SENSORS Module
 */

static const struct sensor_t sSensorList[] = {
        { "BMA150 3-axis Accelerometer",
                "The Android Open Source Project",
                1, SENSORS_HANDLE_BASE+ID_A,
                SENSOR_TYPE_ACCELEROMETER, 2.8f, 1.0f/4032.0f, 3.0f, { } },
        { "AK8973 3-axis Magnetic field sensor",
                "The Android Open Source Project",
                1, SENSORS_HANDLE_BASE+ID_M,
                SENSOR_TYPE_MAGNETIC_FIELD, 2000.0f, 1.0f, 6.7f, { } },
        { "AK8973 Orientation sensor",
                "The Android Open Source Project",
                1, SENSORS_HANDLE_BASE+ID_O,
                SENSOR_TYPE_ORIENTATION, 360.0f, 1.0f, 9.7f, { } },
        { "AK8973 Temperature sensor",
                "The Android Open Source Project",
                1, SENSORS_HANDLE_BASE+ID_T,
                SENSOR_TYPE_TEMPERATURE, 80.0f, 1.0f, 0.0f, { } },
};

static int open_sensors(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static uint32_t sensors__get_sensors_list(struct sensors_module_t* module,
        struct sensor_t const** list) 
{
    *list = sSensorList;
    return sizeof(sSensorList)/sizeof(sSensorList[0]);
}

static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors
};

const struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "AK8973 SENSORS Module",
        .author = "The Android Open Source Project",
        .methods = &sensors_module_methods,
    },
    .get_sensors_list = sensors__get_sensors_list
};

/*****************************************************************************/

#define AKM_DEVICE_NAME     "/dev/akm8973_aot"

// sensor IDs must be a power of two and
// must match values in SensorManager.java
#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Y
#define EVENT_TYPE_ACCEL_Z          ABS_Z
#define EVENT_TYPE_ACCEL_STATUS     ABS_WHEEL

#define EVENT_TYPE_YAW              ABS_RX
#define EVENT_TYPE_PITCH            ABS_RY
#define EVENT_TYPE_ROLL             ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS    ABS_RUDDER

#define EVENT_TYPE_MAGV_X           ABS_HAT0X
#define EVENT_TYPE_MAGV_Y           ABS_HAT0Y
#define EVENT_TYPE_MAGV_Z           ABS_BRAKE

#define EVENT_TYPE_TEMPERATURE      ABS_THROTTLE
#define EVENT_TYPE_STEP_COUNT       ABS_GAS

// 720 LSG = 1G
#define LSG                         (720.0f)

// conversion of acceleration data to SI units (m/s^2)
#define CONVERT_A                   (GRAVITY_EARTH / LSG)
#define CONVERT_A_X                 (-CONVERT_A)
#define CONVERT_A_Y                 (CONVERT_A)
#define CONVERT_A_Z                 (-CONVERT_A)

// conversion of magnetic data to uT units
#define CONVERT_M                   (1.0f/16.0f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (-CONVERT_M)
#define CONVERT_M_Z                 (-CONVERT_M)

#define CONVERT_O                   (1.0f/64.0f)
#define CONVERT_O_A                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (-CONVERT_O)

#define SENSOR_STATE_MASK           (0x7FFF)

static int open_inputs(int mode, int *akm_fd)
{
    /* scan all input drivers and look for "compass" */
    int fd = -1;
    const char *dirname = "/dev/input";
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    *akm_fd = -1;
    
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        fd = open(devname, mode);
        if (fd>=0) {
            char name[80];
            if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
                name[0] = '\0';
            }
            if (!strcmp(name, "compass")) {
                LOGV("using %s (name=%s)", devname, name);
                *akm_fd = fd;
            }
            else
                close(fd);
        }
    }
    closedir(dir);

    fd = 0;
    if (*akm_fd < 0) {
        LOGE("Couldn't find or open 'compass' driver (%s)", strerror(errno));
        fd = -1;
    }
    return fd;
}

static int open_akm(struct sensors_control_context_t* dev)
{
    if (dev->akmd_fd <= 0) {
        dev->akmd_fd = open(AKM_DEVICE_NAME, O_RDONLY);
        //LOGD("%s, fd=%d", __PRETTY_FUNCTION__, dev->akmd_fd);
        LOGE_IF(dev->akmd_fd<0, "Couldn't open %s (%s)",
                AKM_DEVICE_NAME, strerror(errno));
        if (dev->akmd_fd >= 0) {
            dev->active_sensors &= ~SENSORS_GROUP;
        }
    }
    return dev->akmd_fd;
}

static void close_akm(struct sensors_control_context_t* dev)
{
    if (dev->akmd_fd > 0) {
        LOGV("%s, fd=%d", __PRETTY_FUNCTION__, dev->akmd_fd);
        close(dev->akmd_fd);
        dev->akmd_fd = -1;
    }
}

static uint32_t read_sensors_state(int fd)
{
    if (fd<0) return 0;
    short flags;
    uint32_t sensors = 0;
    // read the actual value of all sensors
    if (!ioctl(fd, ECS_IOCTL_APP_GET_MFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_ORIENTATION;
        else        sensors &= ~SENSORS_ORIENTATION;
    }
    if (!ioctl(fd, ECS_IOCTL_APP_GET_AFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_ACCELERATION;
        else        sensors &= ~SENSORS_ACCELERATION;
    }
    if (!ioctl(fd, ECS_IOCTL_APP_GET_TFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_TEMPERATURE;
        else        sensors &= ~SENSORS_TEMPERATURE;
    }
    if (!ioctl(fd, ECS_IOCTL_APP_GET_MVFLAG, &flags)) {
        if (flags)  sensors |= SENSORS_MAGNETIC_FIELD;
        else        sensors &= ~SENSORS_MAGNETIC_FIELD;
    }
    return sensors;
}

static uint32_t enable_disable(struct sensors_control_context_t *dev,
                                   uint32_t active, uint32_t sensors,
                                   uint32_t mask)
{
    uint32_t now_active_akm_sensors;

    int fd = open_akm(dev);
    if (fd < 0)
        return 0;

    LOGV("(before) akm sensors = %08x, real = %08x",
         sensors, read_sensors_state(fd));

    short flags;
    if (mask & SENSORS_ORIENTATION) {
        flags = (sensors & SENSORS_ORIENTATION) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_MFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_MFLAG error (%s)", strerror(errno));
        }
    }
    if (mask & SENSORS_ACCELERATION) {
        flags = (sensors & SENSORS_ACCELERATION) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_AFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_AFLAG error (%s)", strerror(errno));
        }
    }
    if (mask & SENSORS_TEMPERATURE) {
        flags = (sensors & SENSORS_TEMPERATURE) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_TFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_TFLAG error (%s)", strerror(errno));
        }
    }
    if (mask & SENSORS_MAGNETIC_FIELD) {
        flags = (sensors & SENSORS_MAGNETIC_FIELD) ? 1 : 0;
        if (ioctl(fd, ECS_IOCTL_APP_SET_MVFLAG, &flags) < 0) {
            LOGE("ECS_IOCTL_APP_SET_MVFLAG error (%s)", strerror(errno));
        }
    }

    now_active_akm_sensors = read_sensors_state(fd);

    LOGV("(after) akm sensors = %08x, real = %08x",
         sensors, now_active_akm_sensors);

    if (!sensors)
        close_akm(dev);

    return now_active_akm_sensors;
}

/*****************************************************************************/

static native_handle_t* control__open_data_source(struct sensors_control_context_t *dev)
{
    native_handle_t* handle;
    int akm_fd;
    
    if (open_inputs(O_RDONLY, &akm_fd) < 0 ||
        akm_fd < 0) {
        return NULL;
    }

    handle = native_handle_create(1, 0);
    handle->data[0] = akm_fd;
    return handle;
}

static int control__activate(struct sensors_control_context_t *dev,
        int handle, int enabled)
{
    if ((handle < SENSORS_HANDLE_BASE) ||
            (handle >= SENSORS_HANDLE_BASE+MAX_NUM_SENSORS))
        return -1;

    uint32_t mask = (1 << handle);
    uint32_t sensors = enabled ? mask : 0;

    uint32_t active = dev->active_sensors;
    uint32_t new_sensors = (active & ~mask) | (sensors & mask);
    uint32_t changed = active ^ new_sensors;

    if (changed) {
        if (!active && new_sensors)
            // force all sensors to be updated
            changed = SUPPORTED_SENSORS;

        dev->active_sensors =
            enable_disable(dev,
                               active & SENSORS_GROUP,
                               new_sensors & SENSORS_GROUP,
                               changed & SENSORS_GROUP);
    }

    return 0;
}

static int control__set_delay(struct sensors_control_context_t *dev, int32_t ms)
{
#ifdef ECS_IOCTL_APP_SET_DELAY
    if (dev->akmd_fd <= 0) {
        return -1;
    }
    short delay = ms;
    if (!ioctl(dev->akmd_fd, ECS_IOCTL_APP_SET_DELAY, &delay)) {
        return -errno;
    }
    return 0;
#else
    return -1;
#endif
}

static int control__wake(struct sensors_control_context_t *dev)
{
    int err = 0;
    int akm_fd;
    if (open_inputs(O_RDWR, &akm_fd) < 0 ||
            akm_fd < 0) {
        return -1;
    }
    
    struct input_event event[1];
    event[0].type = EV_SYN;
    event[0].code = SYN_CONFIG;
    event[0].value = 0;

    err = write(akm_fd, event, sizeof(event));
    LOGV_IF(err<0, "control__wake(compass), fd=%d (%s)",
            akm_fd, strerror(errno));
    close(akm_fd);

    return err;
}

/*****************************************************************************/

static int data__data_open(struct sensors_data_context_t *dev, native_handle_t* handle)
{
    int i;
    struct input_absinfo absinfo;
    memset(&dev->sensors, 0, sizeof(dev->sensors));

    for (i = 0; i < MAX_NUM_SENSORS; i++) {
        // by default all sensors have high accuracy
        // (we do this because we don't get an update if the value doesn't
        // change).
        dev->sensors[i].vector.status = SENSOR_STATUS_ACCURACY_HIGH;
    }

    dev->sensors[ID_A].sensor = SENSOR_TYPE_ACCELEROMETER;
    dev->sensors[ID_M].sensor = SENSOR_TYPE_MAGNETIC_FIELD;
    dev->sensors[ID_O].sensor = SENSOR_TYPE_ORIENTATION;
    dev->sensors[ID_T].sensor = SENSOR_TYPE_TEMPERATURE;

    dev->events_fd = dup(handle->data[0]);
    LOGV("data__data_open: compass fd = %d", handle->data[0]);
    // Framework will close the handle
    native_handle_delete(handle);

    dev->pendingSensors = 0;
    
    return 0;
}

static int data__data_close(struct sensors_data_context_t *dev)
{
    if (dev->events_fd >= 0) {
        //LOGV("(data close) about to close compass fd=%d", dev->events_fd);
        close(dev->events_fd);
        dev->events_fd = -1;
    }
    return 0;
}

static int pick_sensor(struct sensors_data_context_t *dev,
        sensors_data_t* values)
{
    uint32_t mask = SUPPORTED_SENSORS;
    while (mask) {
        uint32_t i = 31 - __builtin_clz(mask);
        mask &= ~(1<<i);
        if (dev->pendingSensors & (1<<i)) {
            dev->pendingSensors &= ~(1<<i);
            *values = dev->sensors[i];
            values->sensor = (1<<i);
            LOGD_IF(0, "%d [%f, %f, %f]", (1<<i),
                    values->vector.x,
                    values->vector.y,
                    values->vector.z);
            return i;
        }
    }
    LOGE("No sensor to return!!! pendingSensors=%08x", dev->pendingSensors);
    // we may end-up in a busy loop, slow things down, just in case.
    usleep(10000);
    return -1;
}
static uint32_t data__poll_process_akm_abs(struct sensors_data_context_t *dev,
                                           int fd __attribute__((unused)),
                                           struct input_event *event)
{
    uint32_t new_sensors = 0;
    if (event->type == EV_ABS) {
        LOGV("compass type: %d code: %d value: %-5d time: %ds",
             event->type, event->code, event->value,
             (int)event->time.tv_sec);
        switch (event->code) {
        case EVENT_TYPE_ACCEL_X:
            new_sensors |= SENSORS_ACCELERATION;
            // because of physical arrangement of BMA chip
            dev->sensors[ID_A].acceleration.y = event->value * CONVERT_A_X;
            break;
        case EVENT_TYPE_ACCEL_Y:
            new_sensors |= SENSORS_ACCELERATION;
            // because of physical arrangement of BMA chip
            dev->sensors[ID_A].acceleration.x = event->value * CONVERT_A_Y;
            break;
        case EVENT_TYPE_ACCEL_Z:
            new_sensors |= SENSORS_ACCELERATION;
            dev->sensors[ID_A].acceleration.z = event->value * CONVERT_A_Z;
            break;
        case EVENT_TYPE_MAGV_X:
            new_sensors |= SENSORS_MAGNETIC_FIELD;
            dev->sensors[ID_M].magnetic.x = event->value * CONVERT_M_X;
            break;
        case EVENT_TYPE_MAGV_Y:
            new_sensors |= SENSORS_MAGNETIC_FIELD;
            dev->sensors[ID_M].magnetic.y = event->value * CONVERT_M_Y;
            break;
        case EVENT_TYPE_MAGV_Z:
            new_sensors |= SENSORS_MAGNETIC_FIELD;
            dev->sensors[ID_M].magnetic.z = event->value * CONVERT_M_Z;
            break;
        case EVENT_TYPE_YAW:
            new_sensors |= SENSORS_ORIENTATION;
            dev->sensors[ID_O].orientation.azimuth =  event->value * CONVERT_O_A;
            break;
        case EVENT_TYPE_PITCH:
            new_sensors |= SENSORS_ORIENTATION;
            dev->sensors[ID_O].orientation.pitch = event->value * CONVERT_O_P;
            break;
        case EVENT_TYPE_ROLL:
            new_sensors |= SENSORS_ORIENTATION;
            dev->sensors[ID_O].orientation.roll = event->value * CONVERT_O_R;
            break;
        case EVENT_TYPE_TEMPERATURE:
            new_sensors |= SENSORS_TEMPERATURE;
            dev->sensors[ID_T].temperature = event->value;
            break;
        case EVENT_TYPE_STEP_COUNT:
            // step count (only reported in MODE_FFD)
            // we do nothing with it for now.
            break;
        case EVENT_TYPE_ACCEL_STATUS:
            // accuracy of the calibration (never returned!)
            //LOGV("G-Sensor status %d", event->value);
            break;
        case EVENT_TYPE_ORIENT_STATUS: {
            // accuracy of the calibration
            uint32_t v = (uint32_t)(event->value & SENSOR_STATE_MASK);
            LOGV_IF(dev->sensors[ID_O].orientation.status != (uint8_t)v,
                    "M-Sensor status %d", v);
            dev->sensors[ID_O].orientation.status = (uint8_t)v;
        }
            break;
        }
    }

    return new_sensors;
}

static void data__poll_process_syn(struct sensors_data_context_t *dev,
                                   struct input_event *event,
                                   uint32_t new_sensors)
{
    if (new_sensors) {
        dev->pendingSensors |= new_sensors;
        int64_t t = event->time.tv_sec*1000000000LL +
            event->time.tv_usec*1000;
        while (new_sensors) {
            uint32_t i = 31 - __builtin_clz(new_sensors);
            new_sensors &= ~(1<<i);
            dev->sensors[i].time = t;
        }
    }
}

static int data__poll(struct sensors_data_context_t *dev, sensors_data_t* values)
{
    int akm_fd = dev->events_fd;

    if (akm_fd < 0) {
        LOGE("invalid compass file descriptor, fd=%d", akm_fd);
        return -1;
    }

    // there are pending sensors, returns them now...
    if (dev->pendingSensors) {
        LOGV("pending sensors 0x%08x", dev->pendingSensors);
        return pick_sensor(dev, values);
    }

    // wait until we get a complete event for an enabled sensor
    uint32_t new_sensors = 0;
    while (1) {
        /* read the next event; first, read the compass event, then the
           proximity event */
        struct input_event event;
        int got_syn = 0;
        int exit = 0;
        int nread;
        fd_set rfds;
        
        FD_ZERO(&rfds);
        FD_SET(akm_fd, &rfds);

        if (FD_ISSET(akm_fd, &rfds)) {
            nread = read(akm_fd, &event, sizeof(event));
            if (nread == sizeof(event)) {
                new_sensors |= data__poll_process_akm_abs(dev, akm_fd, &event);
                LOGV("akm abs %08x", new_sensors);
                got_syn = event.type == EV_SYN;
                exit = got_syn && event.code == SYN_CONFIG;
                if (got_syn) {
                    LOGV("akm syn %08x", new_sensors);
                    data__poll_process_syn(dev, &event, new_sensors);
                    new_sensors = 0;
                }
            }
            else LOGE("akm read too small %d", nread);
        }
        else LOGV("akm fd is not set");

        if (exit) {
            // we use SYN_CONFIG to signal that we need to exit the
            // main loop.
            //LOGV("got empty message: value=%d", event->value);
            LOGV("exit");
            return 0x7FFFFFFF;
        }

        if (got_syn && dev->pendingSensors) {
            LOGV("got syn, picking sensor");
            return pick_sensor(dev, values);
        }
    }
}



/*****************************************************************************/

static int control__close(struct hw_device_t *dev)
{
    struct sensors_control_context_t* ctx =
        (struct sensors_control_context_t*)dev;
    if (ctx) {
        close_akm(ctx);
        free(ctx);
    }
    return 0;
}

static int data__close(struct hw_device_t *dev)
{
    struct sensors_data_context_t* ctx = (struct sensors_data_context_t*)dev;
    if (ctx) {
        data__data_close(ctx);
        free(ctx);
    }
    return 0;
}


/** Open a new instance of a sensor device using name */
static int open_sensors(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, SENSORS_HARDWARE_CONTROL)) {
        struct sensors_control_context_t *dev;
        dev = malloc(sizeof(*dev));
        memset(dev, 0, sizeof(*dev));
        dev->akmd_fd = -1;
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = module;
        dev->device.common.close = control__close;
        dev->device.open_data_source = control__open_data_source;
        dev->device.activate = control__activate;
        dev->device.set_delay= control__set_delay;
        dev->device.wake = control__wake;
        *device = &dev->device.common;
    } else if (!strcmp(name, SENSORS_HARDWARE_DATA)) {
        struct sensors_data_context_t *dev;
        dev = malloc(sizeof(*dev));
        memset(dev, 0, sizeof(*dev));
        dev->events_fd = -1;
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = module;
        dev->device.common.close = data__close;
        dev->device.data_open = data__data_open;
        dev->device.data_close = data__data_close;
        dev->device.poll = data__poll;
        *device = &dev->device.common;
    }
    return status;
}
