#include "exchange.h"

typedef unsigned char uint8_t;
	typedef signed char int8_t;
	typedef unsigned short uint16_t;
	typedef signed short int16_t;
    //typedef unsigned long uint32_t;

exchange::exchange(QObject *parent) :
    QObject(parent)
{
    this->selectedExchange = this->Bittrex;//set the default to use bittrex.
}

QString exchange::sha512(QString hashstring)
{
    //qDebug()<<"Running SHA512...";

    unsigned char digest[SHA512_DIGEST_LENGTH];

    //convert QString to cString for the SHA function.
    QByteArray ba = hashstring.toLocal8Bit();
    const char* string = ba.data();

    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, string, strlen(string));
    SHA512_Final(digest, &ctx);

    char mdString[SHA512_DIGEST_LENGTH*2+1];
        for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
            sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

    QString sha512hash = QString::fromUtf8(mdString);//assume UTF-8 C-string format.

    //qDebug()<<"SHA512 digest:"<< sha512hash;
    return sha512hash;
}

//Get access to the HMAC_Sha512 function
namespace HMAC_Sha512
{
    void hmacSha512(uint8_t *out, const uint8_t *key, const unsigned int key_length, const uint8_t *text, const unsigned int text_length);
};

QString exchange::hmac(QString algo, QString key, QString secret)
{
        if(algo!="Sha512"){
            return "Error - Algorithm not supported at this time.";
        }

        unsigned char a[64];
        HMAC_Sha512::hmacSha512(a,(const uint8_t*)secret.toLocal8Bit().data(),secret.length(),(const uint8_t*)key.toLocal8Bit().data(),key.length()); //key, keylength, message,message length

        QString hash = QString::fromLocal8Bit(QByteArray::fromRawData((const char*)a, 64).toHex());

        return hash;
}

void exchange::setUpSocketAndFetch(QString key, QString secret, QString url)
{
    qint64 time = QDateTime::currentMSecsSinceEpoch()/1000; //this will give seconds since epoc.
    QString timestring = QString::number(time);

    socket = new httpsocket(this);

    url.append(key);
    url.append("&nonce=");
    url.append(timestring);
    qDebug()<<url;

    QString hash  = this->hmac("Sha512",url,secret);

    //qDebug()<<"HMAC_Sha512_Hash="<<hash;

    socket->headers["apisign"] = hash;

    socket->getUrl(url);

    //Our custom socket will emit the signal finished when its got the data it needs.
//    QEventLoop loop;
//    QObject::connect(socket, SIGNAL(finished()), &loop, SLOT(quit()));

//    loop.exec();//start the loop until socket is done.

    qDebug()<<socket->response;
}

void exchange::setUpSocketAndFetch(QString url){
     socket = new httpsocket(this);

     socket->getUrl(url);

     //Our custom socket will emit the signal finished when its got the data it needs.
//     QEventLoop loop;
//     QObject::connect(socket, SIGNAL(finished()), &loop, SLOT(quit()));

//     loop.exec();//start the loop until socket is done.

     qDebug()<<socket->response;
}

bool exchange::verifyApiKey(QString key, QString secret)
{
    setUpSocketAndFetch(key,secret,"https://bittrex.com/api/v1.1/account/getbalance?currency=CLOAK&apikey=");

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON

    QJsonParseError *e = new QJsonParseError();
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit(),e);

    //If there is an error parsing report error and quit so it doesn't crash.
    if(e->error!=QJsonParseError::NoError){
        qDebug()<<"JSON Parse Error:"<<e->errorString();
        delete e;//free memory up from pointer we declared on stack.
        return false;
    }else{
        qDebug() << "verifyApiKey: check validated success";
    }

    QJsonObject     jsonObject = document.object();

    QStringList keys = jsonObject.keys();
    for (int i = 0; i < keys.size(); ++i){
        qDebug()<<"i:"<<QString::number(i)<<keys.at(i);
    }

    bool validated          = jsonObject.value("success").toBool();
    if(validated){
        QString message         = jsonObject.value("message").toString();
        QJsonObject result      = jsonObject.value("result").toObject();

QStringList keysR = result.keys();
    for (int j = 0; j < result.size(); ++j){
        qDebug()<<"result j:"<<QString::number(j)<<keysR.at(j);
    }

        //QJsonObject cloakdata   = result.value("CLOAK").toObject();

        this->cryptoAddress = result.value("CryptoAddress").toString();
        this->balance       = result.value("Balance").toVariant().toString();
        this->pending       = result.value("Pending").toVariant().toString();
        this->available     = result.value("Available").toVariant().toString();

	qDebug() << "Balance: " << result.value("Balance").toVariant().toString();


	setUpSocketAndFetch(key,secret,"https://bittrex.com/api/v1.1/account/getbalance?currency=BTC&apikey=");

        if(socket->response=="Error"){
            qDebug()<<"Error Occured:"<<socket->error;
            return false;
        }

        //Parse JSON
        QJsonParseError *e = new QJsonParseError();
        QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit(),e);

        //If there is an error parsing report error and quit so it doesn't crash.
        if(e->error!=QJsonParseError::NoError){
            qDebug()<<"JSON Parse Error:"<<e->errorString();
            delete e;//free memory up from pointer we declared on stack.
            return false;
        }else{
            qDebug() << "verifyApiKey: check validated success";
        }

        QJsonObject     jsonObject = document.object();

        QStringList keys = jsonObject.keys();
        for (int i = 0; i < keys.size(); ++i){
            qDebug()<<"i:"<<QString::number(i)<<keys.at(i);
        }

    	bool validated          = jsonObject.value("success").toBool();
    	if(validated){
            QString message         = jsonObject.value("message").toString();
            QJsonObject result      = jsonObject.value("result").toObject();

            //QJsonObject btcdata     = result.value("BTC").toObject();
            this->btcAvailable      = result.value("Balance").toVariant().toString();
	}
    }

    delete socket;
    delete e;//free memory for parse error check.
    return validated;
}

