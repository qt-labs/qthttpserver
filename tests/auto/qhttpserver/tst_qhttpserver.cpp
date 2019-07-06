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
#include <QtCore/qdatetime.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qjsonarray.h>

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

private slots:
    void initTestCase();
    void routeGet_data();
    void routeGet();
    void routeKeepAlive();
    void routePost_data();
    void routePost();
    void routeDelete_data();
    void routeDelete();
    void invalidRouterArguments();
    void checkRouteLambdaCapture();

private:
    void checkReply(QNetworkReply *reply, const QString &response);

private:
    QHttpServer httpserver;
    QString urlBase;
};

struct CustomArg {
    int data = 10;

    CustomArg() {} ;
    CustomArg(const QString &urlArg) : data(urlArg.toInt()) {}
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

    httpserver.route("/post-and-get", "GET|POST", [] (const QHttpServerRequest &request) {
        if (request.method() == QHttpServerRequest::Method::Get)
            return "Hello world get";
        else if (request.method() == QHttpServerRequest::Method::Post)
            return "Hello world post";

        return "This should not work";
    });

    httpserver.route("/any", "All", [] (const QHttpServerRequest &request) {
        static const int index = QHttpServerRequest::staticMetaObject.indexOfEnumerator("Method");
        if (index == -1)
            return "Error: Could not find enum Method";

        static const QMetaEnum en = QHttpServerRequest::staticMetaObject.enumerator(index);
        return en.valueToKey(static_cast<int>(request.method()));
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

    httpserver.router()->addConverter<CustomArg>(QLatin1String("[+-]?\\d+"));
    httpserver.route("/check-custom-type/", [] (const CustomArg &customArg) {
        return QString("data = %1").arg(customArg.data);
    });

    httpserver.route("/post-body", "POST", [] (const QHttpServerRequest &request) {
        return request.body();
    });

    httpserver.route("/file/", [] (const QString &file) {
        return QHttpServerResponse::fromFile(QFINDTESTDATA(QLatin1String("data/") + file));
    });

    httpserver.route("/json-object/", [] () {
        return QJsonObject{
            {"property", "test"},
            {"value", 1}
        };
    });

    httpserver.route("/json-array/", [] () {
        return QJsonArray{
            1, "2",
            QJsonObject{
                {"name", "test"}
            }
        };
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
        << "text/plain"
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
        << "text/plain"
        << "page: 10";

    QTest::addRow("arg:-int")
        << "/page/-10"
        << 200
        << "text/plain"
        << "page: -10";

    QTest::addRow("arg:uint")
        << "/page/10/detail"
        << 200
        << "text/plain"
        << "page: 10 detail";

    QTest::addRow("arg:-uint")
        << "/page/-10/detail"
        << 404
        << "text/html"
        << "";

    QTest::addRow("arg:string")
        << "/user/test"
        << 200
        << "text/plain"
        << "test";

    QTest::addRow("arg:string")
        << "/user/test test ,!a+."
        << 200
        << "text/plain"
        << "test test ,!a+.";

    QTest::addRow("arg:string,ba")
        << "/user/james/bond"
        << 200
        << "text/plain"
        << "james-bond";

    QTest::addRow("arg:url")
        << "/test/api/v0/cmds?val=1"
        << 200
        << "text/plain"
        << "path: api/v0/cmds";

    QTest::addRow("arg:float 5.1")
        << "/api/v5.1"
        << 200
        << "text/plain"
        << "api 5.1v";

    QTest::addRow("arg:float 5.")
        << "/api/v5."
        << 200
        << "text/plain"
        << "api 5v";

    QTest::addRow("arg:float 6.0")
        << "/api/v6.0"
        << 200
        << "text/plain"
        << "api 6v";

    QTest::addRow("arg:float,uint")
        << "/api/v5.1/user/10"
        << 200
        << "text/plain"
        << "api 5.1v, user id - 10";

    QTest::addRow("arg:float,uint,query")
        << "/api/v5.2/user/11/settings?role=admin" << 200
        << "text/plain"
        << "api 5.2v, user id - 11, set settings role=admin#''";

    // The fragment isn't actually sent via HTTP (it's information for the user agent)
    QTest::addRow("arg:float,uint, query+fragment")
        << "/api/v5.2/user/11/settings?role=admin#tag"
        << 200 << "text/plain"
        << "api 5.2v, user id - 11, set settings role=admin#''";

    QTest::addRow("custom route rule")
        << "/custom/15"
        << 404
        << "text/html"
        << "";

    QTest::addRow("custom route rule + query")
        << "/custom/10?key=11&g=1"
        << 200
        << "text/plain"
        << "Custom router rule: 10, key=11";

    QTest::addRow("custom route rule + query key req")
        << "/custom/10?g=1&key=12"
        << 200
        << "text/plain"
        << "Custom router rule: 10, key=12";

    QTest::addRow("post-and-get, get")
        << "/post-and-get"
        << 200
        << "text/plain"
        << "Hello world get";

    QTest::addRow("invalid-rule-method, get")
        << "/invalid-rule-method"
        << 404
        << "text/html"
        << "";

    QTest::addRow("check custom type, data=1")
        << "/check-custom-type/1"
        << 200
        << "text/plain"
        << "data = 1";

    QTest::addRow("any, get")
        << "/any"
        << 200
        << "text/plain"
        << "Get";

    QTest::addRow("response from html file")
        << "/file/text.html"
        << 200
        << "text/html"
        << "<html></html>";

    QTest::addRow("response from json file")
        << "/file/application.json"
        << 200
        << "application/json"
        << "{ \"key\": \"value\" }";

    QTest::addRow("json-object")
        << "/json-object/"
        << 200
        << "application/json"
        << "{\"property\":\"test\",\"value\":1}";

    QTest::addRow("json-array")
        << "/json-array/"
        << 200
        << "application/json"
        << "[1,\"2\",{\"name\":\"test\"}]";
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
    QCOMPARE(reply->readAll().trimmed(), body);
}

void tst_QHttpServer::routeKeepAlive()
{
    httpserver.route("/keep-alive", [] (const QHttpServerRequest &req) -> QHttpServerResponse {
        if (req.headers()["Connection"] != "keep-alive")
            return QHttpServerResponse::StatusCode::NotFound;

        return QString("header: %1, query: %2, body: %3, method: %4")
            .arg(req.value("CustomHeader"),
                 req.url().query(),
                 req.body())
            .arg(static_cast<int>(req.method()));
    });

    QNetworkAccessManager networkAccessManager;
    QNetworkRequest request(urlBase.arg("/keep-alive"));
    request.setRawHeader(QByteArray("Connection"), QByteArray("keep-alive"));

    checkReply(networkAccessManager.get(request),
               QString("header: , query: , body: , method: %1")
                 .arg(static_cast<int>(QHttpServerRequest::Method::Get)));
    if (QTest::currentTestFailed())
        return;

    request.setUrl(urlBase.arg("/keep-alive?po=98"));
    request.setRawHeader("CustomHeader", "1");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/html");

    checkReply(networkAccessManager.post(request, QByteArray("test")),
               QString("header: 1, query: po=98, body: test, method: %1")
                 .arg(static_cast<int>(QHttpServerRequest::Method::Post)));
    if (QTest::currentTestFailed())
        return;

    request = QNetworkRequest(urlBase.arg("/keep-alive"));
    request.setRawHeader(QByteArray("Connection"), QByteArray("keep-alive"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/html");

    checkReply(networkAccessManager.post(request, QByteArray("")),
               QString("header: , query: , body: , method: %1")
                 .arg(static_cast<int>(QHttpServerRequest::Method::Post)));
    if (QTest::currentTestFailed())
        return;

    checkReply(networkAccessManager.get(request),
               QString("header: , query: , body: , method: %1")
                 .arg(static_cast<int>(QHttpServerRequest::Method::Get)));
    if (QTest::currentTestFailed())
        return;
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
        << "text/plain"
        << ""
        << "Hello world post";

    QTest::addRow("post-and-get, post")
        << "/post-and-get"
        << 200
        << "text/plain"
        << ""
        << "Hello world post";

    QTest::addRow("any, post")
        << "/any"
        << 200
        << "text/plain"
        << ""
        << "Post";

    QTest::addRow("post-body")
        << "/post-body"
        << 200
        << "text/plain"
        << "some post data"
        << "some post data";

    QString body;
    for (int i = 0; i < 10000; i++)
        body.append(QString::number(i));

    QTest::addRow("post-body - huge body, chunk test")
        << "/post-body"
        << 200
        << "text/plain"
        << body
        << body;
}

void tst_QHttpServer::routePost()
{
    QFETCH(QString, url);
    QFETCH(int, code);
    QFETCH(QString, type);
    QFETCH(QString, data);
    QFETCH(QString, body);

    QNetworkAccessManager networkAccessManager;
    QNetworkRequest request(QUrl(urlBase.arg(url)));
    if (data.size())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "text/html");

    auto reply = networkAccessManager.post(request, data.toUtf8());

    QTRY_VERIFY(reply->isFinished());

    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader), type);
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), code);
    QCOMPARE(reply->readAll(), body);
}

void tst_QHttpServer::routeDelete_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("code");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("data");

