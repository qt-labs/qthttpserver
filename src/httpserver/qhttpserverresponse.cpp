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

#include <QtHttpServer/qhttpserverresponse.h>

#include <private/qhttpserverresponse_p.h>

#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

QT_BEGIN_NAMESPACE

namespace {

const QByteArray mimeTextHtml("text/html");
const QByteArray mimeApplicationJson("application/json");

}

QHttpServerResponse::QHttpServerResponse(QHttpServerResponse &&other)
    : d_ptr(other.d_ptr.take())
{
}

QHttpServerResponse::QHttpServerResponse(const QHttpServerResponse::StatusCode statusCode)
    : QHttpServerResponse(mimeTextHtml, QByteArray(), statusCode)
{
}

QHttpServerResponse::QHttpServerResponse(const char *data)
    : QHttpServerResponse(mimeTextHtml, QByteArray(data))
{
}

QHttpServerResponse::QHttpServerResponse(const QString &data)
    : QHttpServerResponse(mimeTextHtml, data.toUtf8())
{
}

QHttpServerResponse::QHttpServerResponse(const QByteArray &data)
    : QHttpServerResponse(mimeTextHtml, data)
{
}

QHttpServerResponse::QHttpServerResponse(const QJsonObject &data)
    : QHttpServerResponse(mimeApplicationJson, QJsonDocument(data).toJson())
{
}

QHttpServerResponse::QHttpServerResponse(const QByteArray &mimeType,
                                         const QByteArray &data,
                                         const StatusCode status)
    : QHttpServerResponse(new QHttpServerResponsePrivate{mimeType, data, status})
{
}

QHttpServerResponse::~QHttpServerResponse()
{
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