void exchange::getMarkets()
{
    socket = new httpsocket();

    switch(this->selectedExchange){
        case Bittrex:
            break;
    default:
        qDebug()<<"Exchange not supported";
    }
}

void exchange::getCurrencies()
{

}

void exchange::getTicker()
{

}

void exchange::getMarketSummaries()
{

}

bool exchange::getOrderBook()
{
    setUpSocketAndFetch("https://bittrex.com/api/v1.1/public/getorderbook?market=BTC-CLOAK&type=both&depth=100");

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated              = jsonObject.value("success").toBool();
    if(validated){
        QJsonObject result       = jsonObject.value("result").toObject();

        QJsonArray buyResults   = result.value("buy").toArray();

        for(int i=0;i<buyResults.size();i++){
            QJsonObject tempResultObj = buyResults.at(i).toObject();
            QMap<QString,QVariant> tempMap;

            tempMap["Quantity"]            = QVariant::fromValue(QString::number(tempResultObj.value("Quantity").toDouble(),'f',8));
            tempMap["Rate"]                = QVariant::fromValue(QString::number(tempResultObj.value("Rate").toDouble(),'f',8));

            //This is not included in the JSON so we can compute it here.
            tempMap["Total"]               = QVariant::fromValue(QString::number(tempMap["Quantity"].toDouble()*tempMap["Rate"] .toDouble(),'f',8));

            buyOrderBook.append(tempMap);
        }

        QJsonArray sellResults  = result.value("sell").toArray();

        for(int i=0;i<sellResults.size();i++){
            QJsonObject tempResultObj = sellResults.at(i).toObject();
            QMap<QString,QVariant> tempMap;

            tempMap["Quantity"]            = QVariant::fromValue(QString::number(tempResultObj.value("Quantity").toDouble(),'f',8));
            tempMap["Rate"]                = QVariant::fromValue(QString::number(tempResultObj.value("Rate").toDouble(),'f',8));

            //This is not included in the JSON so we can compute it here.
            tempMap["Total"]               = QVariant::fromValue(QString::number(tempMap["Quantity"].toDouble()*tempMap["Rate"] .toDouble(),'f',8));

            sellOrderBook.append(tempMap);
        }

    }

    delete socket;
    return validated;
}

bool exchange::getMarketHistory()
{
    setUpSocketAndFetch("https://bittrex.com/api/v1.1/public/getmarkethistory?market=BTC-CLOAK&count=100");

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();
    if(validated){
        //QString message         = jsonObject.value("message").toString();
        QJsonArray result      = jsonObject.value("result").toArray();

        for(int i=0;i<result.size();i++){
            QJsonObject tempResultObj = result.at(i).toObject();
            QMap<QString,QVariant> tempMap;

            tempMap["Id"]                   = tempResultObj.value("Id").toVariant();
            tempMap["TimeStamp"]            = tempResultObj.value("TimeStamp").toVariant();
            tempMap["Quantity"]             = QVariant::fromValue(QString::number(tempResultObj.value("Quantity").toDouble(),'f',8));
            tempMap["Price"]                = QVariant::fromValue(QString::number(tempResultObj.value("Price").toDouble(),'f',8));
            tempMap["Total"]                = QVariant::fromValue(QString::number(tempResultObj.value("Total").toDouble(),'f',8));
            tempMap["FillType"]             = tempResultObj.value("FillType").toVariant();
            tempMap["OrderType"]            = tempResultObj.value("OrderType").toVariant();

            marketHistory.append(tempMap);
        }

    }

    delete socket;
    return validated;
}

