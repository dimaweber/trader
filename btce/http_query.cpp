#include "utils.h"
#include "http_query.h"
#include <QJsonDocument>

size_t HttpQuery::writeFunc(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    QByteArray* array = reinterpret_cast<QByteArray*>(userdata);
    int lenBeforeAppend = array->length();
    array->append(ptr, static_cast<int>(size * nmemb));
    return array->length() - lenBeforeAppend;
}

void HttpQuery::setHeaders(CurlListWrapper& headers)
{
    headers.setHeaders(curlHandle);
}

QVariantMap HttpQuery::convertReplyToMap(const QByteArray& json)
{
    QJsonDocument jsonResponce;
    QJsonParseError error;
    jsonResponce = QJsonDocument::fromJson(json, &error);
    if (jsonResponce.isNull())
    {
        throw BrokenJson(QString("json parse error [offset: %2]: %1").arg(error.errorString()).arg(error.offset));
    }
    if (!jsonResponce.isObject())
        throw BrokenJson("json recieved but it is not object");

    QVariant v = jsonResponce.toVariant();

    if (!v.canConvert<QVariantMap>())
        throw BrokenJson("json could not be converted to map");

    return v.toMap();

}

bool HttpQuery::performQuery()
{
    CURLcode curlResult = CURLE_OK;

    jsonData.clear();
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 20L);

    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &jsonData);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, HttpQuery::writeFunc);

    QByteArray sUrl = path().toUtf8();
    curl_easy_setopt(curlHandle, CURLOPT_URL, sUrl.constData());

    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 20L);

    valid = false;
    int retry_count = 10;
    do {
        CurlListWrapper headers;
        setHeaders(headers);
        curlResult = curl_easy_perform(curlHandle);
        if (curlResult == CURLE_OK)
            break;
        if (curlResult == CURLE_OPERATION_TIMEDOUT)
        {
            std::cerr << "http operation timed out. Retry";
            retry_count--;
        }
    } while (retry_count);

    if (curlResult != CURLE_OK)
        throw HttpError(curl_easy_strerror(curlResult));

    double responce_size =0;
    double request_size =0;
    double dl_speed = 0;
    double ul_speed =0;
    double transfer_time = 0;
    double headers_size =0;

    curl_easy_getinfo(curlHandle, CURLINFO_SIZE_DOWNLOAD, &responce_size);
    curl_easy_getinfo(curlHandle, CURLINFO_HEADER_SIZE, &headers_size);
    curl_easy_getinfo(curlHandle, CURLINFO_SIZE_UPLOAD, &request_size);
    curl_easy_getinfo(curlHandle, CURLINFO_SPEED_DOWNLOAD, &dl_speed);
    curl_easy_getinfo(curlHandle, CURLINFO_SPEED_UPLOAD, &ul_speed);
    curl_easy_getinfo(curlHandle, CURLINFO_TOTAL_TIME, &transfer_time);

    std::clog << "upload " << request_size << " bytes " << " with speed " << ul_speed << " b/sec" << std::endl;
    std::clog << "download " << responce_size << " bytes " << " with speed " << dl_speed << " b/sec" << std::endl;
    std::clog << "transfer time: " << transfer_time << " sec" << std::endl;

    long http_code = 0;
    curl_easy_getinfo (curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200 )
    {
        std::cerr << "Bad HTTP reply code: " << http_code << std::endl;
        throw HttpError(QString("HTTP code: %1").arg(http_code));
        return false;
    }

    return parse(jsonData);
}
