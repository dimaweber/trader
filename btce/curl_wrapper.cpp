#include "curl_wrapper.h"

CurlHandleWrapper::CurlHandleWrapper()
{
    do
    {
        curlHandle = curl_easy_init();
        if (!curlHandle)
            std::cerr << "*** FAIL! Unable to create curl handle.";
    } while (!curlHandle);
}

CurlHandleWrapper::~CurlHandleWrapper()
{
    curl_easy_cleanup(curlHandle);
}

CurlListWrapper::CurlListWrapper():slist(nullptr){}

CurlListWrapper::~CurlListWrapper() { curl_slist_free_all(slist);}

CurlListWrapper&CurlListWrapper::append(const QByteArray& ba)
{
    dataCopy.append(ba);
    return *this;
}

CURLcode CurlListWrapper::setHeaders(CURL* curlHandle)
{
    for(const QByteArray& ba: dataCopy)
    {
        slist = curl_slist_append(slist, ba.constData());
        if (!slist)
            throw std::runtime_error("something went wrong");
    }
    return curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, slist);
}

CurlWrapper::CurlWrapper()
{
    CURLcode curlResult;
    curlResult = curl_global_init(CURL_GLOBAL_ALL);
    if (curlResult != CURLE_OK)
    {
        std::cerr << curl_easy_strerror(curlResult) << std::endl;
    }
}

CurlWrapper::~CurlWrapper()
{
    curl_global_cleanup();
}

static CurlWrapper w;
