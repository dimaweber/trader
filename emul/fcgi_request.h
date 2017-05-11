#ifndef FCGX_REQUEST_H
#define FCGX_REQUEST_H

#include <fcgiapp.h>
#include <fcgio.h>
#include <fcgi_config.h>

#include <QString>

#include <memory>

class FCGI_Request
{
    FCGX_Request request;
    std::unique_ptr<fcgi_streambuf> _stream_in;
    std::unique_ptr<fcgi_streambuf> _stream_out;
    std::unique_ptr<fcgi_streambuf> _stream_err;
public:
    FCGI_Request(int socket)
    {
        int ret;
        ret = FCGX_InitRequest(&request, socket, 0);
        if (ret < 0)
            throw std::runtime_error("Fail to initialize request");
    }
    ~FCGI_Request()
    {
        FCGX_Free(&request, true);
    }

    int accept()
    {
        int ret = FCGX_Accept_r(&request);
        if (ret == 0)
        {
            _stream_in.reset(new fcgi_streambuf(request.in));
            _stream_out.reset(new fcgi_streambuf(request.out));
            _stream_err.reset(new fcgi_streambuf(request.err));
        }
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
    fcgi_streambuf& stream_in()
    {
        return *_stream_in;
    }
    fcgi_streambuf& stream_out()
    {
        return *_stream_out;
    }
    fcgi_streambuf& stream_err()
    {
        return *_stream_err;
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
