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

#include <QtHttpServer/qhttpserverresponder.h>
#include <QtHttpServer/qabstracthttpserver.h>
#include <QtHttpServer/qhttpserverrouter.h>
#include <QtHttpServer/qhttpserverrouterrule.h>

#include <QtTest/qsignalspy.h>
#include <QtTest/qtest.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtNetwork/qtcpsocket.h>

Q_DECLARE_METATYPE(QNetworkAccessManager::Operation);

QT_BEGIN_NAMESPACE

struct HttpServer : QAbstractHttpServer {
    QHttpServerRouter router;

    HttpServer()
        : QAbstractHttpServer()
    {
        connect(this, &QAbstractHttpServer::missingHandler,
                [] (const QHttpServerRequest &request, QTcpSocket *socket) {
            makeResponder(request, socket).write(QHttpServerResponder::StatusCode::NotFound);
        });
    }

    template<typename ViewHandler>
    void route(const char *path, const QHttpServerRequest::Methods methods, ViewHandler &&viewHandler)
    {
        auto rule = new QHttpServerRouterRule(
                path, methods, [this, &viewHandler] (QRegularExpressionMatch &match,
                                                     const QHttpServerRequest &request,
                                                     QTcpSocket *socket) {
            auto boundViewHandler = router.bindCaptured<ViewHandler>(
                    std::forward<ViewHandler>(viewHandler), match);
            boundViewHandler(makeResponder(request, socket));
        });

        router.addRule<ViewHandler>(rule);
    }

    template<typename ViewHandler>
    void route(const char *path, ViewHandler &&viewHandler)
    {
        route(path, QHttpServerRequest::Method::All, std::forward<ViewHandler>(viewHandler));
    }

    bool handleRequest(const QHttpServerRequest &request, QTcpSocket *socket) override {
        return router.handleRequest(request, socket);
    }
};

class tst_QHttpServerRouter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void routerRule_data();
    void routerRule();

private:
    HttpServer httpserver;
    QString urlBase;
};

void tst_QHttpServerRouter::initTestCase()
{
    httpserver.route("/page/", [] (const quint64 &page, QHttpServerResponder &&responder) {
        responder.write(QString("page: %1").arg(page).toUtf8(), "text/plain");
    });

    httpserver.route("/post-only", QHttpServerRequest::Method::Post,
                     [] (QHttpServerResponder &&responder) {
        responder.write(QString("post-test").toUtf8(), "text/plain");
    });

    httpserver.route("/get-only", QHttpServerRequest::Method::Get,
                     [] (QHttpServerResponder &&responder) {
        responder.write(QString("get-test").toUtf8(), "text/plain");
    });

    urlBase = QStringLiteral("http://localhost:%1%2").arg(httpserver.listen());
}

void tst_QHttpServerRouter::routerRule_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("code");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("body");
    QTest::addColumn<QNetworkAccessManager::Operation>("replyType");

    QTest::addRow("/page/1")
        << "/page/1"
        << 200
        << "text/plain"
        << "page: 1"
        << QNetworkAccessManager::GetOperation;

    QTest::addRow("/page/-1")
        << "/page/-1"
        << 404
        << "application/x-empty"
        << ""
        << QNetworkAccessManager::GetOperation;

    QTest::addRow("/post-only [GET]")
        << "/post-only"
        << 404
        << "application/x-empty"
        << ""
        << QNetworkAccessManager::GetOperation;

    QTest::addRow("/post-only [DELETE]")
        << "/post-only"
        << 404
        << "application/x-empty"
        << ""
        << QNetworkAccessManager::DeleteOperation;

    QTest::addRow("/post-only [POST]")
        << "/post-only"
        << 200
        << "text/plain"
        << "post-test"
        << QNetworkAccessManager::PostOperation;

    QTest::addRow("/get-only [GET]")
        << "/get-only"
        << 200
        << "text/plain"
        << "get-test"
        << QNetworkAccessManager::GetOperation;

    QTest::addRow("/get-only [POST]")
        << "/get-only"
        << 404
        << "application/x-empty"
        << ""
        << QNetworkAccessManager::PostOperation;

    QTest::addRow("/get-only [DELETE]")
        << "/get-only"
        << 404
        << "application/x-empty"
        << ""
        << QNetworkAccessManager::DeleteOperation;
}

void tst_QHttpServerRouter::routerRule()
{
    QFETCH(QString, url);
    QFETCH(int, code);
    QFETCH(QString, type);
    QFETCH(QString, body);
    QFETCH(QNetworkAccessManager::Operation, replyType);

    QNetworkAccessManager networkAccessManager;
    QNetworkReply *reply;
    QNetworkRequest request(QUrl(urlBase.arg(url)));

    switch (replyType) {
    case QNetworkAccessManager::GetOperation:
        reply = networkAccessManager.get(request);
        break;
    case QNetworkAccessManager::PostOperation:
        request.setHeader(QNetworkRequest::ContentTypeHeader, type);
        reply = networkAccessManager.post(request, QByteArray("post body"));
        break;
    case QNetworkAccessManager::DeleteOperation:
        reply = networkAccessManager.deleteResource(request);
        break;
    default:
        QFAIL("The replyType does not supported");
    }

    QTRY_VERIFY(reply->isFinished());

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), code);
    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader), type);
    QCOMPARE(reply->readAll(), body);
}

QT_END_NAMESPACE

QTEST_MAIN(tst_QHttpServerRouter)

#include "tst_qhttpserverrouter.moc"
