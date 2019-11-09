#include "httpsocket.h"
#include <QtCore>

httpsocket::httpsocket(QObject *parent) :
    QObject(parent)
{
    response = "";
    useProxy = false;
}

/* CURL based implementation */
// callback function writes data to a std::ostream
size_t httpsocket::data_write(void* buf, size_t size, size_t nmemb, void* userp)
{
	if(userp)
	{
		std::ostream& os = *static_cast<std::ostream*>(userp);
		std::streamsize len = size * nmemb;
		if(os.write(static_cast<char*>(buf), len))
			return len;
	}

	return 0;
}

/**
 * timeout is in seconds
 **/
CURLcode httpsocket::curl_read(const std::string& url, std::ostream& os, long timeout = 30)
{
	qDebug() << "curl_read: timeout: " << timeout << " url: " << QString::fromStdString(url);
	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = curl_easy_init();
        struct curl_slist *chunk = NULL;

	if(curl)
	{
    //Modify request to add headers if there are any.
    QMapIterator<QString, QString> i(this->headers);
    while (i.hasNext()) {
        i.next();
        qDebug() <<"Header:"<< i.key() << ": " << i.value() << endl;
        QString key = i.key();
        QString value = i.value();
        //request.setRawHeader( key.toLocal8Bit().data(), value.toLocal8Bit().data() );
        QString hd = key + ": " + value;
        chunk = curl_slist_append(chunk, hd.toLocal8Bit().data());
    }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		if(CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &data_write))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
		{
			code = curl_easy_perform(curl);
		}
		curl_easy_cleanup(curl);
	}

	qDebug() << "curl_read: return code: " << code;

	return code;
}

void httpsocket::getUrl(QString url)
{
    qDebug() << "httpsocket getCurl: Reading url:";
    qDebug() << url;

	curl_global_init(CURL_GLOBAL_ALL);
	
	std::ostringstream oss;
	if(CURLE_OK == curl_read(url.toStdString(), oss, 30))
	{
		// Web page successfully written to string
		//std::string html = oss.str();
		std::string html = oss.str();
		this->response = QString::fromStdString(html);
		qDebug() << "httpsocket::getUrl response: \n" << this->response;
	}

	curl_global_cleanup();

    qDebug() << "httpsocket getCurl: Emitting finished";
    emit finished();
}
