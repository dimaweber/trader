#ifndef FCGX_REQUEST_H
#define FCGX_REQUEST_H

#include <fcgiapp.h>
#include <fcgio.h>
#include <fcgi_config.h>

#include <QMap>
#include <QString>
#include <QUrl>
#include <QVector>

#include <memory>

extern "C"
{
extern char** environ;
}

class FCGI_Request
{
    FCGX_Request request;
    bool emulatedRequest;

public:
    FCGI_Request(int socket)
        :emulatedRequest(false)
    {
        int ret;
        ret = FCGX_InitRequest(&request, socket, 0);
        if (ret < 0)
            throw std::runtime_error("Fail to initialize request");
    }

    FCGI_Request(const QUrl& url, const QMap<QString, QString>& httpHeaders, QByteArray& in)
        :emulatedRequest(true)
    {
        request.in = new FCGX_Stream;
        request.in->rdNext = (unsigned char*)in.data();
        request.in->stop = (unsigned char*)in.data() + in.length()+1;
        request.in->stopUnget = nullptr;
        request.in->wrNext = request.in->stop;
        request.in->isReader = true;
        request.in->isClosed = false;
        request.in->wasFCloseCalled = false;
        request.in->FCGI_errno = 0;
        request.in->fillBuffProc = nullptr;
        request.in->emptyBuffProc = nullptr;
        request.in->data = in.data();

        request.out = new FCGX_Stream;
        request.out->rdNext = nullptr;
        request.out->stop = nullptr;
        request.out->wrNext = nullptr;
        request.out->stopUnget = nullptr;
        request.out->isReader = false;
        request.out->isClosed = true;
        request.out->wasFCloseCalled = false;
        request.out->FCGI_errno = 0;
        request.out->fillBuffProc = nullptr;
        request.out->emptyBuffProc = nullptr;
        request.out->data = nullptr;

        request.err = new FCGX_Stream;
        request.err->rdNext = nullptr;
        request.err->stop = nullptr;
        request.err->wrNext = nullptr;
        request.err->stopUnget = nullptr;
        request.err->isReader = false;
        request.err->isClosed = true;
        request.err->wasFCloseCalled = false;
        request.err->FCGI_errno = 0;
        request.err->fillBuffProc = nullptr;
        request.err->emptyBuffProc = nullptr;
        request.err->data = nullptr;

        for (const QString& key : httpHeaders.keys())
            qputenv(key.toUtf8(), httpHeaders[key].toUtf8());

        qputenv("REQUEST_SCHEME", url.scheme().toUtf8());
        qputenv("SERVER_ADDR", url.host().toUtf8());
        qputenv("SERVER_PORT", QString::number(url.port()).toUtf8());
        qputenv("DOCUMENT_URI", url.path().toUtf8());
        qputenv("QUERY_STRING", url.query().toUtf8());

        request.envp = environ;
    }

    ~FCGI_Request()
    {
        if (!emulatedRequest)
            FCGX_Free(&request, true);
        else
        {
            delete request.in;
            delete request.out;
            delete request.err;
        }
    }

    int accept()
    {
        int ret = FCGX_Accept_r(&request);
        return ret;
    }
    void finish()
    {
        FCGX_FFlush(request.out);
        return FCGX_Finish_r(&request);
    }
    int put(const char* str)
    {
        int ret = FCGX_PutS(str, request.out);
        if (ret < 0)
            throw std::runtime_error("fail to put data to out socket");
        return ret;
    }
    int put(const char* str, size_t n)
    {
        int ret = FCGX_PutStr(str, n, request.out);
        if (ret < 0)
            throw std::runtime_error("fail to put data to out socket");
        return ret;
    }
    int put(const QString& str)
    {
        return put(str.toUtf8());
    }
    int put (const QByteArray& ba)
    {
        return put(ba.constData(), ba.length());
    }

    QString getParam(const char* paramName) const
    {
        return QString(FCGX_GetParam(paramName, request.envp));
    }

    QString request_uri() const
    {
        return getParam("REQUEST_URI");
    }

    QMap<QString,QString> authHeaders() const
    {
        QMap<QString, QString> map;
        map["Key"] = getParam("KEY");
        map["Sign"] = getParam("SIGN");
        return map;
    }

    QByteArray postData() const
    {
        QString httpMethod = getParam("REQUEST_METHOD");
        if (httpMethod == "POST")
        {
            size_t len = getParam("CONTENT_LENGTH").toUInt();
            QByteArray buffer(len, 0);
            FCGX_GetStr(buffer.data(), len, request.in);
            return buffer;
        }
        return QByteArray();
    }
};
#endif // FCGX_REQUEST_H
