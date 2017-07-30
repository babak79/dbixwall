TEMPLATE = app

QT += qml quick widgets network websockets

INCLUDEPATH += src
DEPENDPATH += src

linux {
    CONFIG += link_pkgconfig
    PKGCONFIG += protobuf
}

win32 {
    INCLUDEPATH += C:\MinGW\msys\1.0\local\include
    LIBS += C:\MinGW\msys\1.0\local\lib\libprotobuf.a -lsetupapi -lws2_32
    RC_ICONS = src/dbixwall.ico
}

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    INCLUDEPATH += /usr/local/include
    INCLUDEPATH += /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/System/Library/Frameworks/IOKit.framework/Headers
    INCLUDEPATH += /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/System/Library/Frameworks/CoreFoundation.framework/Headers
    QMAKE_LFLAGS += -F/System/Library/Frameworks/CoreFoundation.framework -F/System/Library/Frameworks/IOKit.framework
    LIBS += -framework CoreFoundation
    LIBS += -framework IOKit
    LIBS += /usr/local/lib/libprotobuf.a
    ICON=qml/images/dbixwall.icns
}

SOURCES += src/main.cpp \
    src/accountmodel.cpp \
    src/types.cpp \
    src/dbixipc.cpp \
    src/settings.cpp \
    src/bigint.cpp \
    src/transactionmodel.cpp \
    src/clipboard.cpp \
    src/dbixlog.cpp \
    src/currencymodel.cpp \
    src/accountproxymodel.cpp \
    src/gdbixlog.cpp \
    src/helpers.cpp \
    src/contractmodel.cpp \
    src/contractinfo.cpp \
    src/eventmodel.cpp \
    src/filtermodel.cpp \
    src/dubaicoin/tx.cpp 

RESOURCES += qml/qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH = qml

# Default rules for deployment.
include(src/deployment.pri)

TRANSLATIONS += \
    i18n/dbixwall.ts

lupdate_only {
    SOURCES += \
        qml/*.qml \
        qml/components/*.qml
}

HEADERS += \
    src/accountmodel.h \
    src/types.h \
    src/dbixipc.h \
    src/settings.h \
    src/bigint.h \
    src/transactionmodel.h \
    src/clipboard.h \
    src/dbixlog.h \
    src/currencymodel.h \
    src/accountproxymodel.h \
    src/gdbixlog.h \
    src/helpers.h \
    src/contractmodel.h \
    src/contractinfo.h \
    src/eventmodel.h \
    src/filtermodel.h \
    src/dubaicoin/tx.h \
    src/dubaicoin/keccak.h

