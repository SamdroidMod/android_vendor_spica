# Generic leshakmod product
PRODUCT_NAME := SamdroidMod
PRODUCT_BRAND := leshak
PRODUCT_DEVICE := generic

# Use edify for otapackage
PRODUCT_SPECIFIC_DEFINES += TARGET_OTA_SCRIPT_MODE=amend

# Used by BusyBox
KERNEL_MODULES_DIR:=/lib/modules

# Overlay
#PRODUCT_PACKAGE_OVERLAYS += vendor/spica/overlay/common

# Tiny toolbox
#TINY_TOOLBOX:=true

# Enable Windows Media if supported by the board
WITH_WINDOWS_MEDIA:=true

PRODUCT_PROPERTY_OVERRIDES += \
    ro.url.legal=http://www.google.com/intl/%s/mobile/android/basic/phone-legal.html \
    ro.url.legal.android_privacy=http://www.google.com/intl/%s/mobile/android/basic/privacy.html \
    ro.com.google.clientidbase=android-google \
    ro.com.android.wifi-watchlist=GoogleGuest \
    ro.setupwizard.enterprise_mode=1 \
    ro.com.android.dateformat=MM-dd-yyyy \
    ro.com.android.dataroaming=false

PRODUCT_PACKAGES += \
    FileManager \
    Superuser \
    ADWLauncher

# Bring in some audio files
include frameworks/base/data/sounds/AudioPackage4.mk

PRODUCT_COPY_FILES += \
    vendor/spica/prebuilt/common/bin/backuptool.sh:system/bin/backuptool.sh \
    vendor/spica/prebuilt/common/lib/libncurses.so:system/lib/libncurses.so \
    vendor/spica/prebuilt/common/etc/resolv.conf:system/etc/resolv.conf \
    vendor/spica/prebuilt/common/etc/terminfo/l/linux:system/etc/terminfo/l/linux \
    vendor/spica/prebuilt/common/etc/terminfo/u/unknown:system/etc/terminfo/u/unknown \
    vendor/spica/prebuilt/common/etc/profile:system/etc/profile \
    vendor/spica/prebuilt/common/xbin/bash:system/xbin/bash \
    vendor/spica/prebuilt/common/xbin/htop:system/xbin/htop \
    vendor/spica/prebuilt/common/xbin/irssi:system/xbin/irssi \
    vendor/spica/prebuilt/common/xbin/lsof:system/xbin/lsof \
    vendor/spica/prebuilt/common/xbin/nano:system/xbin/nano \
    vendor/spica/prebuilt/common/xbin/powertop:system/xbin/powertop \
    vendor/spica/prebuilt/common/xbin/remount:system/xbin/remount \
    vendor/spica/prebuilt/common/xbin/openvpn-up.sh:system/xbin/openvpn-up.sh

#SPICA_WITH_GOOGLE:=true

PRODUCT_PACKAGES += \
        Provision \
        GoogleSearch \
        LatinIME

# Copy over the changelog to the device
PRODUCT_COPY_FILES += \
    vendor/spica/CHANGELOG:system/etc/CHANGELOG.txt
