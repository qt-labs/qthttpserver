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

#include "qhttpserverrequest_p.h"

#include <QtHttpServer/qhttpserverrequest.h>

#include <QtCore/qdebug.h>
#include <QtCore/qloggingcategory.h>
#include <QtNetwork/qtcpsocket.h>
#if defined(QT_FEATURE_ssl)
#include <QtNetwork/qsslsocket.h>
#endif

Q_LOGGING_CATEGORY(lc, "qt.httpserver.request")

QT_BEGIN_NAMESPACE

#if !defined(QT_NO_DEBUG_STREAM)
Q_HTTPSERVER_EXPORT QDebug operator<<(QDebug debug, const QHttpServerRequest &request)
{
    const auto oldSetting = debug.autoInsertSpaces();
    debug.nospace() << "QHttpServerRequest(";
    debug << "(Url: " << request.url() << ")";
    debug << "(Headers: " << request.headers() << ")";
    debug << ')';
    debug.setAutoInsertSpaces(oldSetting);
    return debug.maybeSpace();
}

QDebug operator<<(QDebug debug, const http_parser *const httpParser)
{
    const auto oldSetting = debug.autoInsertSpaces();
    debug.nospace() << "http_parser(" << static_cast<const void *>(httpParser) << ": ";
    debug << "HTTP " << httpParser->http_major << "." << httpParser->http_minor << " "
          << http_method_str(http_method(httpParser->method)) << ')';
    debug.setAutoInsertSpaces(oldSetting);
    return debug.maybeSpace();
}
#endif

http_parser_settings QHttpServerRequestPrivate::httpParserSettings {
    &QHttpServerRequestPrivate::onMessageBegin,
    &QHttpServerRequestPrivate::onUrl,
    &QHttpServerRequestPrivate::onStatus,
    &QHttpServerRequestPrivate::onHeaderField,
    &QHttpServerRequestPrivate::onHeaderValue,
    &QHttpServerRequestPrivate::onHeadersComplete,
    &QHttpServerRequestPrivate::onBody,
    &QHttpServerRequestPrivate::onMessageComplete,
    &QHttpServerRequestPrivate::onChunkHeader,
    &QHttpServerRequestPrivate::onChunkComplete
};

QHttpServerRequestPrivate::QHttpServerRequestPrivate()
{
    httpParser.data = this;
}

QString QHttpServerRequestPrivate::header(const QString &key) const
{
    return headers.value(headerHash(key)).second;
}

bool QHttpServerRequestPrivate::parse(QIODevice *socket)
{
    const auto fragment = socket->readAll();
    if (fragment.size()) {
#if defined(QT_FEATURE_ssl)
        auto sslSocket = qobject_cast<QSslSocket *>(socket);
        url.setScheme(sslSocket && sslSocket->isEncrypted() ? QStringLiteral("https")
                                                            : QStringLiteral("http"));
#else
        url.setScheme(QStringLiteral("http"));
#endif
        const auto parsed = http_parser_execute(&httpParser,
                                                &httpParserSettings,
                                                fragment.constData(),
                                                size_t(fragment.size()));
        if (int(parsed) < fragment.size()) {
            qCDebug(lc, "Parse error: %d", httpParser.http_errno);
            return false;
        }
    }
    return true;
}

uint QHttpServerRequestPrivate::headerHash(const QString &key) const
{
    return qHash(key.toLower(), headersSeed);
}

bool QHttpServerRequestPrivate::parseUrl(const char *at, size_t length, bool connect, QUrl *url)
{
    static const std::map<std::size_t, std::function<void(const QString &, QUrl *)>> functions {
        { UF_SCHEMA,    [](const QString &string, QUrl *url) { url->setScheme(string); } },
        { UF_HOST,      [](const QString &string, QUrl *url) { url->setHost(string); } },
        { UF_PORT,      [](const QString &string, QUrl *url) { url->setPort(string.toInt()); } },
        { UF_PATH,
          [](const QString &string, QUrl *url) { url->setPath(string, QUrl::TolerantMode); } },
        { UF_QUERY,     [](const QString &string, QUrl *url) { url->setQuery(string); } },
        { UF_FRAGMENT,  [](const QString &string, QUrl *url) { url->setFragment(string); } },
        { UF_USERINFO,  [](const QString &string, QUrl *url) { url->setUserInfo(string); } },
    };
    struct http_parser_url u;
    if (http_parser_parse_url(at, length, connect ? 1 : 0, &u) == 0) {
        for (auto i = 0u; i < UF_MAX; i++) {
            if (u.field_set & (1 << i)) {
                functions.find(i)->second(QString::fromUtf8(at + u.field_data[i].off,
                                                            u.field_data[i].len),
                                          url);
            }
        }
        return true;
    }
    return false;
}

QHttpServerRequestPrivate *QHttpServerRequestPrivate::instance(http_parser *httpParser)
{
    return static_cast<QHttpServerRequestPrivate *>(httpParser->data);
}

