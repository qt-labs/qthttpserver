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

#include <QtHttpServer/qhttpserverresponse.h>

#include <private/qhttpserverresponse_p.h>
#include <private/qhttpserverliterals_p.h>

#include <QtCore/qfile.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qmimedatabase.h>

QT_BEGIN_NAMESPACE

QHttpServerResponse::QHttpServerResponse(QHttpServerResponse &&other)
    : d_ptr(other.d_ptr.take())
{
}

QHttpServerResponse::QHttpServerResponse(
        const QHttpServerResponse::StatusCode statusCode)
    : QHttpServerResponse(QHttpServerLiterals::contentTypeXEmpty(),
                          QByteArray(),
                          statusCode)
{
}

QHttpServerResponse::QHttpServerResponse(const char *data)
    : QHttpServerResponse(QByteArray::fromRawData(data, qstrlen(data)))
{
}

QHttpServerResponse::QHttpServerResponse(const QString &data)
    : QHttpServerResponse(data.toUtf8())
{
}

QHttpServerResponse::QHttpServerResponse(const QByteArray &data)
    : QHttpServerResponse(QMimeDatabase().mimeTypeForData(data).name().toLocal8Bit(), data)
{
}

QHttpServerResponse::QHttpServerResponse(QByteArray &&data)
    : QHttpServerResponse(
            QMimeDatabase().mimeTypeForData(data).name().toLocal8Bit(),
            std::move(data))
{
}

QHttpServerResponse::QHttpServerResponse(const QJsonObject &data)
    : QHttpServerResponse(QHttpServerLiterals::contentTypeJson(),
                          QJsonDocument(data).toJson(QJsonDocument::Compact))
{
}

QHttpServerResponse::QHttpServerResponse(const QJsonArray &data)
    : QHttpServerResponse(QHttpServerLiterals::contentTypeJson(),
                          QJsonDocument(data).toJson(QJsonDocument::Compact))
{
}

QHttpServerResponse::QHttpServerResponse(const QByteArray &mimeType,
                                         const QByteArray &data,
                                         const StatusCode status)
    : QHttpServerResponse(new QHttpServerResponsePrivate{mimeType, data, status})
{
}

QHttpServerResponse::QHttpServerResponse(QByteArray &&mimeType,
                                         const QByteArray &data,
                                         const StatusCode status)
    : QHttpServerResponse(
            new QHttpServerResponsePrivate{std::move(mimeType), data, status})
{
}

QHttpServerResponse::QHttpServerResponse(const QByteArray &mimeType,
                                         QByteArray &&data,
                                         const StatusCode status)
    : QHttpServerResponse(
            new QHttpServerResponsePrivate{mimeType, std::move(data), status})
{
}

QHttpServerResponse::QHttpServerResponse(QByteArray &&mimeType,
                                         QByteArray &&data,
                                         const StatusCode status)
    : QHttpServerResponse(
            new QHttpServerResponsePrivate{std::move(mimeType), std::move(data),
                                           status})
{
}

QHttpServerResponse::~QHttpServerResponse()
{
}

QHttpServerResponse QHttpServerResponse::fromFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return QHttpServerResponse(StatusCode::NotFound);
    const QByteArray data = file.readAll();
    file.close();
    const QByteArray mimeType = QMimeDatabase().mimeTypeForFileNameAndData(fileName, data).name().toLocal8Bit();
    return QHttpServerResponse(mimeType, data);
}

QHttpServerResponse::QHttpServerResponse(QHttpServerResponsePrivate *d)
    : d_ptr(d)
{
}

QByteArray QHttpServerResponse::data() const
{
    Q_D(const QHttpServerResponse);
    return d->data;
}

QByteArray QHttpServerResponse::mimeType() const
{
    Q_D(const QHttpServerResponse);
    return d->mimeType;
}

QHttpServerResponse::StatusCode QHttpServerResponse::statusCode() const
{
    Q_D(const QHttpServerResponse);
    return d->statusCode;
}

QT_END_NAMESPACE
