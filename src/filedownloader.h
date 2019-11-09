#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#define USE_CURL_DOWNLOADER 0
 
#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <curl/curl.h>

class FileDownloader : public QObject
{
 Q_OBJECT
 public:
  explicit FileDownloader(QUrl imageUrl, QObject *parent = 0);
  virtual ~FileDownloader();
 
 signals:
  void downloaded();
  void gotBytes(const char*, int);
  void progressUpdated(qint64, qint64);
 
 private slots:
  void fileDownloaded();
  void httpReadyRead();
  void error(QNetworkReply::NetworkError);
  void onprogress(qint64, qint64);
 
 private:
  QNetworkAccessManager m_WebCtrl;
  QNetworkReply* reply;
  int downloadWithCurl(std::string downloadUrl, std::string savePath);
  static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);
  static int older_progress(void *p,
                            double dltotal, double dlnow,
                            double ultotal, double ulnow);
  static int xferinfo(void *p,
                      curl_off_t dltotal, curl_off_t dlnow,
                      curl_off_t ultotal, curl_off_t ulnow);
};
 
#endif // FILEDOWNLOADER_H
