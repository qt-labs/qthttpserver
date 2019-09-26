TARGET = QtSslServer
INCLUDEPATH += .

QT = network network-private core-private

HEADERS += \
    qsslserver.h \
    qtsslserverglobal.h \
    qsslserver_p.h

SOURCES += \
    qsslserver.cpp

load(qt_module)
