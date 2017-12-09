#ifndef EXCHANGE_H
#define EXCHANGE_H

#include <QObject>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <QtCore>
#include "httpsocket.h"
#include "QJsonValue.h"
#include "QJsonDocument.h"
#include "QJsonArray.h"
#include "QJsonObject.h"
#include "QJsonParseError.h"

class exchange : public QObject
{
    Q_OBJECT
public:
    explicit exchange(QObject *parent = 0);
    enum exchangeServices{
        Bittrex,
        Other
    }; //Holds the values for the exchanges we service.
    QString proxyAddress;
    int proxyPort;
    bool useProxy;
    QString apiSecret;
    QString apiKey;
    int selectedExchange;
    httpsocket *socket; //socket used for communication.
    QString balance;
    QString cryptoAddress;
    QString available;
    QString pending;
    QString btcAvailable;
    QList<QMap<QString,QVariant> > orderHistory;
    QList<QMap<QString,QVariant> > openOrders;
    QList<QMap<QString,QVariant> > withdrawelHistory;
    QList<QMap<QString,QVariant> > marketHistory;
    QList<QMap<QString,QVariant> > sellOrderBook;
    QList<QMap<QString,QVariant> > buyOrderBook;
    //Helper
    QString sha512(QString string);
    QString hmac(QString algo,QString key,QString secret);
    void setUpSocketAndFetch(QString key,QString secret, QString url); //Sets socket and headers to be ready to go.
    void setUpSocketAndFetch(QString url);//Version not requiring key and secret
    //Class Functions
    bool verifyApiKey(QString key,QString secret);
    //Public API
    void getMarkets();
    void getCurrencies();
    void getTicker();
    void getMarketSummaries();
    bool getOrderBook();
    bool getMarketHistory();
    //Market API
    bool buyLimit(QString key, QString secret,QString quantity,QString rate);
    bool buyMarket(QString key, QString secret,QString quantity);
    bool sellLimit(QString key, QString secret,QString quantity,QString rate);
    bool sellMarket(QString key, QString secret,QString quantity);
    bool cancel(QString key, QString secret,QString uuid);
    bool getOpenOrders(QString key, QString secret);
    //Account API
    void getBalances();
    void getBalance();
    void getDepositAddress();
    bool withdraw(QString key,QString secret,QString address,QString amount);
    bool getOrderHistory(QString key,QString secret);
    bool getWithdrawalHistory(QString key, QString secret);
    void getDepositHistory();
signals:

public slots:

};

#endif // EXCHANGE_H
