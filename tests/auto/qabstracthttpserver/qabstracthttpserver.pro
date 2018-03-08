CONFIG += testcase
TARGET = tst_qabstracthttpserver
SOURCES  += tst_qabstracthttpserver.cpp

requires(qtConfig(private_tests))
QT = core network network-private testlib httpserver

qtHaveModule(websockets): QT += websockets
