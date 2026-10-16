# Headless service that receives notifications while the UI app is closed. Expects
# <workspace>/Zalo10/ (with src/) next to this project.
APP_NAME = Zalo10Headless

CONFIG += qt warn_on
CONFIG -= cascades10
# QtGui only for QImage in ZaloService (no QApplication/widgets).
QT += network script gui

UI_SRC = ../Zalo10/src

LIBS += -lbbsystem -lbb -lbbplatform -lbbdevice -lbbpim -lunifieddatasourcec
LIBS += -lQtNetwork -lQtScript -lQtGui
LIBS += -lssl -lcrypto -lsqlite3
LIBS += -lsocket   # POSIX sockets used directly by PhoneSyncCore::httpsGet
# zlib must be explicit: ZaloService_WebSocket.cpp uses inflate*/deflate*, and the service
# doesn't link Cascades (which provides libz for the UI).
LIBS += -lz
LIBS += -L$$PWD/$$UI_SRC/third_party/webp/lib -lwebpdecoder

INCLUDEPATH += src $$UI_SRC $$UI_SRC/third_party/webp/include

# src/third_party/xz is vendored in Zalo10; built in qcc's default C mode, no -std=gnu99.
INCLUDEPATH += $$(QNX_TARGET)/usr/include/qt4/QtGui

HEADERS += \
    src/ServiceController.hpp \
    $$UI_SRC/ServiceHandoff.hpp \
    $$UI_SRC/LogSanitizer.hpp \
    $$UI_SRC/ZaloNetworkManager.hpp \
    $$UI_SRC/PhoneSyncCore.hpp \
    $$UI_SRC/ZaloCookieJar.hpp \
    $$UI_SRC/ZaloService.hpp \
    $$UI_SRC/ZaloServiceUtils.hpp \
    $$UI_SRC/HubIntegration.hpp

SOURCES += \
    src/main.cpp \
    src/ServiceController.cpp \
    src/ContactPickerStub.cpp \
    $$UI_SRC/ZaloService.cpp \
    $$UI_SRC/ZaloService_Auth.cpp \
    $$UI_SRC/ZaloService_WebSocket.cpp \
    $$UI_SRC/ZaloService_Contacts.cpp \
    $$UI_SRC/ZaloService_Messages.cpp \
    $$UI_SRC/ZaloService_Crypto.cpp \
    $$UI_SRC/ZaloService_Network.cpp \
    $$UI_SRC/ZaloService_Db.cpp \
    $$UI_SRC/ZaloService_Sync.cpp \
    $$UI_SRC/ZaloService_PhoneSync.cpp \
    $$UI_SRC/PhoneSyncCore.cpp \
    $$UI_SRC/third_party/xz/xz_crc32.c \
    $$UI_SRC/third_party/xz/xz_crc64.c \
    $$UI_SRC/third_party/xz/xz_dec_stream.c \
    $$UI_SRC/third_party/xz/xz_dec_lzma2.c \
    $$UI_SRC/HubIntegration.cpp
