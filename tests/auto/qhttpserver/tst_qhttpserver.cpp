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

#include <QtHttpServer/qhttpserver.h>
#include <QtHttpServer/qhttpserverrequest.h>
#include <QtHttpServer/qhttpserverrouterrule.h>

#include <private/qhttpserverrouterrule_p.h>

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>

#include <QtCore/qurl.h>
#include <QtCore/qstring.h>
#include <QtCore/qlist.h>
#include <QtCore/qbytearray.h>

#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtNetwork/qnetworkreply.h>
#include <QtNetwork/qnetworkrequest.h>

QT_BEGIN_NAMESPACE

class QueryRequireRouterRule : public QHttpServerRouterRule
{
public:
    QueryRequireRouterRule(const QString &pathPattern,
                           const char *queryKey,
                           RouterHandler &&routerHandler)
        : QHttpServerRouterRule(pathPattern, std::forward<RouterHandler>(routerHandler)),
          m_queryKey(queryKey)
    {
    }

    bool matches(const QHttpServerRequest &request, QRegularExpressionMatch *match) const override
    {
        if (QHttpServerRouterRule::matches(request, match)) {
            if (request.query().hasQueryItem(m_queryKey))
                return true;
        }

        return false;
    }

private:
    const char * m_queryKey;
};

class tst_QHttpServer final : public QObject
{
    Q_OBJECT
signals:
    void test();

private slots:
    void initTestCase();
    void routeGet_data();
    void routeGet();
    void routePost_data();
    void routePost();

private:
    QHttpServer httpserver;
    QString urlBase;
};

void tst_QHttpServer::initTestCase()
{
    httpserver.route("/test", [] (QHttpServerResponder &&responder) {
        responder.write("test msg", "text/html");
    });

    httpserver.route("/", QHttpServerRequest::Method::Get, [] () {
        return "Hello world get";
    });

    httpserver.route("/", QHttpServerRequest::Method::Post, [] () {
        return "Hello world post";
    });

    httpserver.route("/page/", [] (const qint32 number) {
        return QString("page: %1").arg(number);
    });

    httpserver.route("/page/<arg>/detail", [] (const quint32 number) {
        return QString("page: %1 detail").arg(number);
    });

    httpserver.route("/user/", [] (const QString &name) {
        return QString("%1").arg(name);
    });

    httpserver.route("/user/<arg>/", [] (const QString &name, const QByteArray &ba) {
        return QString("%1-%2").arg(name).arg(QString::fromLatin1(ba));
    });

    httpserver.route("/test/", [] (const QUrl &url) {
        return QString("path: %1").arg(url.path());
    });

    httpserver.route("/api/v", [] (const float api) {
        return QString("api %1v").arg(api);
    });

    httpserver.route("/api/v<arg>/user/", [] (const float api, const quint64 user) {
        return QString("api %1v, user id - %2").arg(api).arg(user);
    });

    httpserver.route("/api/v<arg>/user/<arg>/settings", [] (const float api, const quint64 user,
                                                             const QHttpServerRequest &request) {
        const auto &role = request.query().queryItemValue(QString::fromLatin1("role"));
        const auto &fragment = request.url().fragment();

        return QString("api %1v, user id - %2, set settings role=%3#'%4'")
                   .arg(api).arg(user).arg(role, fragment);
    });

    httpserver.route<QueryRequireRouterRule>(
            "/custom/",
            "key",
            [] (const quint64 num, const QHttpServerRequest &request) {
        return QString("Custom router rule: %1, key=%2")
                    .arg(num)
                    .arg(request.query().queryItemValue("key"));
    });

    urlBase = QStringLiteral("http://localhost:%1%2").arg(httpserver.listen());
}

