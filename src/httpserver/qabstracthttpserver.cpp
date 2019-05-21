/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtHttpServer module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtHttpServer/qabstracthttpserver.h>

#include <QtHttpServer/qhttpserverrequest.h>
#include <QtHttpServer/qhttpserverresponder.h>
#include <private/qabstracthttpserver_p.h>
#include <private/qhttpserverrequest_p.h>

#include <QtCore/qloggingcategory.h>
#include <QtCore/qmetaobject.h>
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>

#include "http_parser.h"

#include <algorithm>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcHttpServer, "qt.httpserver")

#if !defined(QT_NO_USERDATA)
QAtomicInt QAbstractHttpServerPrivate::userDataId = -1;
#endif

QAbstractHttpServerPrivate::QAbstractHttpServerPrivate()
{
#if !defined(QT_NO_USERDATA)
    userDataId.testAndSetRelaxed(-1, int(QObject::registerUserData()));
#endif
}

void QAbstractHttpServerPrivate::handleNewConnections()
{
    Q_Q(QAbstractHttpServer);
    auto tcpServer = qobject_cast<QTcpServer *>(q->sender());
    Q_ASSERT(tcpServer);
    while (auto *socket = tcpServer->nextPendingConnection()) {
        auto request = new QHttpServerRequest;  // TODO own tcp server could pre-allocate it
        http_parser_init(&request->d->httpParser, HTTP_REQUEST);
        connect(socket, &QTcpSocket::readyRead, this, &QAbstractHttpServerPrivate::handleReadyRead);
        socket->connect(socket, &QTcpSocket::disconnected, &QObject::deleteLater);
#if !defined(QT_NO_USERDATA)
        socket->setUserData(uint(userDataId), request);
#else
        QObject::connect(socket, &QTcpSocket::destroyed, [&]() {
            requests.remove(socket);
        });
        requests.insert(socket, request);
#endif
    }
}

void QAbstractHttpServerPrivate::handleReadyRead()
{
    Q_Q(QAbstractHttpServer);
    auto socket = qobject_cast<QTcpSocket *>(q->sender());
    Q_ASSERT(socket);
#if !defined(QT_NO_USERDATA)
    auto request = static_cast<QHttpServerRequest *>(socket->userData(uint(userDataId)));
#else
    auto request = requests[socket];
#endif
    auto &requestPrivate = request->d;

    if (!socket->isTransactionStarted())
        socket->startTransaction();

    if (requestPrivate->state == QHttpServerRequestPrivate::State::OnMessageComplete)
        requestPrivate->clear();

    if (!requestPrivate->parse(socket)) {
        socket->disconnect();
        return;
    }

    if (!requestPrivate->httpParser.upgrade &&
            requestPrivate->state != QHttpServerRequestPrivate::State::OnMessageComplete)
        return; // Partial read

    if (requestPrivate->httpParser.upgrade) { // Upgrade
        const auto &headers = requestPrivate->headers;
        const auto upgradeHash = requestPrivate->headerHash(QByteArrayLiteral("upgrade"));
        const auto it = headers.find(upgradeHash);
        if (it != headers.end()) {
#if defined(QT_WEBSOCKETS_LIB)
            if (it.value().second.compare(QByteArrayLiteral("websocket"), Qt::CaseInsensitive) == 0) {
                static const auto signal = QMetaMethod::fromSignal(
                            &QAbstractHttpServer::newWebSocketConnection);
                if (q->isSignalConnected(signal)) {
                    disconnect(socket, &QTcpSocket::readyRead,
                               this, &QAbstractHttpServerPrivate::handleReadyRead);
                    socket->rollbackTransaction();
                    websocketServer.handleConnection(socket);
                    Q_EMIT socket->readyRead();
                } else {
                    qWarning(lcHttpServer, "WebSocket received but no slots connected to "
                                           "QWebSocketServer::newConnection");
                    socket->disconnectFromHost();
                }
                return;
            }
#endif
        }
        qCWarning(lcHttpServer, "Upgrade to %s not supported", it.value().second.constData());
        socket->disconnectFromHost();
        return;
    }

    socket->commitTransaction();
    if (!q->handleRequest(*request, socket))
        Q_EMIT q->missingHandler(*request, socket);
}

