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

#ifndef QHTTPSERVERREQUEST_H
#define QHTTPSERVERREQUEST_H

#include <QtHttpServer/qthttpserverglobal.h>

#include <QtCore/qdebug.h>
#include <QtCore/qglobal.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

class QRegularExpression;
class QString;
class QTcpSocket;

class QHttpServerRequestPrivate;
class Q_HTTPSERVER_EXPORT QHttpServerRequest : public QObjectUserData
{
    friend class QAbstractHttpServerPrivate;
    friend class QHttpServerResponse;

public:
    ~QHttpServerRequest() override;

    enum class Method
    {
        Unknown = -1,

        Get,
        Put,
        Delete,
        Post,
        Head,
        Options,
        Patch
    };

    QString value(const QString &key) const;
    QUrl url() const;
    Method method() const;
    QVariantMap headers() const;
    QByteArray body() const;

protected:
    QHttpServerRequest(const QHttpServerRequest &other);

private:
#if !defined(QT_NO_DEBUG_STREAM)
    friend QDebug operator<<(QDebug debug, const QHttpServerRequest &request);
#endif

    QHttpServerRequest();

    QExplicitlySharedDataPointer<QHttpServerRequestPrivate> d;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QHttpServerRequest::Method)

#endif // QHTTPSERVERREQUEST_H