void tst_QHttpServer::routeGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("code");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("body");

    QTest::addRow("hello world")
        << "/"
        << 200
        << "text/html"
        << "Hello world get";

    QTest::addRow("test msg")
        << "/test"
        << 200
        << "text/html"
        << "test msg";

    QTest::addRow("not found")
        << "/not-found"
        << 404
        << "text/html"
        << "";

    QTest::addRow("arg:int")
        << "/page/10"
        << 200
        << "text/html"
        << "page: 10";

    QTest::addRow("arg:-int")
        << "/page/-10"
        << 200
        << "text/html"
        << "page: -10";

    QTest::addRow("arg:uint")
        << "/page/10/detail"
        << 200
        << "text/html"
        << "page: 10 detail";

    QTest::addRow("arg:-uint")
        << "/page/-10/detail"
        << 404
        << "text/html"
        << "";

    QTest::addRow("arg:string")
        << "/user/test"
        << 200
        << "text/html"
        << "test";

    QTest::addRow("arg:string")
        << "/user/test test ,!a+."
        << 200
        << "text/html"
        << "test test ,!a+.";

    QTest::addRow("arg:string,ba")
        << "/user/james/bond"
        << 200
        << "text/html"
        << "james-bond";

    QTest::addRow("arg:url")
        << "/test/api/v0/cmds?val=1"
        << 200
        << "text/html"
        << "path: api/v0/cmds";

    QTest::addRow("arg:float 5.1")
        << "/api/v5.1"
        << 200
        << "text/html"
        << "api 5.1v";

    QTest::addRow("arg:float 5.")
        << "/api/v5."
        << 200
        << "text/html"
        << "api 5v";

    QTest::addRow("arg:float 6.0")
        << "/api/v6.0"
        << 200
        << "text/html"
        << "api 6v";

    QTest::addRow("arg:float,uint")
        << "/api/v5.1/user/10"
        << 200
        << "text/html"
        << "api 5.1v, user id - 10";

    QTest::addRow("arg:float,uint,query")
        << "/api/v5.2/user/11/settings?role=admin" << 200
        << "text/html"
        << "api 5.2v, user id - 11, set settings role=admin#''";

    // The fragment isn't actually sent via HTTP (it's information for the user agent)
    QTest::addRow("arg:float,uint, query+fragment")
        << "/api/v5.2/user/11/settings?role=admin#tag"
        << 200 << "text/html"
        << "api 5.2v, user id - 11, set settings role=admin#''";

    QTest::addRow("custom route rule")
        << "/custom/15"
        << 404
        << "text/html"
        << "";

    QTest::addRow("custom route rule + query")
        << "/custom/10?key=11&g=1"
        << 200
        << "text/html"
        << "Custom router rule: 10, key=11";

    QTest::addRow("custom route rule + query key req")
        << "/custom/10?g=1&key=12"
        << 200
        << "text/html"
        << "Custom router rule: 10, key=12";
}

void tst_QHttpServer::routeGet()
{
    QFETCH(QString, url);
    QFETCH(int, code);
    QFETCH(QString, type);
    QFETCH(QString, body);

    QNetworkAccessManager networkAccessManager;
    const QUrl requestUrl(urlBase.arg(url));
    auto reply = networkAccessManager.get(QNetworkRequest(requestUrl));

    QTRY_VERIFY(reply->isFinished());

    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader), type);
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), code);
    QCOMPARE(reply->readAll(), body);
}

void tst_QHttpServer::routePost_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("code");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("data");
    QTest::addColumn<QString>("body");

    QTest::addRow("hello world")
        << "/"
        << 200
        << "text/html"
        << ""
        << "Hello world post";
}

void tst_QHttpServer::routePost()
{
    QFETCH(QString, url);
    QFETCH(int, code);
    QFETCH(QString, type);
    QFETCH(QString, data);
    QFETCH(QString, body);

    QNetworkAccessManager networkAccessManager;
    const QUrl requestUrl(urlBase.arg(url));
    auto reply = networkAccessManager.post(QNetworkRequest(requestUrl), data.toUtf8());

    QTRY_VERIFY(reply->isFinished());

    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader), type);
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), code);
    QCOMPARE(reply->readAll(), body);
}

QT_END_NAMESPACE

QTEST_MAIN(tst_QHttpServer)

#include "tst_qhttpserver.moc"
