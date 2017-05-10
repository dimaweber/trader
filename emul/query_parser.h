#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

#include "fcgi_request.h"

#include <QUrl>
#include <QUrlQuery>


#define API_PATH "/api/3/"

class QueryParser
{
    QUrl url;
    QStringList splittedPath;
public:
    QueryParser(const QString& scheme, const QString& addr, const QString& port, const QString& path, const QString& query)
    {
        url.setScheme(scheme);
        url.setHost(addr);
        url.setPort(port.toInt());
        url.setPath(path);
        url.setQuery(query);

        QString p = url.path();
        if (p.startsWith(API_PATH))
            p = p.remove(API_PATH);
        splittedPath = p.split('/');
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
        url.setUrl(str);
        QString p = url.path();
        if (p.startsWith(API_PATH))
            p = p.remove(API_PATH);
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
};
#endif // QUERY_PARSER_H
