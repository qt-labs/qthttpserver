/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtHttpServer module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtHttpServer/qhttpserverresponder.h>
#include <QtHttpServer/qhttpserverrequest.h>
#include <private/qhttpserverresponder_p.h>
#include <private/qhttpserverrequest_p.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qtcpsocket.h>
#if defined(QT_WEBSOCKETS_LIB)
#include <QtWebSockets/qwebsocket.h>
#include <private/qwebsocket_p.h>
#include <private/qwebsockethandshakeresponse_p.h>
#include <private/qwebsockethandshakerequest_p.h>
#endif

#include <map>
#include <memory>

QT_BEGIN_NAMESPACE

static const QLoggingCategory &lc()
{
    static const QLoggingCategory category("qt.httpserver.response");
    return category;
}

// https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
static const std::map<QHttpServerResponder::StatusCode, QByteArray> statusString {
#define STATUS_CODE(CODE, TEXT) { QHttpServerResponder::StatusCode::CODE, QByteArrayLiteral(TEXT) }
    STATUS_CODE(Continue,                      "Continue"),
    STATUS_CODE(SwitchingProtocols,            "Switching Protocols"),
    STATUS_CODE(Processing,                    "Processing"),
    STATUS_CODE(Ok,                            "OK"),
    STATUS_CODE(Created,                       "Created"),
    STATUS_CODE(Accepted,                      "Accepted"),
    STATUS_CODE(NonAuthoritativeInformation,   "Non-Authoritative Information"),
    STATUS_CODE(NoContent,                     "No Content"),
    STATUS_CODE(ResetContent,                  "Reset Content"),
    STATUS_CODE(PartialContent,                "Partial Content"),
    STATUS_CODE(MultiStatus,                   "Multi Status"),
    STATUS_CODE(AlreadyReported,               "Already Reported"),
    STATUS_CODE(IMUsed,                        "IM Used"),
    STATUS_CODE(MultipleChoices,               "Multiple Choices"),
    STATUS_CODE(MovedPermanently,              "Moved Permanently"),
    STATUS_CODE(Found,                         "Found"),
    STATUS_CODE(SeeOther,                      "See Other"),
    STATUS_CODE(NotModified,                   "Not Modified"),
    STATUS_CODE(UseProxy,                      "Use Proxy"),
    STATUS_CODE(TemporaryRedirect,             "Temporary Redirect"),
    STATUS_CODE(PermanentRedirect,             "Permanent Redirect"),
    STATUS_CODE(BadRequest,                    "Bad Request"),
    STATUS_CODE(Unauthorized,                  "Unauthorized"),
    STATUS_CODE(PaymentRequired,               "Payment Required"),
    STATUS_CODE(Forbidden,                     "Forbidden"),
    STATUS_CODE(NotFound,                      "Not Found"),
    STATUS_CODE(MethodNotAllowed,              "Method Not Allowed"),
    STATUS_CODE(NotAcceptable,                 "Not Acceptable"),
    STATUS_CODE(ProxyAuthenticationRequired,   "Proxy Authentication Required"),
    STATUS_CODE(RequestTimeout,                "Request Time-out"),
    STATUS_CODE(Conflict,                      "Conflict"),
    STATUS_CODE(Gone,                          "Gone"),
    STATUS_CODE(LengthRequired,                "Length Required"),
    STATUS_CODE(PreconditionFailed,            "Precondition Failed"),
    STATUS_CODE(PayloadTooLarge,               "Payload Too Large"),
    STATUS_CODE(UriTooLong,                    "URI Too Long"),
    STATUS_CODE(UnsupportedMediaType,          "Unsupported Media Type"),
    STATUS_CODE(RequestRangeNotSatisfiable,    "Request Range Not Satisfiable"),
    STATUS_CODE(ExpectationFailed,             "Expectation Failed"),
    STATUS_CODE(ImATeapot,                     "I'm A Teapot"),
    STATUS_CODE(MisdirectedRequest,            "Misdirected Request"),
    STATUS_CODE(UnprocessableEntity,           "Unprocessable Entity"),
    STATUS_CODE(Locked,                        "Locked"),
    STATUS_CODE(FailedDependency,              "Failed Dependency"),
    STATUS_CODE(UpgradeRequired,               "Upgrade Required"),
    STATUS_CODE(PreconditionRequired,          "Precondition Required"),
    STATUS_CODE(TooManyRequests,               "Too Many Requests"),
    STATUS_CODE(RequestHeaderFieldsTooLarge,   "Request Header Fields Too Large"),
    STATUS_CODE(UnavailableForLegalReasons,    "Unavailable For Legal Reasons"),
    STATUS_CODE(InternalServerError,           "Internal Server Error"),
    STATUS_CODE(NotImplemented,                "Not Implemented"),
    STATUS_CODE(BadGateway,                    "Bad Gateway"),
    STATUS_CODE(ServiceUnavailable,            "Service Unavailable"),
    STATUS_CODE(GatewayTimeout,                "Gateway Time-out"),
    STATUS_CODE(HttpVersionNotSupported,       "HTTP Version not supported"),
    STATUS_CODE(VariantAlsoNegotiates,         "Variant Also Negotiates"),
    STATUS_CODE(InsufficientStorage,           "Insufficient Storage"),
    STATUS_CODE(LoopDetected,                  "Loop Detected"),
    STATUS_CODE(NotExtended,                   "Not Extended"),
    STATUS_CODE(NetworkAuthenticationRequired, "Network Authentication Required"),
    STATUS_CODE(NetworkConnectTimeoutError,    "Network Connect Timeout Error"),
#undef STATUS_CODE
};

