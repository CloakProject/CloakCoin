#ifndef JSONSAVERLOADER_H
#define JSONSAVERLOADER_H

#include <QObject>
#include <QtCore>
#include "QJsonValue.h"
#include "QJsonDocument.h"
#include "QJsonArray.h"
#include "QJsonObject.h"
#include "QJsonParseError.h"

class jsonsaverloader : public QObject
{
    Q_OBJECT
public:
    explicit jsonsaverloader(QObject *parent = 0);
    QJsonDocument loadJson(QString fileName);
    void saveJson(QJsonDocument document, QString fileName);
signals:

public slots:

};

#endif // JSONSAVERLOADER_H
