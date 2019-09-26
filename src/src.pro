TEMPLATE = subdirs

QT = network

SUBDIRS = \
    httpserver

qtConfig(ssl) {
    SUBDIRS += sslserver
    httpserver.depends = sslserver
}
