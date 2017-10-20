#include "filedownloader.h"


/*
#include <fstream>
#include <sstream>
#include <iostream>
*/

#define STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES         6000
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL     3

struct myprogress {
  double lastruntime;
  CURL *curl;
};

/* this is how the CURLOPT_XFERINFOFUNCTION callback works */
int FileDownloader::xferinfo(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
  struct myprogress *myp = (struct myprogress *)p;
  CURL *curl = myp->curl;
  double curtime = 0;

  curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);

  /* under certain circumstances it may be desirable for certain functionality
     to only run every N seconds, in order to do this the transaction time can
     be used */
  if((curtime - myp->lastruntime) >= MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL) {
    myp->lastruntime = curtime;
    fprintf(stderr, "TOTAL TIME: %f \r\n", curtime);
  }

  fprintf(stderr, "UP: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
          "  DOWN: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
          "\r\n",
          ulnow, ultotal, dlnow, dltotal);

  if(dlnow > STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES)
    return 1;
  return 0;
}

/* for libcurl older than 7.32.0 (CURLOPT_PROGRESSFUNCTION) */
int FileDownloader::older_progress(void *p,
                          double dltotal, double dlnow,
                          double ultotal, double ulnow)
{
  return xferinfo(p,
                  (curl_off_t)dltotal,
                  (curl_off_t)dlnow,
                  (curl_off_t)ultotal,
                  (curl_off_t)ulnow);
}

size_t FileDownloader::write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int FileDownloader::downloadWithCurl(std::string downloadUrl, std::string savePath) {
    CURLcode res = CURLE_OK;
    CURL *curl;
    FILE *fp;
    struct myprogress prog;
    char *url = "https://backend.cloakcoin.com/wallet/v2/cloak_ldb.zip";
    curl = curl_easy_init();
    if (curl) {
        prog.lastruntime = 0;
        prog.curl = curl;
        fp = fopen(savePath.c_str(),"wb");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, older_progress);
        /* pass the struct pointer into the progress function */
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &prog);

#if LIBCURL_VERSION_NUM >= 0x072000
        /* xferinfo was introduced in 7.32.0, no earlier libcurl versions will
           compile as they won't have the symbols around.

           If built with a newer libcurl, but running with an older libcurl:
           curl_easy_setopt() will fail in run-time trying to set the new
           callback, making the older callback get used.

           New libcurls will prefer the new callback and instead use that one even
           if both callbacks are set. */

        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
        /* pass the struct pointer into the xferinfo function, note that this is
           an alias to CURLOPT_PROGRESSDATA */
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
#endif

        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        res = curl_easy_perform(curl);
        /* always cleanup */
        curl_easy_cleanup(curl);
        fclose(fp);
        emit downloaded();
    }
    return 0;
}

FileDownloader::FileDownloader(QUrl imageUrl, QObject *parent) : QObject(parent)
{
#if USE_CURL_DOWNLOADER == 1
    downloadWithCurl(imageUrl.toString().toStdString(), "/users/joe/Documents/tst/x.zip");
#else
    QNetworkRequest request(imageUrl);

    QSslConfiguration conf = request.sslConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(conf);

    reply = m_WebCtrl.get(request);
    reply->ignoreSslErrors();

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(error(QNetworkReply::NetworkError)));

    connect(reply, SIGNAL(downloadProgress(qint64, qint64)),
                this, SLOT(onprogress(qint64, qint64)));

    connect(reply, SIGNAL(readyRead()),
                this, SLOT(httpReadyRead()));

    connect(reply, SIGNAL(finished()),
                this, SLOT(fileDownloaded()));
#endif
}
 
FileDownloader::~FileDownloader() { }

void FileDownloader::onprogress(qint64 bytesRead, qint64 bytesTotal)
{
    emit progressUpdated(bytesRead, bytesTotal);
}

void FileDownloader::httpReadyRead()
{
    QByteArray bytes = reply->readAll();
    emit gotBytes(bytes.constData(), bytes.length());
}


void FileDownloader::error(QNetworkReply::NetworkError err)
{
    printf("Download zip error");
}
 
void FileDownloader::fileDownloaded() {
 //m_DownloadedData = pReply->readAll();
 //emit a signal
 emit downloaded(); 
}
