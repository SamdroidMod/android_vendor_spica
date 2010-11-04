LOCAL_PATH := $(call my-dir)

#
# copy brcm binaries into android
#
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/bin/btld:system/bin/btld \
	$(LOCAL_PATH)/bin/BCM4325D1_004.002.004.0153.0173.hcd:system/bin/BCM4325D1_004.002.004.0153.0173.hcd

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
    include $(all-subdir-makefiles)
endif
