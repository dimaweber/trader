#ifndef SMPTAUTHDATA_H
#define SMPTAUTHDATA_H

#include <QString>

class SmtpAuthData
{

public:
    struct Data
    {
        Data();
        QString email;
        QString login;
        QString name;
        QString password;
        QString host;
        QString port;
        bool enableSsl;
    };

private:
    Data data;
    int id;


public:
    SmtpAuthData();
    // set allmost all data in smtpAuth
    void rewriteData(const Data& newData);
    // return smtpAuthData
    const Data& get_SmtpAuth() const { return data; }

    const QString& get_email() const { return data.email; }
    const QString& get_name() const { return data.name; }
    const QString& get_port() const { return data.port; }
    const QString& get_host() const {return data.host;}

    int get_id() const { return id; }
    void set_id(int rowId) { id = rowId; }

    bool isEmpty() const;


};

#endif // SMPTAUTHDATA_H
