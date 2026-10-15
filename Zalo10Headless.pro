# Zalo10Headless — service headless nhận thông báo khi app đã đóng hẳn (xem README.md).
# Đặt cạnh project UI: <workspace>/Zalo10/ (có src/) và <workspace>/Zalo10Headless/ (thư mục này).
APP_NAME = Zalo10Headless

CONFIG += qt warn_on
CONFIG -= cascades10
# QtGui chỉ để dùng QImage trong ZaloService (không tạo QApplication/widget).
QT += network script gui

UI_SRC = ../Zalo10/src

LIBS += -lbbsystem -lbb -lbbplatform -lbbdevice -lbbpim -lunifieddatasourcec
LIBS += -lQtNetwork -lQtScript -lQtGui
LIBS += -lssl -lcrypto -lsqlite3
LIBS += -lsocket   # PhoneSyncCore::httpsGet dùng socket POSIX trực tiếp
# zlib: ZaloService_WebSocket.cpp dùng inflate*/deflate*. App UI có libz qua -lbbcascades,
# service không link Cascades nên phải khai báo tường minh.
LIBS += -lz
LIBS += -L$$PWD/$$UI_SRC/third_party/webp/lib -lwebpdecoder

INCLUDEPATH += src $$UI_SRC $$UI_SRC/third_party/webp/include

# src/third_party/xz (vendored trong Zalo10): biên dịch ở chế độ C mặc định của qcc, không cần -std=gnu99
INCLUDEPATH += $$(QNX_TARGET)/usr/include/qt4/QtGui

HEADERS += \
    src/ServiceController.hpp \
    $$UI_SRC/ServiceHandoff.hpp \
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
