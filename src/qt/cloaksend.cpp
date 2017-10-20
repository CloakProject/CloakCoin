#include "cloaksend.h"
#include <QtCore>

cloaksend::cloaksend(QObject *parent) :
    QObject(parent)
{
}

QString cloaksend::getCloakedAddress()
{
    socket = new httpsocket(this); //create socket with

    //Add proxy support to socket as neccesary.
    if(this->useProxy==true){
        socket->useProxy = true;
        socket->proxyAddress = this->proxyAddress;
        socket->proxyPort = this->proxyPort;
    }

    QString url = "http://www.cloaksend.com/api/cloaks/";
    url.append(this->fromAddress);// Ex. "CBnkPKapz8U2FKc7EQSeCcSZ3qPD82XpGp");
    url.append("/");
    url.append(this->destinationAddress);//Ex. "C5qKmSjW1K1CiADtnMHMPBjQWybHQ9S8ce");
    url.append("/");
    url.append(this->amount);//Ex. "50.0");

    socket->getUrl(url);

    //Our custom socket will emit the signal finished when its got the data it needs.
    QEventLoop loop;
    QObject::connect(socket, SIGNAL(finished()), &loop, SLOT(quit()));

    loop.exec();//start the loop until socket is done.

    //qDebug()<<"HTML Response="<<socket->response;

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return "Error";
    }

    //Clean Output
    socket->response.replace(QString("\""),QString(""));//replace occurences of " with nothing.

    return socket->response;
}
