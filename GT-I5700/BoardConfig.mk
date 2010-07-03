# 
# Config for Samsung Spica
#

TARGET_CPU_ABI := armeabi
TARGET_ARCH_VARIANT := armv5te-vfp
TARGET_GLOBAL_CFLAGS += -mtune=arm1176jzf-s -mfpu=vfp
TARGET_GLOBAL_CPPFLAGS += -mtune=arm1176jzf-s -mfpu=vfp

USE_CAMERA_STUB := true
BOARD_CAMERA_LIBRARIES := libcamera
TARGET_BOOTLOADER_BOARD_NAME := GT-I5700
TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := true
TARGET_NO_RADIOIMAGE := true
TARGET_BOARD_PLATFORM := s3c6410
BOARD_USES_GENERIC_AUDIO := true
BOARD_USES_ALSA_AUDIO := false

BOARD_WLAN_DEVICE           := eth0
WIFI_DRIVER_MODULE_PATH     := "/lib/modules/dhd.ko"
WIFI_DRIVER_MODULE_ARG      := "firmware_path=/system/etc/rtecdc.bin nvram_path=/system/etc/nvram.txt"
WIFI_DRIVER_MODULE_NAME     := "dhd"

BOARD_HAVE_BLUETOOTH := false
