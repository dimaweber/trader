#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

#include "fcgi_request.h"
#include "authentificator.h"

#include <QUrl>
#include <QUrlQuery>


#define API_PATH "/api/3/"
#define TAPI_PATH "/tapi"


class QueryParser
{
    QueryParser(const QString& scheme, const QString& addr, const QString& port, const QString& path, const QString& query,
                const QMap<QString, QString>& headers = QMap<QString,QString>(), const QByteArray& postData = QByteArray())
    {
        QUrl u;
        u.setScheme(scheme);
        u.setHost(addr);
        u.setPort(port.toInt());
        u.setPath(path);
        u.setQuery(query);

        setUrl(std::move(u), headers, postData);
    }

    void setUrl(const QUrl& u, const QMap<QString, QString>& headers = QMap<QString,QString>(), const QByteArray& postData = QByteArray())
    {
        url = u;

        QString p = url.path();

        if (p.startsWith(API_PATH))
        {
            scope = Scope::Public;
            p = p.remove(API_PATH);
            splittedPath = p.split('/');
        }
        else if(p.startsWith(TAPI_PATH))
        {
            scope = Scope::Private;
            splittedPath.clear();
        }
        else
            std::cerr << "bad url path" << std::endl;


        this->headers = headers;

        rawPostData = postData;
        QString str = QString::fromUtf8(postData);
        postParams.setQuery(str);
    }
public:
    enum Scope {Public, Private};

    QueryParser(const FCGI_Request& request)
        :QueryParser(request.getParam("REQUEST_SCHEME"),
                     request.getParam("SERVER_ADDR"),
                     request.getParam("SERVER_PORT"),
                     request.getParam("DOCUMENT_URI"),
                     request.getParam("QUERY_STRING"),
                     request.authHeaders(),
                     request.postData())
    {
    }

    QString toString() const
    {
        return url.toString();
    }
    QString method() const
    {
        QString ret;
        if (apiScope() == Scope::Public)
        {
            if (splittedPath.length() > 0)
                ret = splittedPath[0];
        }
        else
        {
            ret = postParams.queryItemValue("method");
        }
        return ret;
    }
    QStringList pairs() const
    {
        QStringList ret;
        if (splittedPath.length() > 1)
            ret = splittedPath[1].split('-');
        return ret;
    }
    bool ignoreInvalid() const
    {
        QUrlQuery q(url);
        if (q.hasQueryItem("ignore_invalid") && q.queryItemValue("ignore_invalid").toInt() == 1)
            return true;
        return false;
    }
    int limit() const
    {
        QUrlQuery q(url);
        if (!q.hasQueryItem("limit"))
            return 150;
        bool ok;
        int l = q.queryItemValue("limit").toInt(&ok);
        if (!ok)
            return 150;
        if (l>5000)
            return 5000;
        return l;
    }
    Scope apiScope() const
    {
        return scope;
    }
    QString key() const
    {
        return headers["Key"];
    }
    QByteArray sign() const
    {
        return headers["Sign"].toUtf8();
    }
    QString nonce() const
    {
        return postParams.queryItemValue("nonce");
    }
    QByteArray signedData() const
    {
        return rawPostData;
    }
    QString order_id() const
    {
        return postParams.queryItemValue("order_id");
    }
private:
    QUrl url;
    QStringList splittedPath;
    Scope scope;
    QMap<QString, QString> headers;
    QUrlQuery postParams;
    QByteArray rawPostData;
};
#endif // QUERY_PARSER_H