bool exchange::buyLimit(QString key, QString secret, QString quantity, QString rate)
{
    QString url = "https://bittrex.com/api/v1.1/market/buylimit?market=BTC-CLOAK&quantity=";
    url.append(quantity);
    url.append("&rate=");
    url.append(rate);
    url.append("&apikey=");
    setUpSocketAndFetch(key,secret,url);

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();

    delete socket;//free memory

    return validated;
}

bool exchange::buyMarket(QString key, QString secret, QString quantity)
{
    QString url = "https://bittrex.com/api/v1.1/market/buymarket?market=BTC-CLOAK&quantity=";
    url.append(quantity);
    url.append("&apikey=");
    setUpSocketAndFetch(key,secret,url);

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();

    delete socket;//free memory

    return validated;
}

bool exchange::sellLimit(QString key, QString secret, QString quantity, QString rate)
{
    QString url = "https://bittrex.com/api/v1.1/market/selllimit?market=BTC-CLOAK&quantity=";
    url.append(quantity);
    url.append("&rate=");
    url.append(rate);
    url.append("&apikey=");
    setUpSocketAndFetch(key,secret,url);

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();

    delete socket;//free memory

    return validated;
}

bool exchange::sellMarket(QString key, QString secret, QString quantity)
{
    QString url = "https://bittrex.com/api/v1.1/market/sellmarket?market=BTC-CLOAK&quantity=";
    url.append(quantity);
    url.append("&apikey=");
    setUpSocketAndFetch(key,secret,url);

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();

    delete socket;//free memory

    return validated;
}

bool exchange::cancel(QString key, QString secret, QString uuid)
{
     QString url = "https://bittrex.com/api/v1.1/market/cancel?uuid=";
     url.append(uuid);
     url.append("&apikey=");
     setUpSocketAndFetch(key,secret,url);

     if(socket->response=="Error"){
         qDebug()<<"Error Occured:"<<socket->error;
         return false;
     }

     //Parse JSON
     QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
     QJsonObject     jsonObject = document.object();

     bool validated          = jsonObject.value("success").toBool();

     delete socket;//free memory

     return validated;
}

bool exchange::getOpenOrders(QString key, QString secret)
{
    setUpSocketAndFetch(key,secret,"https://bittrex.com/api/v1.1/market/getopenorders?market=BTC-CLOAK&apikey=");

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();
    if(validated){
        //QString message         = jsonObject.value("message").toString();
        QJsonArray result      = jsonObject.value("result").toArray();

        for(int i=0;i<result.size();i++){
            QJsonObject tempResultObj = result.at(i).toObject();
            QMap<QString,QVariant> tempMap;

            tempMap["OrderUuid"]            = tempResultObj.value("OrderUuid").toVariant();
            tempMap["Exchange"]             = tempResultObj.value("Exchange").toVariant();
            tempMap["TimeStamp"]            = tempResultObj.value("TimeStamp").toVariant();
            tempMap["OrderType"]            = tempResultObj.value("OrderType").toVariant();
            tempMap["Limit"]                = QVariant::fromValue(QString::number(tempResultObj.value("Limit").toDouble(),'f',8));
            tempMap["Quantity"]             = QVariant::fromValue(QString::number(tempResultObj.value("Quantity").toDouble(),'f',8));
            tempMap["QuantityRemaining"]    = QVariant::fromValue(QString::number(tempResultObj.value("QuantityRemaining").toDouble(),'f',8));
            tempMap["Price"]                = QVariant::fromValue(QString::number(tempResultObj.value("Price").toDouble(),'f',8));
            tempMap["PricePerUnit"]         = QVariant::fromValue(QString::number(tempResultObj.value("PricePerUnit").toDouble(),'f',8));
            tempMap["CancelInitiated"]      = tempResultObj.value("CancelInitiated").toVariant();
            tempMap["IsConditional"]        = tempResultObj.value("OrderUuid").toVariant();
            tempMap["Condition"]            = tempResultObj.value("Condition").toVariant();
            tempMap["ConditionTarget"]      = tempResultObj.value("ConditionTarget").toVariant();
            tempMap["ImmediateOrCancel"]    = tempResultObj.value("ImmediateOrCancel").toVariant();

            openOrders.append(tempMap);
        }

    }

    delete socket;
    return validated;
}

void exchange::getBalances()
{

}

void exchange::getBalance()
{

}

void exchange::getDepositAddress()
{

}

