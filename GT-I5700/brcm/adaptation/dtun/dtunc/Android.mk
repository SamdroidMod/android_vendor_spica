
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	dtun_clnt.c

LOCAL_SHARED_LIBRARIES := \
	libhcid \
	libbluetooth \
	libdbus \
	libexpat \
	libcutils

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, bluez-utils)/eglib \
	$(call include-path-for, bluez-utils)/hcid \
	$(call include-path-for, bluez-utils)/gdbus \
	$(call include-path-for, dbus) \
	. $(LOCAL_PATH)/include $(LOCAL_PATH)/../include 


LOCAL_CFLAGS := -DANDROID_CHANGES -DHAVE_BLUETOOTH

LOCAL_LDLIBS += -lpthread -ldl

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:= dtun

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_EXECUTABLE)

endif
