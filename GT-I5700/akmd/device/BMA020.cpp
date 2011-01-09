#include <fcntl.h>
#include <sys/ioctl.h>
#include <utils/Log.h>

#include "device/BMA020.hpp"
#include "linux-2.6.29/bma020.h"
#include "util.hpp"

namespace akmd {

BMA020::BMA020()
: index(0)
{
    abuf[0] = abuf[1] = Vector();
    unsigned char param=0;
    
    fd = open(BMA020_NAME, O_RDONLY);
    SUCCEED(fd != -1);
    
    param = BMA020_RANGE_2G;
    SUCCEED(ioctl(fd, BMA020_SET_RANGE, &param) == 0);
    
    param = BMA020_BW_50HZ;
    SUCCEED(ioctl(fd, BMA020_SET_BANDWIDTH, &param) == 0);
}

BMA020::~BMA020()
{
    SUCCEED(close(fd) == 0);
}

int BMA020::get_delay()
{
    return -1;
}

void BMA020::calibrate()
{
    const int REFRESH = 10;
    const float ERROR = 0.05f;
    const float GRAVITY_SMOOTH = 0.8f;

    accelerometer_g = accelerometer_g.multiply(GRAVITY_SMOOTH).add(a.multiply(1.0f - GRAVITY_SMOOTH));

    float al = a.length();
    float gl = accelerometer_g.length();

    if (al == 0 || gl == 0) {
        return;
    }

}

void BMA020::measure()
{
    SUCCEED(gettimeofday(&next_update, NULL) == 0);

    bma020acc_t accels;
    
    SUCCEED(ioctl(fd, BMA020_READ_ACCEL_XYZ, &accels) == 0);

    abuf[index] = Vector(-accels.y, accels.x, accels.z);

    index = (index + 1) & 1;

    a = abuf[0].add(abuf[1]).multiply(0.5f * (720.0f / 256.0f));

    calibrate();
}

Vector BMA020::read()
{
    return a;
}

void BMA020::start()
{
    unsigned char bmode = BMA020_MODE_NORMAL;
    SUCCEED(ioctl(fd, BMA020_SET_MODE, &bmode) == 0);
}

void BMA020::stop()
{        
    unsigned char bmode = BMA020_MODE_SLEEP;
    SUCCEED(ioctl(fd, BMA020_SET_MODE, &bmode) == 0);
}

}
