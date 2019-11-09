#include "jsonsaverloader.h"

jsonsaverloader::jsonsaverloader(QObject *parent) :
    QObject(parent)
{

}

QJsonDocument jsonsaverloader::loadJson(QString fileName)
{
    QFile jsonFile(fileName,this);
    jsonFile.open(QFile::ReadOnly);
    return QJsonDocument::fromJson(jsonFile.readAll());
}

void jsonsaverloader::saveJson(QJsonDocument document, QString fileName)
{
    QFile jsonFile(fileName,this);
    jsonFile.open(QFile::WriteOnly);
    qint64 result = jsonFile.write(document.toJson());
    if(result==-1){
        qDebug()<<"Failed To Save!";
    }else{
        qDebug()<<"Saved ("<<jsonFile.fileName()<<")"<<" PATH="<<QDir::currentPath();
    }
}
