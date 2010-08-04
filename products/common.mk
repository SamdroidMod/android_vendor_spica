# Generic leshakmod product
PRODUCT_NAME := leshak
PRODUCT_BRAND := leshak
PRODUCT_DEVICE := generic

# Use edify for otapackage
PRODUCT_SPECIFIC_DEFINES += TARGET_OTA_SCRIPT_MODE=amend

# Used by BusyBox
KERNEL_MODULES_DIR:=/lib/modules

# Tiny toolbox
TINY_TOOLBOX:=true

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

# Bring in some audio files
include frameworks/base/data/sounds/AudioPackage4.mk

PRODUCT_COPY_FILES += \
    vendor/leshak/prebuilt/common/bin/backuptool.sh:system/bin/backuptool.sh \
    vendor/leshak/prebuilt/common/lib/libncurses.so:system/lib/libncurses.so \
    vendor/leshak/prebuilt/common/etc/resolv.conf:system/etc/resolv.conf \
    vendor/leshak/prebuilt/common/etc/terminfo/l/linux:system/etc/terminfo/l/linux \
    vendor/leshak/prebuilt/common/etc/terminfo/u/unknown:system/etc/terminfo/u/unknown \
    vendor/leshak/prebuilt/common/etc/profile:system/etc/profile \
    vendor/leshak/prebuilt/common/xbin/bash:system/xbin/bash \
    vendor/leshak/prebuilt/common/xbin/htop:system/xbin/htop \
    vendor/leshak/prebuilt/common/xbin/irssi:system/xbin/irssi \
    vendor/leshak/prebuilt/common/xbin/lsof:system/xbin/lsof \
    vendor/leshak/prebuilt/common/xbin/nano:system/xbin/nano \
    vendor/leshak/prebuilt/common/xbin/powertop:system/xbin/powertop \
    vendor/leshak/prebuilt/common/xbin/openvpn-up.sh:system/xbin/openvpn-up.sh

#SPICA_WITH_GOOGLE:=true

ifdef SPICA_WITH_GOOGLE

    PRODUCT_COPY_FILES += \
        vendor/leshak/proprietary/CarHomeGoogle.apk:./system/app/CarHomeGoogle.apk \
        vendor/leshak/proprietary/CarHomeLauncher.apk:./system/app/CarHomeLauncher.apk \
        vendor/leshak/proprietary/com.amazon.mp3.apk:./system/app/com.amazon.mp3.apk \
        vendor/leshak/proprietary/Maps.apk:./system/app/Maps.apk \
        vendor/leshak/proprietary/Facebook.apk:./system/app/Facebook.apk \
        vendor/leshak/proprietary/GenieWidget.apk:./system/app/GenieWidget.apk \
        vendor/leshak/proprietary/Gmail.apk:./system/app/Gmail.apk \
        vendor/leshak/proprietary/GoogleBackupTransport.apk:./system/app/GoogleBackupTransport.apk \
        vendor/leshak/proprietary/GoogleCalendarSyncAdapter.apk:./system/app/GoogleCalendarSyncAdapter.apk \
        vendor/leshak/proprietary/GoogleContactsSyncAdapter.apk:./system/app/GoogleContactsSyncAdapter.apk \
        vendor/leshak/proprietary/GoogleFeedback.apk:./system/app/GoogleFeedback.apk \
        vendor/leshak/proprietary/GooglePartnerSetup.apk:./system/app/GooglePartnerSetup.apk \
        vendor/leshak/proprietary/GoogleQuickSearchBox.apk:./system/app/GoogleQuickSearchBox.apk \
        vendor/leshak/proprietary/GoogleServicesFramework.apk:./system/app/GoogleServicesFramework.apk \
        vendor/leshak/proprietary/googlevoice.apk:./system/app/googlevoice.apk \
        vendor/leshak/proprietary/HtcCopyright.apk:./system/app/HtcCopyright.apk \
        vendor/leshak/proprietary/HtcEmailPolicy.apk:./system/app/HtcEmailPolicy.apk \
        vendor/leshak/proprietary/HtcSettings.apk:./system/app/HtcSettings.apk \
        vendor/leshak/proprietary/kickback.apk:./system/app/kickback.apk \
        vendor/leshak/proprietary/LatinImeGoogle.apk:./system/app/LatinImeGoogle.apk \
        vendor/leshak/proprietary/LatinImeTutorial.apk:./system/app/LatinImeTutorial.apk \
        vendor/leshak/proprietary/MarketUpdater.apk:./system/app/MarketUpdater.apk \
        vendor/leshak/proprietary/MediaUploader.apk:./system/app/MediaUploader.apk \
        vendor/leshak/proprietary/NetworkLocation.apk:./system/app/NetworkLocation.apk \
        vendor/leshak/proprietary/OneTimeInitializer.apk:./system/app/OneTimeInitializer.apk \
        vendor/leshak/proprietary/PassionQuickOffice.apk:./system/app/PassionQuickOffice.apk \
        vendor/leshak/proprietary/SetupWizard.apk:./system/app/SetupWizard.apk \
        vendor/leshak/proprietary/soundback.apk:./system/app/soundback.apk \
        vendor/leshak/proprietary/Street.apk:./system/app/Street.apk \
        vendor/leshak/proprietary/Talk.apk:./system/app/Talk.apk \
        vendor/leshak/proprietary/talkback.apk:./system/app/talkback.apk \
        vendor/leshak/proprietary/Twitter.apk:./system/app/Twitter.apk \
        vendor/leshak/proprietary/Vending.apk:./system/app/Vending.apk \
        vendor/leshak/proprietary/VoiceSearch.apk:./system/app/VoiceSearch.apk \
        vendor/leshak/proprietary/YouTube.apk:./system/app/YouTube.apk \
        vendor/leshak/proprietary/com.google.android.maps.xml:./system/etc/permissions/com.google.android.maps.xml \
        vendor/leshak/proprietary/com.google.android.maps.jar:./system/framework/com.google.android.maps.jar \
	vendor/leshak/proprietary/features.xml:./system/etc/permissions/features.xml \
        vendor/leshak/proprietary/libinterstitial.so:./system/lib/libinterstitial.so \
        vendor/leshak/proprietary/libspeech.so:./system/lib/libspeech.so
else
    PRODUCT_PACKAGES += \
        Provision \
        GoogleSearch \
        LatinIME
endif

# Copy over the changelog to the device
PRODUCT_COPY_FILES += \
    vendor/leshak/CHANGELOG:system/etc/CHANGELOG.txt