static const QByteArray contentTypeString(QByteArrayLiteral("Content-Type"));
static const QByteArray contentLengthString(QByteArrayLiteral("Content-Length"));

template <qint64 BUFFERSIZE = 512>
struct IOChunkedTransfer
{
    // TODO This is not the fastest implementation, as it does read & write
    // in a sequential fashion, but these operation could potentially overlap.
    // TODO Can we implement it without the buffer? Direct write to the target buffer
    // would be great.

    const qint64 bufferSize = BUFFERSIZE;
    char buffer[BUFFERSIZE];
    qint64 beginIndex = -1;
    qint64 endIndex = -1;
    QScopedPointer<QIODevice, QScopedPointerDeleteLater> source;
    const QPointer<QIODevice> sink;
    const QMetaObject::Connection bytesWrittenConnection;
    const QMetaObject::Connection readyReadConnection;
    IOChunkedTransfer(QIODevice *input, QIODevice *output) :
        source(input),
        sink(output),
        bytesWrittenConnection(QObject::connect(sink, &QIODevice::bytesWritten, [this] () {
              writeToOutput();
        })),
        readyReadConnection(QObject::connect(source.get(), &QIODevice::readyRead, [this] () {
            readFromInput();
        }))
    {
        Q_ASSERT(!source->atEnd());  // TODO error out
        readFromInput();
    }

    ~IOChunkedTransfer()
    {
        QObject::disconnect(bytesWrittenConnection);
        QObject::disconnect(readyReadConnection);
    }

    inline bool isBufferEmpty()
    {
        Q_ASSERT(beginIndex <= endIndex);
        return beginIndex == endIndex;
    }

    void readFromInput()
    {
        if (!isBufferEmpty()) // We haven't consumed all the data yet.
            return;
        beginIndex = 0;
        endIndex = source->read(buffer, bufferSize);
        if (endIndex < 0) {
            endIndex = beginIndex; // Mark the buffer as empty
            qCWarning(lc, "Error reading chunk: %s", qPrintable(source->errorString()));
            return;
        } else if (endIndex) {
            memset(buffer + endIndex, 0, sizeof(buffer) - std::size_t(endIndex));
            writeToOutput();
        }
    }

    void writeToOutput()
    {
        if (isBufferEmpty())
            return;

        const auto writtenBytes = sink->write(buffer + beginIndex, endIndex);
        if (writtenBytes < 0) {
            qCWarning(lc, "Error writing chunk: %s", qPrintable(sink->errorString()));
            return;
        }
        beginIndex += writtenBytes;
        if (isBufferEmpty()) {
            if (source->bytesAvailable())
                QTimer::singleShot(0, source.get(), [this]() { readFromInput(); });
            else if (source->atEnd()) // Finishing
                source.reset();
        }
    }
};

/*!
    Constructs a QHttpServerResponder using the request \a request
    and the socket \a socket.
*/
QHttpServerResponder::QHttpServerResponder(const QHttpServerRequest &request,
                                           QTcpSocket *socket) :
    d_ptr(new QHttpServerResponderPrivate(request, socket))
{
    Q_ASSERT(socket);
}

/*!
    Move-constructs a QHttpServerResponder instance, making it point
    at the same object that \a other was pointing to.
*/
QHttpServerResponder::QHttpServerResponder(QHttpServerResponder &&other) :
    d_ptr(other.d_ptr.take())
{}

/*!
    Destroys a QHttpServerResponder.
*/
QHttpServerResponder::~QHttpServerResponder()
{}