QAbstractHttpServer::QAbstractHttpServer(QObject *parent)
    : QAbstractHttpServer(*new QAbstractHttpServerPrivate, parent)
{}

QAbstractHttpServer::QAbstractHttpServer(QAbstractHttpServerPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
#if defined(QT_WEBSOCKETS_LIB)
    Q_D(QAbstractHttpServer);
    connect(&d->websocketServer, &QWebSocketServer::newConnection,
            this, &QAbstractHttpServer::newWebSocketConnection);
#endif
}

/*!
    Tries to bind a \c QTcpServer to \a address and \a port.

    Returns the server port upon success, -1 otherwise.
*/
int QAbstractHttpServer::listen(const QHostAddress &address, quint16 port)
{
    auto tcpServer = new QTcpServer(this);
    const auto listening = tcpServer->listen(address, port);
    if (listening) {
        bind(tcpServer);
        return tcpServer->serverPort();
    }

    delete tcpServer;
    return -1;
}

/*!
    Bind the HTTP server to given TCP \a server over which
    the transmission happens. It is possible to call this function
    multiple times with different instances of TCP \a server to
    handle multiple connections and ports, for example both SSL and
    non-encrypted connections.

    After calling this function, every _new_ connection will be
    handled and forwarded by the HTTP server.

    It is the user's responsibility to call QTcpServer::listen() on
    the \a server.

    If the \a server is null, then a new, default-constructed TCP
    server will be constructed, which will be listening on a random
    port and all interfaces.

    The \a server will be parented to this HTTP server.

    \sa QTcpServer, QTcpServer::listen()
*/
void QAbstractHttpServer::bind(QTcpServer *server)
{
    Q_D(QAbstractHttpServer);
    if (!server) {
        server = new QTcpServer(this);
        if (!server->listen()) {
            qCCritical(lcHttpServer, "QTcpServer listen failed (%s)",
                       qPrintable(server->errorString()));
        }
    } else {
        if (!server->isListening())
            qCWarning(lcHttpServer) << "The TCP server" << server << "is not listening.";
        server->setParent(this);
    }
    QObjectPrivate::connect(server, &QTcpServer::newConnection,
                            d, &QAbstractHttpServerPrivate::handleNewConnections,
                            Qt::UniqueConnection);
}

/*!
    Returns list of child TCP servers of this HTTP server.
 */
QVector<QTcpServer *> QAbstractHttpServer::servers() const
{
    return findChildren<QTcpServer *>().toVector();
}

#if defined(QT_WEBSOCKETS_LIB)
/*!
    \fn QAbstractHttpServer::newConnection
    This signal is emitted every time a new WebSocket connection is
    available.

    \sa hasPendingWebSocketConnections(), nextPendingWebSocketConnection()
*/

/*!
    Returns \c true if the server has pending WebSocket connections;
    otherwise returns \c false.

    \sa newWebSocketConnection(), nextPendingWebSocketConnection()
*/
bool QAbstractHttpServer::hasPendingWebSocketConnections() const
{
    Q_D(const QAbstractHttpServer);
    return d->websocketServer.hasPendingConnections();
}

/*!
    Returns the next pending connection as a connected QWebSocket
    object. QAbstractHttpServer does not take ownership of the
    returned QWebSocket object. It is up to the caller to delete the
    object explicitly when it will no longer be used, otherwise a
    memory leak will occur. \c nullptr is returned if this function
    is called when there are no pending connections.

    \note The returned QWebSocket object cannot be used from another
    thread.

    \sa newWebSocketConnection(), hasPendingWebSocketConnections()
*/
QWebSocket *QAbstractHttpServer::nextPendingWebSocketConnection()
{
    Q_D(QAbstractHttpServer);
    return d->websocketServer.nextPendingConnection();
}
#endif

QHttpServerResponder QAbstractHttpServer::makeResponder(const QHttpServerRequest &request,
                                                        QTcpSocket *socket)
{
    return QHttpServerResponder(request, socket);
}

QT_END_NAMESPACE
