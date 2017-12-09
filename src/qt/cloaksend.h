#ifndef CLOAKSEND_H
#define CLOAKSEND_H

#include <QObject>
#include "httpsocket.h"

class cloaksend : public QObject
{
    Q_OBJECT
public:
    explicit cloaksend(QObject *parent = 0);
    QString fromAddress;
    QString destinationAddress;
    QString amount;
    QString getCloakedAddress(); //returns the cloaked address assuming object variables set correctly.
    bool useProxy;
    QString proxyAddress;
    int proxyPort;
signals:

public slots:

private:
    httpsocket *socket;
};

#endif // CLOAKSEND_H
