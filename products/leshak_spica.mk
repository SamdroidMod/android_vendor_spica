# Inherit AOSP device configuration for passion.
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_small.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full.mk)

# Inherit some common cyanogenmod stuff.
$(call inherit-product, vendor/leshak/products/common.mk)

#
# Setup device specific product configuration.
#
PRODUCT_NAME := leshak_spica
PRODUCT_BRAND := samsung
PRODUCT_DEVICE := GT-I5700
PRODUCT_MODEL := GT-I5700
PRODUCT_MANUFACTURER := SAMSUNG
PRODUCT_BUILD_PROP_OVERRIDES += BUILD_ID=Froyo BUILD_DISPLAY_ID=Froyo PRODUCT_NAME=spica BUILD_FINGERPRINT=/passion/passion/mahimahi:2.2/FRF91/43546:user/release-keys TARGET_BUILD_TYPE=userdebug BUILD_VERSION_TAGS=release-keys
PRIVATE_BUILD_DESC="spica-user 2.2 FRF91 43546 release-keys"

PRODUCT_SPECIFIC_DEFINES += TARGET_PRELINKER_MAP=$(TOP)/vendor/leshak/prelink-linux-arm-spica.map

#
# Set ro.modversion
#
ifdef CYANOGEN_NIGHTLY
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.modversion=SamdroidMod-2-$(shell date +%m%d%Y)-NIGHTLY-Spica
else
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.modversion=SamdroidMod-2.0.0-Spica-test0
endif

# Time between scans in seconds. Keep it high to minimize battery drain.
# This only affects the case in which there are remembered access points,
# but none are in range.
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.supplicant_scan_interval=15

# density in DPI of the LCD of this board. This is used to scale the UI
# appropriately. If this property is not defined, the default value is 160 dpi. 
PRODUCT_PROPERTY_OVERRIDES += \
    ro.sf.lcd_density=160

PRODUCT_COPY_FILES += \
        device/common/gps/gps.conf_AS_SUPL:system/etc/gps.conf

# Install the features available on this device.
PRODUCT_COPY_FILES += \
    frameworks/base/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/base/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
    frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/base/data/etc/android.hardware.touchscreen.xml:system/etc/permissions/android.hardware.touchscreen.xml 

#
# Copy spica specific prebuilt files
#

#
# Wifi
#
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/spica/wifi/libwlmlogger.so:system/lib/libwlmlogger.so \
    vendor/leshak/prebuilt/spica/wifi/libwlservice.so:system/lib/libwlservice.so \
    vendor/leshak/prebuilt/spica/wifi/nvram.txt:system/etc/nvram.txt \
    vendor/leshak/prebuilt/spica/wifi/rtecdc.bin:system/etc/rtecdc.bin \
    vendor/leshak/prebuilt/spica/wifi/nvram_mfg.txt:system/etc/nvram_mfg.txt \
    vendor/leshak/prebuilt/spica/wifi/rtecdc_mfg.bin:system/etc/rtecdc_mfg.bin \
    vendor/leshak/prebuilt/spica/wifi/bcm_supp.conf:system/etc/bcm_supp.conf \
    vendor/leshak/prebuilt/spica/wifi/wifi.conf:system/etc/wifi.conf \
    vendor/leshak/prebuilt/spica/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    vendor/leshak/prebuilt/spica/wifi/dhcpcd.conf:system/etc/dhcpcd/dhcpcd.conf \
    vendor/leshak/prebuilt/spica/wifi/wlservice:system/bin/wlservice \
    vendor/leshak/prebuilt/spica/wifi/wpa_supplicant:system/bin/wpa_supplicant

#
# Display (2D)
#
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/spica/gralloc-libs/libs3c2drender.so:system/lib/libs3c2drender.so \
    vendor/leshak/prebuilt/spica/gralloc-libs/libsavscmn.so:system/lib/libsavscmn.so \
    vendor/leshak/prebuilt/spica/gralloc-libs/hw/gralloc.GT-I5700.so:system/lib/hw/gralloc.GT-I5700.so

#
# Display (3D)
#
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/spica/fimg-libs/egl.cfg:system/lib/egl/egl.cfg \
    vendor/leshak/prebuilt/spica/fimg-libs/libChunkAlloc.so:system/lib/egl/libChunkAlloc.so \
    vendor/leshak/prebuilt/spica/fimg-libs/libEGL_fimg.so:system/lib/egl/libEGL_fimg.so \
    vendor/leshak/prebuilt/spica/fimg-libs/libGLESv1_CM_fimg.so:system/lib/egl/libGLESv1_CM_fimg.so \
    vendor/leshak/prebuilt/spica/fimg-libs/libGLESv2_fimg.so:system/lib/egl/libGLESv2_fimg.so

#
# Keys
#
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/spica/keys/s3c-keypad-rev0020.kl:system/usr/keylayout/s3c-keypad-rev0020.kl \
    vendor/leshak/prebuilt/spica/keys/sec_headset.kl:system/usr/keylayout/sec_headset.kl \
    vendor/leshak/prebuilt/spica/keys/s3c-keypad-rev0020.kcm.bin:system/usr/keychars/s3c-keypad-rev0020.kcm.bin

#
# Sensors, Lights etc
#
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/spica/hw/copybit.GT-I5700.so:system/lib/hw/copybit.GT-I5700.so \
    vendor/leshak/prebuilt/spica/hw/lights.GT-I5700.so:system/lib/hw/lights.GT-I5700.so \
    vendor/leshak/prebuilt/spica/hw/sensors.GT-I5700.so:system/lib/hw/sensors.GT-I5700.so 

#
# Vold
#
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/spica/vold/vold.fstab:system/etc/vold.fstab

#
# RIL
#
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/spica/ril/drexe:system/bin/drexe \
    vendor/leshak/prebuilt/spica/ril/efsd:system/bin/efsd \
    vendor/leshak/prebuilt/spica/ril/rilclient-test:system/bin/rilclient-test \
    vendor/leshak/prebuilt/spica/ril/libsec-ril.so:system/lib/libsec-ril.so \
    vendor/leshak/prebuilt/spica/ril/libsecril-client.so:system/lib/libsecril-client.so

# GSM APN list
PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/common/etc/apns-conf.xml:system/etc/apns-conf.xml

