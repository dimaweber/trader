#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

#include "fcgi_request.h"

#include <QUrl>
#include <QUrlQuery>


#define API_PATH "/api/3/"
#define TAPI_PATH "/tapi/"

class QueryParser
{
public:
    enum Scope {Public, Private};
    QueryParser(const QString& scheme, const QString& addr, const QString& port, const QString& path, const QString& query)
    {
        QUrl u;
        u.setScheme(scheme);
        u.setHost(addr);
        u.setPort(port.toInt());
        u.setPath(path);
        u.setQuery(query);

        setUrl(std::move(u));
    }

    QueryParser(const FCGI_Request& request)
        :QueryParser(request.getParam("REQUEST_SCHEME"),
                     request.getParam("SERVER_ADDR"),
                     request.getParam("SERVER_PORT"),
                     request.getParam("DOCUMENT_URI"),
                     request.getParam("QUERY_STRING"))
    {
    }
    QueryParser(const QString& str)
    {
        setUrl(str);
    }

    void setUrl(const QString& str)
    {
        setUrl(QUrl(str));
    }

    void setUrl(const QUrl& u)
    {
        url = u;

        QString p = url.path();
        if (p.startsWith(API_PATH))
        {
            scope = Scope::Public;
            p = p.remove(API_PATH);
        }
        else if(p.startsWith(TAPI_PATH))
        {
            scope = Scope::Private;
            p = p.remove(TAPI_PATH);
        }
        else
            std::cerr << "bad url path" << std::endl;

        splittedPath = p.split('/');
    }

    QString toString() const
    {
        return url.toString();
    }
    QString method() const
    {
        QString ret;
        if (splittedPath.length() > 0)
            ret = splittedPath[0];
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
private:
    QUrl url;
    QStringList splittedPath;
    Scope scope;
};
#endif // QUERY_PARSER_H