int QHttpServerRequestPrivate::onMessageBegin(http_parser *httpParser)
{
    qCDebug(lc) << static_cast<void *>(httpParser);
    instance(httpParser)->state = State::OnMessageBegin;
    return 0;
}

int QHttpServerRequestPrivate::onUrl(http_parser *httpParser, const char *at, size_t length)
{
    qCDebug(lc) << httpParser << QString::fromUtf8(at, int(length));
    auto instance = static_cast<QHttpServerRequestPrivate *>(httpParser->data);
    instance->state = State::OnUrl;
    parseUrl(at, length, false, &instance->url);
    return 0;
}

int QHttpServerRequestPrivate::onStatus(http_parser *httpParser, const char *at, size_t length)
{
    qCDebug(lc) << httpParser << QString::fromUtf8(at, int(length));
    instance(httpParser)->state = State::OnStatus;
    return 0;
}

int QHttpServerRequestPrivate::onHeaderField(http_parser *httpParser, const char *at, size_t length)
{
    qCDebug(lc) << httpParser << QString::fromUtf8(at, int(length));
    auto i = instance(httpParser);
    i->state = State::OnHeaders;
    const auto key = QString::fromUtf8(at, int(length));
    i->headers.insert(i->headerHash(key), qMakePair(key, QString()));
    i->lastHeader = key;
    return 0;
}

int QHttpServerRequestPrivate::onHeaderValue(http_parser *httpParser, const char *at, size_t length)
{
    qCDebug(lc) << httpParser << QString::fromUtf8(at, int(length));
    auto i = instance(httpParser);
    i->state = State::OnHeaders;
    Q_ASSERT(!i->lastHeader.isEmpty());
    const auto value = QString::fromUtf8(at, int(length));
    i->headers[i->headerHash(i->lastHeader)] = qMakePair(i->lastHeader, value);
    if (i->lastHeader.compare(QStringLiteral("host"), Qt::CaseInsensitive) == 0)
        parseUrl(at, length, true, &i->url);
#if defined(QT_DEBUG)
    i->lastHeader.clear();
#endif
    return 0;
}

int QHttpServerRequestPrivate::onHeadersComplete(http_parser *httpParser)
{
    qCDebug(lc) << httpParser;
    instance(httpParser)->state = State::OnHeadersComplete;
    return 0;
}

int QHttpServerRequestPrivate::onBody(http_parser *httpParser, const char *at, size_t length)
{
    qCDebug(lc) << httpParser << QString::fromUtf8(at, int(length));
    auto i = instance(httpParser);
    i->state = State::OnBody;
    i->body = QByteArray(at, int(length));
    return 0;
}

int QHttpServerRequestPrivate::onMessageComplete(http_parser *httpParser)
{
    qCDebug(lc) << httpParser;
    instance(httpParser)->state = State::OnMessageComplete;
    return 0;
}

int QHttpServerRequestPrivate::onChunkHeader(http_parser *httpParser)
{
    qCDebug(lc) << httpParser;
    instance(httpParser)->state = State::OnChunkHeader;
    return 0;
}

int QHttpServerRequestPrivate::onChunkComplete(http_parser *httpParser)
{
    qCDebug(lc) << httpParser;
    instance(httpParser)->state = State::OnChunkComplete;
    return 0;
}

QHttpServerRequest::QHttpServerRequest() :
    QObjectUserData(),
    d(new QHttpServerRequestPrivate)
{}

QHttpServerRequest::QHttpServerRequest(const QHttpServerRequest &other) :
    QObjectUserData(),
    d(other.d)
{}

QHttpServerRequest::~QHttpServerRequest()
{}

QString QHttpServerRequest::value(const QString &key) const
{
    return d->headers.value(d->headerHash(key)).second;
}

QUrl QHttpServerRequest::url() const
{
    return d->url;
}

QHttpServerRequest::Method QHttpServerRequest::method() const
{
    switch (d->httpParser.method) {
    case HTTP_GET:
        return QHttpServerRequest::Method::Get;
    case HTTP_PUT:
        return QHttpServerRequest::Method::Put;
    case HTTP_DELETE:
        return QHttpServerRequest::Method::Delete;
    case HTTP_POST:
        return QHttpServerRequest::Method::Post;
    case HTTP_HEAD:
        return QHttpServerRequest::Method::Head;
    case HTTP_OPTIONS:
        return QHttpServerRequest::Method::Options;
    case HTTP_PATCH:
        return QHttpServerRequest::Method::Patch;
    default:
        return QHttpServerRequest::Method::Unknown;
    }
}

QVariantMap QHttpServerRequest::headers() const
{
    QVariantMap ret;
    for (auto it = d->headers.cbegin(), end = d->headers.cend(); it != end; ++it)
        ret.insert(it.value().first, it.value().second);
    return ret;
}

QByteArray QHttpServerRequest::body() const
{
    return d->body;
}

QT_END_NAMESPACE