bool exchange::withdraw(QString key,QString secret,QString address, QString amount)
{
    QString url = "https://bittrex.com/api/v1.1/account/withdraw?currency=CLOAK";
    url.append("&quantity=");
    url.append(amount);
    url.append("&address=");
    url.append(address);
    url.append("&apikey=");
    setUpSocketAndFetch(key,secret,url);

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();

    return validated;
}

bool exchange::getOrderHistory(QString key, QString secret)
{
    setUpSocketAndFetch(key,secret,"https://bittrex.com/api/v1.1/account/getorderhistory?market=BTC-CLOAK&apikey=");

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();
    if(validated){
        //QString message         = jsonObject.value("message").toString();
        QJsonArray result      = jsonObject.value("result").toArray();

        for(int i=0;i<result.size();i++){
            QJsonObject tempResultObj = result.at(i).toObject();
            QMap<QString,QVariant> tempMap;

            tempMap["OrderUuid"]            = tempResultObj.value("OrderUuid").toVariant();
            tempMap["Exchange"]             = tempResultObj.value("Exchange").toVariant();
            tempMap["TimeStamp"]            = tempResultObj.value("TimeStamp").toVariant();
            tempMap["OrderType"]            = tempResultObj.value("OrderType").toVariant();
            tempMap["Limit"]                = QVariant::fromValue(QString::number(tempResultObj.value("Limit").toDouble(),'f',8));
            tempMap["Quantity"]             = QVariant::fromValue(QString::number(tempResultObj.value("Quantity").toDouble(),'f',8));
            tempMap["QuantityRemaining"]    = QVariant::fromValue(QString::number(tempResultObj.value("QuantityRemaining").toDouble(),'f',8));
            tempMap["Commission"]           = QVariant::fromValue(QString::number(tempResultObj.value("Commission").toDouble(),'f',8));
            tempMap["Price"]                = QVariant::fromValue(QString::number(tempResultObj.value("Price").toDouble(),'f',8));
            tempMap["PricePerUnit"]         = QVariant::fromValue(QString::number(tempResultObj.value("PricePerUnit").toDouble(),'f',8));
            tempMap["IsConditional"]        = tempResultObj.value("OrderUuid").toVariant();
            tempMap["Condition"]            = tempResultObj.value("Condition").toVariant();
            tempMap["ConditionTarget"]      = tempResultObj.value("ConditionTarget").toVariant();
            tempMap["ImmediateOrCancel"]    = tempResultObj.value("ImmediateOrCancel").toVariant();

            orderHistory.append(tempMap);
        }

    }

    delete socket;
    return validated;
}

bool exchange::getWithdrawalHistory(QString key, QString secret)
{
    setUpSocketAndFetch(key,secret,"https://bittrex.com/api/v1.1/account/getwithdrawalhistory?currency=CLOAK&apikey=");

    if(socket->response=="Error"){
        qDebug()<<"Error Occured:"<<socket->error;
        return false;
    }

    //Parse JSON
    QJsonDocument   document = QJsonDocument::fromJson(socket->response.toLocal8Bit());
    QJsonObject     jsonObject = document.object();

    bool validated          = jsonObject.value("success").toBool();
    if(validated){
        //QString message         = jsonObject.value("message").toString();
        QJsonArray result      = jsonObject.value("result").toArray();

        for(int i=0;i<result.size();i++){
            QJsonObject tempResultObj = result.at(i).toObject();
            QMap<QString,QVariant> tempMap;

            tempMap["PaymentUuid"]          = tempResultObj.value("PaymentUuid").toVariant();
            tempMap["Currency"]             = tempResultObj.value("Currency").toVariant();
            tempMap["Amount"]               = QVariant::fromValue(QString::number(tempResultObj.value("Amount").toDouble(),'f',8));
            tempMap["Address"]              = tempResultObj.value("Address").toVariant();
            tempMap["Opened"]               = tempResultObj.value("Opened").toVariant();
            tempMap["Authorized"]           = tempResultObj.value("Authorized").toVariant();
            tempMap["PendingPayment"]       = tempResultObj.value("PendingPayment").toVariant();
            tempMap["TxCost"]               = QVariant::fromValue(QString::number(tempResultObj.value("TxCost").toDouble(),'f',8));
            tempMap["TxId"]                 = tempResultObj.value("TxId").toVariant();
            tempMap["Canceled"]             = tempResultObj.value("Canceled").toVariant();
            tempMap["InvalidAddress"]       = tempResultObj.value("InvalidAddress").toVariant();

            withdrawelHistory.append(tempMap);
        }
    }

    delete socket;
    return validated;
}


void exchange::getDepositHistory()
{

}