    QTest::addRow("post-and-get, delete")
        << "/post-and-get"
        << 404
        << "text/html"
        << "";

    QTest::addRow("any, delete")
        << "/any"
        << 200
        << "text/plain"
        << "Delete";
}

void tst_QHttpServer::routeDelete()
{
    QFETCH(QString, url);
    QFETCH(int, code);
    QFETCH(QString, type);
    QFETCH(QString, data);

    QNetworkAccessManager networkAccessManager;
    const QUrl requestUrl(urlBase.arg(url));
    auto reply = networkAccessManager.deleteResource(QNetworkRequest(requestUrl));

    QTRY_VERIFY(reply->isFinished());

    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader), type);
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), code);
}

struct CustomType {
    CustomType() {}
    CustomType(const QString &) {}
};

void tst_QHttpServer::invalidRouterArguments()
{
    QCOMPARE(
        httpserver.route("/datetime/", [] (const QDateTime &datetime) {
            return QString("datetime: %1").arg(datetime.toString());
        }),
        false);

    QCOMPARE(
        httpserver.route("/invalid-rule-method", "GeT", [] () {
            return "";
        }),
        false);

    QCOMPARE(
        httpserver.route("/invalid-rule-method", "Garbage", [] () {
            return "";
        }),
        false);

    QCOMPARE(
        httpserver.route("/invalid-rule-method", "Unknown", [] () {
            return "";
        }),
        false);

    QCOMPARE(
        httpserver.route("/implicit-conversion-to-qstring-has-no-registered/",
                         [] (const CustomType &) {
            return "";
        }),
        false);
}

void tst_QHttpServer::checkRouteLambdaCapture()
{
    httpserver.route("/capture-this/", [this] () {
        return urlBase;
    });

    QString msg = urlBase + "/pod";
    httpserver.route("/capture-non-pod-data/", [&msg] () {
        return msg;
    });

    QNetworkAccessManager networkAccessManager;
    checkReply(networkAccessManager.get(QNetworkRequest(QUrl(urlBase.arg("/capture-this/")))),
               urlBase);
    if (QTest::currentTestFailed())
        return;

    checkReply(networkAccessManager.get(
                   QNetworkRequest(QUrl(urlBase.arg("/capture-non-pod-data/")))),
               msg);
    if (QTest::currentTestFailed())
        return;
}

void tst_QHttpServer::checkReply(QNetworkReply *reply, const QString &response) {
    QTRY_VERIFY(reply->isFinished());

    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader), "text/plain");
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    QCOMPARE(reply->readAll(), response);
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(CustomArg);
Q_DECLARE_METATYPE(CustomType);

QTEST_MAIN(tst_QHttpServer)

#include "tst_qhttpserver.moc"
