#ifndef CURL_WRAPPER_H
#define CURL_WRAPPER_H

#include <QString>

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
	CurlHandleWrapper()
	{
		curlHandle = curl_easy_init();
	}

	~CurlHandleWrapper()
	{
		curl_easy_cleanup(curlHandle);
	}
};

class CurlListWrapper
{
	struct curl_slist* slist;

public:
	CurlListWrapper():slist(nullptr){}
	~CurlListWrapper() { curl_slist_free_all(slist);}

	CurlListWrapper& append(const QByteArray& ba)
	{
		slist = curl_slist_append(slist, ba.constData());
		if (!slist)
			throw std::runtime_error("something went wrong");
		else
			return *this;
	}

	CURLcode setHeaders(CURL* curlHandle) const
	{
		return curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, slist);
	}

};

class CurlWrapper
{
	public:
	CurlWrapper()
	{
		CURLcode curlResult;
		curlResult = curl_global_init(CURL_GLOBAL_ALL);
		if (curlResult != CURLE_OK)
		{
			std::cerr << curl_easy_strerror(curlResult) << std::endl;
		}
	}

	~CurlWrapper()
	{
		curl_global_cleanup();
	}
};
#endif // CURL_WRAPPER_H
