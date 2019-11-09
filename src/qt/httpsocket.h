#ifndef HTTPSOCKET_H
#define HTTPSOCKET_H

#include <QObject>
#include <QDebug>

#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <iostream>

class httpsocket : public QObject
{
    Q_OBJECT
public:
    explicit httpsocket(QObject *parent = 0);
    QString response;
    QString error;
    QString proxyAddress;
    QMap<QString, QString>  headers;
    int proxyPort;
    bool useProxy;
signals:
    void finished();
public slots:
    void getUrl(QString url);
private:
    static size_t data_write(void* buf, size_t size, size_t nmemb, void* userp);
    CURLcode curl_read(const std::string& url, std::ostream& os, long timeout);
};

#endif // HTTPSOCKET_H
