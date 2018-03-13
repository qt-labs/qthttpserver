/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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
#include <QtHttpServer/qabstracthttpserver.h>

#include <QtCore/qjsondocument.h>
#include <QtTest/qsignalspy.h>
#include <QtTest/qtest.h>
#include <QtNetwork/qnetworkaccessmanager.h>

#include <functional>

QT_BEGIN_NAMESPACE

class tst_QHttpServerResponder : public QObject
{
    Q_OBJECT

    std::unique_ptr<QNetworkAccessManager> networkAccessManager;

private slots:
    void init() { networkAccessManager.reset(new QNetworkAccessManager); }
    void cleanup() { networkAccessManager.reset(); }

    void defaultStatusCodeNoParameters();
    void defaultStatusCodeByteArray();
    void defaultStatusCodeJson();
    void writeStatusCode_data();
    void writeStatusCode();
    void writeJson();
};

#define qWaitForFinished(REPLY) QVERIFY(QSignalSpy(REPLY, &QNetworkReply::finished).wait())

struct HttpServer : QAbstractHttpServer {
    std::function<void(QHttpServerResponder responder)> handleRequestFunction;
    QUrl url { QStringLiteral("http://localhost:%1").arg(listen()) };

    HttpServer(decltype(handleRequestFunction) function) : handleRequestFunction(function) {}
    bool handleRequest(const QHttpServerRequest &request, QTcpSocket *socket) override;
};

bool HttpServer::handleRequest(const QHttpServerRequest &request, QTcpSocket *socket)
{
    handleRequestFunction(makeResponder(request, socket));
    return true;
}

void tst_QHttpServerResponder::defaultStatusCodeNoParameters()
{
    HttpServer server([](QHttpServerResponder responder) { responder.write(); });
    auto reply = networkAccessManager->get(QNetworkRequest(server.url));
    qWaitForFinished(reply);
    QCOMPARE(reply->error(), QNetworkReply::NoError);
}

void tst_QHttpServerResponder::defaultStatusCodeByteArray()
{
    HttpServer server([](QHttpServerResponder responder) {
        responder.write(QByteArray(), QByteArrayLiteral("application/x-empty"));
    });
    auto reply = networkAccessManager->get(QNetworkRequest(server.url));
    qWaitForFinished(reply);
    QCOMPARE(reply->error(), QNetworkReply::NoError);
}

void tst_QHttpServerResponder::defaultStatusCodeJson()
{
    const auto json = QJsonDocument::fromJson(QByteArrayLiteral("{}"));
    HttpServer server([json](QHttpServerResponder responder) { responder.write(json); });
    auto reply = networkAccessManager->get(QNetworkRequest(server.url));
    qWaitForFinished(reply);
    QCOMPARE(reply->error(), QNetworkReply::NoError);
}

void tst_QHttpServerResponder::writeStatusCode_data()
{
    using StatusCode = QHttpServerResponder::StatusCode;

    QTest::addColumn<QHttpServerResponder::StatusCode>("statusCode");
    QTest::addColumn<QNetworkReply::NetworkError>("networkError");

    QTest::addRow("OK") << StatusCode::Ok << QNetworkReply::NoError;
    QTest::addRow("Content Access Denied") << StatusCode::Forbidden
                                           << QNetworkReply::ContentAccessDenied;
    QTest::addRow("Connection Refused") << StatusCode::NotFound
                                        << QNetworkReply::ContentNotFoundError;
}

void tst_QHttpServerResponder::writeStatusCode()
{
    QFETCH(QHttpServerResponder::StatusCode, statusCode);
    QFETCH(QNetworkReply::NetworkError, networkError);
    HttpServer server([statusCode](QHttpServerResponder responder) {
        responder.write(statusCode);
    });
    auto reply = networkAccessManager->get(QNetworkRequest(server.url));
    qWaitForFinished(reply);
    QCOMPARE(reply->bytesAvailable(), 0);
    QCOMPARE(reply->error(), networkError);
    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader),
             QByteArrayLiteral("application/x-empty"));
    QCOMPARE(reply->header(QNetworkRequest::ServerHeader), QStringLiteral("%1/%2(%3)")
             .arg(QCoreApplication::instance()->applicationName())
             .arg(QCoreApplication::instance()->applicationVersion())
             .arg(QSysInfo::prettyProductName()).toUtf8());
}

void tst_QHttpServerResponder::writeJson()
{
    const auto json = QJsonDocument::fromJson(QByteArrayLiteral(R"JSON({ "key" : "value" })JSON"));
    HttpServer server([json](QHttpServerResponder responder) { responder.write(json); });
    auto reply = networkAccessManager->get(QNetworkRequest(server.url));
    qWaitForFinished(reply);
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader), QByteArrayLiteral("text/json"));
    QCOMPARE(QJsonDocument::fromJson(reply->readAll()), json);
}

QT_END_NAMESPACE

QTEST_MAIN(tst_QHttpServerResponder)

#include "tst_qhttpserverresponder.moc"
