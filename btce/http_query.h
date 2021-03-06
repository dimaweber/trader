#ifndef HTTP_QUERY_H
#define HTTP_QUERY_H

#include "curl_wrapper.h"
#include <QByteArray>
#include <QVariantMap>


class HttpQuery : public CurlHandleWrapper
{
protected:
    bool valid;
    static size_t writeFunc(char *ptr, size_t size, size_t nmemb, void *userdata);
    QByteArray jsonData;

    virtual void setHeaders(CurlListWrapper& headers);

    virtual QString path() const = 0;
    virtual bool parseSuccess(const QVariantMap& returnMap) =0;
    virtual bool parse(const QByteArray& serverAnswer) =0;

    static QVariantMap convertReplyToMap(const QByteArray& json);
public:
    HttpQuery():valid(false){}
    virtual ~HttpQuery() {}
    bool isValid() const { return valid;}
    bool performQuery();

    friend class CommonTest;
};


#endif // HTTP_QUERY_H
