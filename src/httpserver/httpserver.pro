TARGET = QtHttpServer
INCLUDEPATH += .

QT = network core-private

qtHaveModule(websockets): QT += websockets-private

HEADERS += \
    qthttpserverglobal.h \
    qabstracthttpserver.h \
    qabstracthttpserver_p.h \
    qhttpserverhelpers.h \
    qhttpserverrequest.h \
    qhttpserverrequest_p.h \
    qhttpserverresponder.h \
    qhttpserverresponder_p.h \
    qhttpserverrouter.h \
    qhttpserverrouter_p.h \
    qhttpserverrouterrule.h \
    qhttpserverrouterrule_p.h \
    qhttpserverrouterviewtraits.h

SOURCES += \
    qabstracthttpserver.cpp \
    qhttpserverrequest.cpp \
    qhttpserverresponder.cpp \
    qhttpserverrouter.cpp \
    qhttpserverrouterrule.cpp

include(../3rdparty/http-parser.pri)

load(qt_module)