/*!
    Answers a request with an HTTP status code \a status and a
    MIME type \a mimeType. The I/O device \a data provides the body
    of the response. If \a data is sequential, the body of the
    message is sent in chunks: otherwise, the function assumes all
    the content is available and sends it all at once but the read
    is done in chunks.

    \note This function takes the ownership of \a data.
*/
void QHttpServerResponder::write(QIODevice *data,
                                 const QByteArray &mimeType,
                                 StatusCode status)
{
    Q_D(QHttpServerResponder);
    Q_ASSERT(d->socket);
    QScopedPointer<QIODevice, QScopedPointerDeleteLater> input(data);
    auto socket = d->socket;
    QObject::connect(input.get(), &QIODevice::aboutToClose, [&input](){ input.reset(); });
    // TODO protect keep alive sockets
    QObject::connect(input.get(), &QObject::destroyed, socket, &QObject::deleteLater);
    QObject::connect(socket, &QObject::destroyed, [&input](){ input.reset(); });

    input->setParent(nullptr);
    auto openMode = input->openMode();
    if (!(openMode & QIODevice::ReadOnly)) {
        if (openMode == QIODevice::NotOpen) {
            if (!input->open(QIODevice::ReadOnly)) {
                // TODO Add developer error handling
                // TODO Send 500
                qCDebug(lc, "500: Could not open device %s", qPrintable(input->errorString()));
                return;
            }
        } else {
            // TODO Handle that and send 500, the device is opened but not for reading.
            // That doesn't make sense
            qCDebug(lc) << "500: Device is opened in a wrong mode" << openMode
                        << qPrintable(input->errorString());
            return;
        }
    }
    if (!socket->isOpen()) {
        qCWarning(lc, "Cannot write to socket. It's disconnected");
        delete socket;
        return;
    }

    d->writeStatusLine(status);

    if (!input->isSequential()) // Non-sequential QIODevice should know its data size
        d->addHeader(contentLengthString, QByteArray::number(input->size()));

    d->addHeader(contentTypeString, mimeType);

    d->writeHeaders();
    socket->write("\r\n");

    if (input->atEnd()) {
        qCDebug(lc, "No more data available.");
        return;
    }

    auto transfer = new IOChunkedTransfer<>(input.take(), socket);
    QObject::connect(transfer->source.get(), &QObject::destroyed, [transfer]() {
        delete transfer;
    });
}

/*!
    Answers a request with an HTTP status code \a status, a
    MIME type \a mimeType and a body \a data.
*/
void QHttpServerResponder::write(const QByteArray &data,
                                 const QByteArray &mimeType,
                                 StatusCode status)
{
    Q_D(QHttpServerResponder);
    d->writeStatusLine(status);
    addHeaders(contentTypeString, mimeType,
               contentLengthString, QByteArray::number(data.size()));
    d->writeHeaders();
    d->writeBody(data);
}

/*!
    Answers a request with an HTTP status code \a status, and JSON
    document \a document.
*/
void QHttpServerResponder::write(const QJsonDocument &document, StatusCode status)
{
    write(document.toJson(), QByteArrayLiteral("text/json"), status);
}

/*!
    Answers a request with an HTTP status code \a status.
*/
void QHttpServerResponder::write(StatusCode status)
{
    write(QByteArray(), QByteArrayLiteral("application/x-empty"), status);
}

/*!
    Returns the socket used.
*/
QTcpSocket *QHttpServerResponder::socket() const
{
    Q_D(const QHttpServerResponder);
    return d->socket;
}

bool QHttpServerResponder::addHeader(const QByteArray &key, const QByteArray &value)
{
    Q_D(QHttpServerResponder);
    return d->addHeader(key, value);
}

void QHttpServerResponderPrivate::writeStatusLine(StatusCode status,
                                                  const QPair<quint8, quint8> &version) const
{
    Q_ASSERT(socket->isOpen());
    socket->write("HTTP/");
    socket->write(QByteArray::number(version.first));
    socket->write(".");
    socket->write(QByteArray::number(version.second));
    socket->write(" ");
    socket->write(QByteArray::number(quint32(status)));
    socket->write(" ");
    socket->write(statusString.at(status));
    socket->write("\r\n");
}

void QHttpServerResponderPrivate::writeHeader(const QByteArray &header,
                                              const QByteArray &value) const
{
    socket->write(header);
    socket->write(": ");
    socket->write(value);
    socket->write("\r\n");
}

void QHttpServerResponderPrivate::writeHeaders() const
{
    for (const auto &pair : qAsConst(headers()))
        writeHeader(pair.first, pair.second);
}

void QHttpServerResponderPrivate::writeBody(const QByteArray &body) const
{
    Q_ASSERT(socket->isOpen());
    socket->write("\r\n");
    socket->write(body);
}

QT_END_NAMESPACE
