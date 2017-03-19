#ifndef CURL_WRAPPER_H
#define CURL_WRAPPER_H

#include <QString>
#include <QList>

#include <curl/curl.h>
#include <stdexcept>
#include <iostream>

class HttpError : public std::runtime_error
{
public : HttpError(const QString& msg): std::runtime_error(msg.toStdString()){}
    HttpError(CURLcode code): HttpError(curl_easy_strerror(code)){}
};

class CurlHandleWrapper
{
protected:
    CURL* curlHandle;
public:
    CurlHandleWrapper();
    ~CurlHandleWrapper();
};

class CurlListWrapper
{
    struct curl_slist* slist;
    QList<QByteArray> dataCopy;
public:
    CurlListWrapper();
    ~CurlListWrapper();

    CurlListWrapper& append(const QByteArray& ba);
    CURLcode setHeaders(CURL* curlHandle);
};

class CurlWrapper
{
public:
    CurlWrapper();
    ~CurlWrapper();
};
#endif // CURL_WRAPPER_H
