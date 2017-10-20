#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include "cloaksend.h"
#include "exchange.h"
#include "tickertimer.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    //qDebug()<<"Setting up CloakTrade UI";
    ui->setupUi(this);
    isInitialized = false;
    sellMaxActive = false;
    buyMaxActive = false;
}

void MainWindow::initializeCloakTrade()
{
    if(!isInitialized)
    {
	qDebug()<<"initializeCloakTrade...";

	try
	{

	    qDebug()<<"Initialize QFile for keys.dat";
	    //Load keys.dat file if exists for API Remembering.
	    jsonsaverloader jsonLoader;
	    QFile keydata("keys.dat");

	    if(keydata.exists()){
		qDebug()<<"Found keys.dat, loading json...";
		QJsonDocument document = jsonLoader.loadJson("keys.dat");

		QJsonObject jsonObject = document.object();

		apikey      = jsonObject.value("key").toString();
		apisecret   = jsonObject.value("secret").toString();

		ui->apiKeyField->setText(apikey);
		ui->apiSecretField->setText(apisecret);

		qDebug()<<"Call to verify exchange....";
		verifyExchange();
		qDebug()<<"Call to verify exchange bypassed.";
	    }else{
		qDebug()<<"No save file found for api keys";
	    }

	    qDebug()<<"Hide a few sell & buy elements by default";
	    //Hide Conditionals on SELL & BUY
	    ui->buyWhenPriceLabel->hide();
	    ui->buyWhenPriceComboBox->hide();
	    ui->buyWhenPriceLineEdit->hide();
	    ui->buyWhenPriceBTCLabel->hide();

	    ui->sellWhenPriceIsLabel->hide();
	    ui->sellWhenPriceIsComboBox->hide();
	    ui->sellWhenPriceIsLineEdit->hide();
	    ui->sellWhenPriceIsBTCLabel->hide();

	    //Hide hidden UUID Field for open orders.  This is needed to cancel order, but not needed to show to the user.
	    ui->openOrdersTableWidget->setColumnHidden(OPENORDERS_HEADER_UUID,true);

	    qDebug()<<"Set selected exchange to Bittrex";
	    //Set default selected exchange to bittrex
	    selectedExchange = exchange::Bittrex;

	    qDebug()<<"Call tickerTimerUpdate initially to get quick price update";
	    isTickerUpdating = false;
	    tickerTimerUpdate();//run initially to get quick price update

	    isTickerUpdating = false;
	    qDebug()<<"Constructing tickerTimer";
	    theTickerTimer = new tickertimer();

	    qDebug()<<"Connecting tickerTimerUpdate slot";
	    connect(theTickerTimer,SIGNAL(timerReady()),this,SLOT(tickerTimerUpdate()));

	    qDebug()<<"Starting timer";
	    theTickerTimer->start();

	}
	catch(std::exception& e)
	{
		qDebug() << "initializeCloakTrade: Exception occurred\n";
		qDebug() << e.what();
	}

	isInitialized = true;
    }
}

MainWindow::~MainWindow()
{
    /*try
    {
        stopUpdaterTimer();
    }
    catch(std::exception& e)
    {
	qDebug() << "stopUpdaterTimer: Exception occurred\n";
	qDebug() << e.what();
    }*/
    delete ui;
}

void MainWindow::testWorking()
{
    qDebug()<<"testWorking called";
}

void MainWindow::verifyExchange()
{
    // clip whitespace
    ui->apiSecretField->setText(ui->apiSecretField->text().trimmed());
    ui->apiKeyField->setText(ui->apiKeyField->text().trimmed());

    QString secret              = ui->apiSecretField->text();
    QString key                 = ui->apiKeyField->text();

    exchange exchangeService(this);

    exchangeService.selectedExchange = selectedExchange;//set exchange to bittrex.

    bool verified = exchangeService.verifyApiKey(key,secret);

    if(verified==true){
        qDebug()<<"API key verified!";

        ui->messageLabel->setText("<span style='color:#006400'>API key verified!</span>");
        ui->balanceLabel->setText(exchangeService.balance);
        ui->sellCloakAvailableLabel->setText(exchangeService.balance);
        ui->availableLabel->setText(exchangeService.available);
        ui->cryptoAddressField->setText(exchangeService.cryptoAddress);
        ui->pendingLabel->setText(exchangeService.pending);
        ui->btcAvailableLabel->setText(exchangeService.btcAvailable);
        ui->buytab_btcAvailableLabel->setText(exchangeService.btcAvailable);

        btcAvailable = exchangeService.btcAvailable;//set mainwindow reference variable.
        cloakAvailable = exchangeService.balance;

        apiverified = true;//TODO: At this point a timer could be set to keep track of the balance as long as the api key remains verified.

    }else{
        qDebug()<<"Failed to verify key & secret";
        ui->messageLabel->setText("<span style='color:#FF0000'>API key failed to verify!</span>");
    }
}

void MainWindow::updateWithdrawHistory()
{
    exchange exchangeService(this);
    exchangeService.selectedExchange = selectedExchange;

    ui->balanceWithdrawelHistoryTableWidget->setRowCount(0);//clear the table out to redraw.

    bool worked = exchangeService.getWithdrawalHistory(apikey,apisecret);
    if(worked){
        for(int i=0;i<exchangeService.withdrawelHistory.size();i++){
        //Loop through QList from exchange
            //Insert row at end
            int rowCount = ui->balanceWithdrawelHistoryTableWidget->rowCount();
            ui->balanceWithdrawelHistoryTableWidget->insertRow(rowCount);

            //Display results to correct row and column
            QDateTime datetime = QDateTime::fromString(exchangeService.withdrawelHistory.at(i).value("Opened").toString(),"yyyy-MM-ddThh:mm:ss.z");

            ui->balanceWithdrawelHistoryTableWidget->setItem(rowCount,BALANCE_HEADER_TIME,          new QTableWidgetItem(datetime.toString("yyyy-MM-dd hh:mm:ss")));
            ui->balanceWithdrawelHistoryTableWidget->setItem(rowCount,BALANCE_HEADER_AMOUNT,        new QTableWidgetItem(exchangeService.withdrawelHistory.at(i).value("Amount").toString()));
            ui->balanceWithdrawelHistoryTableWidget->setItem(rowCount,BALANCE_HEADER_ADDRESS,       new QTableWidgetItem(exchangeService.withdrawelHistory.at(i).value("Address").toString()));
        }
    }else{
        qDebug()<<"Failed To Get Withdrawel History";
    }
}

void MainWindow::calculateMaxBuyUnits(double price)
{
    qDebug()<<"Calculating Max Units...";
    QVariant maxCloak = QString::number(btcAvailable.toDouble()/(price*1.0025),'f',8);
    ui->units_maxLineEdit->setText(maxCloak.toString());
    calculateTotalBuyPrice();
}

void MainWindow::calculateTotalBuyPrice()
{
    QVariant unitAmount = ui->units_maxLineEdit->text();
    QVariant price      = ui->bid_lineEdit->text();
    QVariant commission = (price.toDouble()*.0025)*unitAmount.toDouble();
    //qDebug()<<"commission="<<QString::number(commission.toDouble(),'f',8);
    QVariant total      = QString::number(unitAmount.toDouble()*price.toDouble()+commission.toDouble(),'f',8);
    ui->totalWithFee->setText(total.toString());
    totalbuyprice = total; //copy for reference in verifyBuy
}

void MainWindow::calculateTotalSellPrice()
{
    QVariant unitAmount = ui->sellUnitsLineEdit->text();
    QVariant price      = ui->sellAskLineEdit->text();
    QVariant commission = (price.toDouble()*.0025)*unitAmount.toDouble();
    //qDebug()<<"commission="<<QString::number(commission.toDouble(),'f',8);
    QVariant total      = QString::number(unitAmount.toDouble()*price.toDouble()-commission.toDouble(),'f',8);
    ui->sellTotalLabel->setText(total.toString());
    totalsellprice = total; //copy for reference in verifyBuy
}

bool MainWindow::validateBuy()
{
    //Validates that the buy request to the exchange will make sense.
    if(buyMarketActive==false){
        if(totalbuyprice.toDouble()>btcAvailable.toDouble()){
            return false;
        }
        if(totalbuyprice.toDouble()==0){
            return false;
        }
    }else{
        QVariant units = ui->units_maxLineEdit->text();
        if(units.toDouble()<=0){
            return false;
        }
    }

    return true;
}

bool MainWindow::validateSell()
{
    //Validates that the sell request to the exchange will make sense.

    //can't be selling where your income is 0
    if(totalsellprice.toDouble()==0){
        return false;
    }

    //You have to be trying to sell more than 0 units
    QVariant units = ui->sellUnitsLineEdit->text();
    if(units.toDouble()<=0){
        return false;
    }

    return true;
}

//void MainWindow::on_actionView_Exchange_triggered()
//{
    //TODO: Put code to show tab control here or start any process's
//}

//void MainWindow::on_actionCloak_Send_Coins_triggered()
//{
    //All fields below need to be filled in from GUI Elements
/*    cloaksend *cloakservice = new cloaksend();
    cloakservice->amount                = "15.0";
    cloakservice->fromAddress           = "CBnkPKapz8U2FKc7EQSeCcSZ3qPD82XpGp";
    cloakservice->destinationAddress    = "C5qKmSjW1K1CiADtnMHMPBjQWybHQ9S8ce";
    cloakservice->useProxy              = false;
    cloakservice->proxyAddress          = "";
    cloakservice->proxyPort             = 80;

    QString cloakedaddress = cloakservice->getCloakedAddress(); //TODO: Put this value to coresponding GUI Element

    //Emits either error or finished
    //TODO: Make connections for those signals.

    //TODO: Assumes variables below will be set form GUI Elements instead of just debugged out.
    qDebug()<<"Cloaked Address="<<cloakedaddress;
    delete cloakservice;
}*/

void MainWindow::on_saveButton_clicked()
{
    QString secret     = ui->apiSecretField->text();
    QString key        = ui->apiKeyField->text();

    //Set Globals
    apisecret   = secret;
    apikey      = key;

    QJsonDocument document;

    QVariantMap map;

    map.insert("secret", secret);
    map.insert("key", key);

    QJsonObject jsonObject = QJsonObject::fromVariantMap(map);

    document.setObject(jsonObject);

    jsonsaverloader *jsonLoader = new jsonsaverloader(this);

    jsonLoader->saveJson(document,"keys.dat");
    delete jsonLoader;
}

void MainWindow::on_checkButton_clicked()
{
    verifyExchange();
}

void MainWindow::tickerTimerUpdate()
{
    qDebug()<<"Ticker Update...";
    //if(!isTickerUpdating)
    //{
    isTickerUpdating = true;
	tickerSocket = new httpsocket(this); //create socket with

	tickerSocket->useProxy = false; //TODO: set this to GUI interface result
	tickerSocket->proxyAddress = ""; //"200.50.166.43"; //TODO: set this to GUI interface result
	tickerSocket->proxyPort = 80; //80; //TODO: set this to GUI interface result

    QString url = "https://bittrex.com/api/v1.1/public/getticker?market=BTC-CLOAK";

	qDebug()<<"ticker: getting url";
	tickerSocket->getUrl(url);
	qDebug() << "ticker: process response";
	tickerResponseFinished();
	//Our custom socket will emit the signal finished when its got the data it needs.
	//qDebug()<<"ticker: setup event loop";
	//QEventLoop loop;
	//QObject::connect(tickerSocket, SIGNAL(finished()), &loop, SLOT(quit()));
	//qDebug()<<"ticker: start loop until socket done";
	//loop.exec();//start the loop until socket is done.

	//qDebug() << "ticker: connect slot for tickerSocket finished signal";
	//QObject::connect(tickerSocket, SIGNAL(finished()), this, SLOT(tickerResponseFinished()));    
	qDebug() << "ticker: slot connected";
	QCoreApplication::processEvents();
    /*}
    else
    {
	qDebug() << "ticker is already updating, skipping this interval";
    }*/
}

void MainWindow::tickerResponseFinished()
{
    qDebug() << "tickerSocket Finished";
    qDebug()<<tickerSocket->response;

    try
    {
        if(tickerSocket->response=="Error"){
        qDebug()<<"Error Occured:"<<tickerSocket->error;
        }

        qDebug() << "tickerResponseFinished: get json document";


        QJsonParseError *e = new QJsonParseError();
        QJsonDocument   document = QJsonDocument::fromJson(tickerSocket->response.toLocal8Bit(),e);

        //If there is an error parsing report error and quit so it doesn't crash.
        if(e->error!=QJsonParseError::NoError){
            qDebug()<<"JSON Parse Error:"<<e->errorString();
            delete e;//free memory up from pointer we declared on stack.
            return;
        }else{
            qDebug() << "tickerResponseFinished: check validated success";
        }

        QJsonObject     jsonObject = document.object();
        qDebug() << "Creating initial jsonObject...";

        bool validated              = jsonObject.value("success").toBool();
        if(validated){
        //QString message         = jsonObject.value("message").toString();
        qDebug() << "tickerResponseFinished: get result";
        QJsonObject result      = jsonObject.value("result").toObject();

        qDebug() << "Get Bid/Ask/Last";
        QString bid = result.value("Bid").toVariant().toString();
        QString ask = result.value("Ask").toVariant().toString();
        QString last = result.value("Last").toVariant().toString();

        //Set reminders of previous values if different
        if(bidprice.toDouble() != bid.toDouble()){
            if(bidprice.toDouble()>bid.toDouble()){//if price change is lower show red
            bidprice    = bid;//set bidprice to bid before we change it.
            bid.prepend("<span style='color:#FF0000'>");
            bid.append("</span>");
            }else{//otherwise green
            bidprice    = bid;
            bid.prepend("<span style='color:#00CC00'>");
            bid.append("</span>");
            }
        }
        if(askprice.toDouble() != ask.toDouble()){
            if(askprice.toDouble()>ask.toDouble()){//if price change is lower show red
            askprice    = ask;//set bidprice to bid before we change it.
            ask.prepend("<span style='color:#FF0000'>");
            ask.append("</span>");
            }else{//otherwise green
            askprice    = ask;
            ask.prepend("<span style='color:#00CC00'>");
            ask.append("</span>");
            }
        }
        if(lastprice.toDouble() != last.toDouble()){
            if(lastprice.toDouble()>last.toDouble()){//if price change is lower show red
            lastprice    = last;//set bidprice to bid before we change it.
            last.prepend("<span style='color:#FF0000'>");
            last.append("</span>");
            }else{//otherwise green
            lastprice    = last;
            last.prepend("<span style='color:#00CC00'>");
            last.append("</span>");
            }
        }

        qDebug() << "tickerResponseFinished: Append Currency Ticker to End";
        //Append Currency Ticker To End
        bid.append(" BTC");
        ask.append(" BTC");
        last.append(" BTC");

        qDebug() << "tickerResponseFinished: setText bid/ask/last";
        ui->bidAmountLabel->setText(bid);
        ui->askAmountLabel->setText(ask);
        ui->lastAmountLabel->setText(last);

        qDebug() << "tickerResponseFinished: Update timer control";
        //Update timer control
        ui->lastUpdateTimeEdit->setTime(QTime::currentTime());
        }else{
        qDebug()<<"Error getting ticker results...";
        }

        qDebug() << "tickerResponseFinished: Delete ticker socket";
        //delete tickerSocket;//free memory for and all space allocated within~

        qDebug() << "tickerResponseFinished: isTickerUpdating = false";
        isTickerUpdating = false;
        delete e;//free memory up from pointer we created.
    }
    catch(std::exception& ex)
    {
        isTickerUpdating = false;
        qDebug() << "tickerResponseFinished: Exception occurred\n";
        qDebug() << ex.what();
    }
}

void MainWindow::stopUpdaterTimer()
{
    isTickerUpdating = false;
    delete theTickerTimer;
    //theTickerTimer = NULL;
}

void MainWindow::on_checkBox_toggled(bool checked)
{
    if(checked==false){
       delete theTickerTimer;
    }else{
        theTickerTimer = new tickertimer();
        connect(theTickerTimer,SIGNAL(timerReady()),this,SLOT(tickerTimerUpdate()));
        theTickerTimer->start();
    }
}

void MainWindow::on_units_maxButton_toggled(bool checked)
{
    QVariant number;
    number = ui->bid_lineEdit->text();

    if(checked){
        buyMaxActive = true;
        calculateMaxBuyUnits(number.toDouble());
    }else{
        buyMaxActive = false;
    }
    calculateTotalBuyPrice();
}

void MainWindow::on_bid_comboBox_currentIndexChanged(const QString &arg1)
{
    QVariant number;

    if(arg1 == "Price"){
        number = ui->bid_lineEdit->text();
    }else if(arg1=="Ask"){
        number = askprice;
        ui->bid_lineEdit->setText(number.toString());
    }else if(arg1=="Last"){
        number = lastprice;
        ui->bid_lineEdit->setText(number.toString());
    }else if(arg1=="Bid"){
        number = bidprice;
        ui->bid_lineEdit->setText(number.toString());
    }

    if(buyMaxActive){
        calculateMaxBuyUnits(number.toDouble()); //will calculate and display max units.
    }
    calculateTotalBuyPrice();
}

void MainWindow::on_buy_cloakbutton_clicked()
{
    if(validateBuy()){
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Buy?", "Are you sure you want to buy?", QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {

            exchange exchangeService(this);
            exchangeService.selectedExchange = selectedExchange;//Set exchange to bittrex.

            QString quantity    = ui->units_maxLineEdit->text();
            QString rate        = ui->bid_lineEdit->text();

            if(ui->buyOrderTypeComboBox->currentText()=="Limit"){
                bool worked = exchangeService.buyLimit(apikey,apisecret,quantity,rate);

                if(worked){
		    QMessageBox::information(this, "Buy Order (Limit) Succeeded", "Buy Succeeded");
                    qDebug()<<"Buy Order (Limit) Placed Succesfully!";
                }else{
		    QMessageBox::information(this, "Buy Order (Limit) Failed", "Buy Failed");
                    qDebug()<<"Buy Order (Limit) Failed";
                }
            }
            if(ui->buyOrderTypeComboBox->currentText()=="Market"){
                bool worked = exchangeService.buyMarket(apikey,apisecret,quantity);

                if(worked){
		    QMessageBox::information(this, "Buy Order (Market) Succeeded", "Buy Placed Successfully");
                    qDebug()<<"Buy Order (Market) Placed";
		    QMessageBox::information(this, "Buy Order (Market) Failed", "Buy Failed");
                }else{
                    qDebug()<<"Buy Order (Market) Failed";
                }
            }
        } else {
            qDebug() << "Buy request canceled...";
        }
    }else{
        QMessageBox::information(this, "Buy Request Invalid", "Buy Request Invalid");
        qDebug()<<"Buy request invalid!";
    }
}

void MainWindow::on_bid_lineEdit_textChanged(const QString &arg1)
{
    if(buyMaxActive){
        calculateMaxBuyUnits(arg1.toDouble());
    }
    calculateTotalBuyPrice();
}

void MainWindow::on_bid_lineEdit_textEdited(const QString &arg1)
{
    if(buyMaxActive){
        calculateMaxBuyUnits(arg1.toDouble());
    }
    calculateTotalBuyPrice();
    int index = ui->bid_comboBox->findText("Price");
    if(index > -1)
        ui->bid_comboBox->setCurrentIndex(index);
}

void MainWindow::on_units_maxLineEdit_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1);

    if(buyMaxActive){
        QVariant bid = ui->bid_lineEdit->text();
        calculateMaxBuyUnits(bid.toDouble());
    }
    calculateTotalBuyPrice();
}

void MainWindow::on_updateBalanceButton_clicked()
{
    verifyExchange();
    updateWithdrawHistory();
}

void MainWindow::on_exchangeWidget_currentChanged(int index)
{
    switch(index){
        case TAB_ORDERHISTORY:
        {
            exchange exchangeService(this);
            exchangeService.selectedExchange = selectedExchange;//Set exchange to bittrex.

            ui->orderHistoryTableWidget->setRowCount(0);

            bool worked = exchangeService.getOrderHistory(apikey,apisecret);
            if(worked){
                for(int i=0;i<exchangeService.orderHistory.size();i++){
                //Loop through QList from exchange for OrderHistory
                    //Insert row at end
                    int rowCount = ui->orderHistoryTableWidget->rowCount();
                    ui->orderHistoryTableWidget->insertRow(rowCount);

                    //Display results to correct row and column
                    QDateTime datetime = QDateTime::fromString(exchangeService.orderHistory.at(i).value("TimeStamp").toString(),"yyyy-MM-ddThh:mm:ss.z");

                    ui->orderHistoryTableWidget->setItem(rowCount,ORDERHISTORY_HEADER_TIME,                 new QTableWidgetItem(datetime.toString("yyyy-MM-dd hh:mm:ss")));
                    ui->orderHistoryTableWidget->setItem(rowCount,ORDERHISTORY_HEADER_TYPE,                 new QTableWidgetItem(exchangeService.orderHistory.at(i).value("OrderType").toString()));
                    ui->orderHistoryTableWidget->setItem(rowCount,ORDERHISTORY_HEADER_BIDASK,               new QTableWidgetItem(exchangeService.orderHistory.at(i).value("Limit").toString()));
                    ui->orderHistoryTableWidget->setItem(rowCount,ORDERHISTORY_HEADER_COMMISSION,           new QTableWidgetItem(exchangeService.orderHistory.at(i).value("Commission").toString()));
                    ui->orderHistoryTableWidget->setItem(rowCount,ORDERHISTORY_HEADER_COSTPROCEEDS,         new QTableWidgetItem(exchangeService.orderHistory.at(i).value("Price").toString()));
                    ui->orderHistoryTableWidget->setItem(rowCount,ORDERHISTORY_HEADER_QUANTITY,             new QTableWidgetItem(exchangeService.orderHistory.at(i).value("Quantity").toString()));
                    ui->orderHistoryTableWidget->setItem(rowCount,ORDERHISTORY_HEADER_QUANTITYREMAINING,    new QTableWidgetItem(exchangeService.orderHistory.at(i).value("QuantityRemaining").toString()));
                }
            }else{
                qDebug()<<"Failed To Get Order History";
            }
            break;
        }
        case TAB_OPENORDERS:
        {
            exchange exchangeService(this);
            exchangeService.selectedExchange = selectedExchange;

            ui->openOrdersTableWidget->setRowCount(0);

            bool worked = exchangeService.getOpenOrders(apikey,apisecret);
            if(worked){
                for(int i=0;i<exchangeService.openOrders.size();i++){
                //Loop through QList from exchange
                    //Insert row at end
                    int rowCount = ui->openOrdersTableWidget->rowCount();
                    ui->openOrdersTableWidget->insertRow(rowCount);

                    //Display results to correct row and column
                    QDateTime datetime = QDateTime::fromString(exchangeService.openOrders.at(i).value("TimeStamp").toString(),"yyyy-MM-ddThh:mm:ss.z");

                    ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_TIME,                 new QTableWidgetItem(datetime.toString("yyyy-MM-dd hh:mm:ss")));
                    ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_TYPE,                 new QTableWidgetItem(exchangeService.openOrders.at(i).value("OrderType").toString()));
                    ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_BIDASK,               new QTableWidgetItem(exchangeService.openOrders.at(i).value("Limit").toString()));
                    ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_QUANTITY,             new QTableWidgetItem(exchangeService.openOrders.at(i).value("Quantity").toString()));
                    ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_QUANTITYREMAINING,    new QTableWidgetItem(exchangeService.openOrders.at(i).value("QuantityRemaining").toString()));
                    ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_UUID,                 new QTableWidgetItem(exchangeService.openOrders.at(i).value("OrderUuid").toString()));

                    QTableWidgetItem *cancelitem = new QTableWidgetItem("CANCEL");

                    QColor blue;
                    blue.setRgb(51,153,255);

                    QFont font;
                    font.setUnderline(true);

                    cancelitem->setTextColor(blue);
                    cancelitem->setFont(font);
                    cancelitem->setTextAlignment(Qt::AlignCenter);

                    if(exchangeService.openOrders.at(i).value("CancelInitiated").toBool()==false){
                        ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_CANCEL,           cancelitem);
                    }else{
                       ui->openOrdersTableWidget->setItem(rowCount,OPENORDERS_HEADER_CANCEL,            new QTableWidgetItem("Canceling..."));
                    }
                }
            }else{
                qDebug()<<"Failed to get Open Orders.";
            }
            break;
        }
        case TAB_BALANCE:
        {
            updateWithdrawHistory();
            break;
        }
        case TAB_MARKETHISTORY:
        {
            exchange exchangeService(this);
            exchangeService.selectedExchange = selectedExchange;

            ui->marketHistoryTableWidget->setRowCount(0);//clear the table out to redraw.
            bool worked = exchangeService.getMarketHistory();
            if(worked){
                for(int i=0;i<exchangeService.marketHistory.size();i++){
                //Loop through QList from exchange
                    //Insert row at end
                    int rowCount = ui->marketHistoryTableWidget->rowCount();
                    ui->marketHistoryTableWidget->insertRow(rowCount);

                    //Display results to correct row and column
                    QDateTime datetime = QDateTime::fromString(exchangeService.marketHistory.at(i).value("TimeStamp").toString(),"yyyy-MM-ddThh:mm:ss.z");

                    ui->marketHistoryTableWidget->setItem(rowCount,MARKETHISTORY_HEADER_TIME,           new QTableWidgetItem(datetime.toString("yyyy-MM-dd hh:mm:ss")));

                    //Color Sell vs Buy
                    QString ordertype = exchangeService.marketHistory.at(i).value("OrderType").toString();
                    QTableWidgetItem *ordertypeitem = new QTableWidgetItem(ordertype);
                    if(ordertype=="SELL"){
                        ordertypeitem->setTextColor(QColor::fromRgb(255,0,0));
                    }else{
                        ordertypeitem->setTextColor(QColor::fromRgb(0,204,0));//#00CC00
                    }
                    ui->marketHistoryTableWidget->setItem(rowCount,MARKETHISTORY_HEADER_ORDERTYPE,      ordertypeitem);

                    ui->marketHistoryTableWidget->setItem(rowCount,MARKETHISTORY_HEADER_FILLTYPE,       new QTableWidgetItem(exchangeService.marketHistory.at(i).value("FillType").toString()));
                    ui->marketHistoryTableWidget->setItem(rowCount,MARKETHISTORY_HEADER_QUANTITY,       new QTableWidgetItem(exchangeService.marketHistory.at(i).value("Quantity").toString()));
                    ui->marketHistoryTableWidget->setItem(rowCount,MARKETHISTORY_HEADER_BIDASK,         new QTableWidgetItem(exchangeService.marketHistory.at(i).value("Price").toString()));
                    ui->marketHistoryTableWidget->setItem(rowCount,MARKETHISTORY_HEADER_TOTAL,          new QTableWidgetItem(exchangeService.marketHistory.at(i).value("Total").toString()));
                }
            }else{
                qDebug()<<"Failed To Get Market History";
            }
            break;
        }
        case TAB_ORDERBOOK:
        {
            exchange exchangeService(this);
            exchangeService.selectedExchange = selectedExchange;

            //Buy Table
            ui->orderBookBuyTableWidget->setRowCount(0);//clear the table out to redraw.
            ui->orderBookSellTableWidget->setRowCount(0);//clear the table out to redraw.

            bool worked = exchangeService.getOrderBook();
            if(worked){
                //BUY ORDER BOOK
                for(int i=0;i<exchangeService.buyOrderBook.size();i++){
                    //Insert row at end
                    int rowCount = ui->orderBookBuyTableWidget->rowCount();
                    ui->orderBookBuyTableWidget->insertRow(rowCount);

                    ui->orderBookBuyTableWidget->setItem(rowCount,ORDERBOOK_HEADER_QUANTITY,        new QTableWidgetItem(exchangeService.buyOrderBook.at(i).value("Quantity").toString()));
                    ui->orderBookBuyTableWidget->setItem(rowCount,ORDERBOOK_HEADER_RATE,            new QTableWidgetItem(exchangeService.buyOrderBook.at(i).value("Rate").toString()));
                    ui->orderBookBuyTableWidget->setItem(rowCount,ORDERBOOK_HEADER_TOTAL,           new QTableWidgetItem(exchangeService.buyOrderBook.at(i).value("Total").toString()));

                }
                //SELL ORDER BOOK
                for(int i=0;i<exchangeService.sellOrderBook.size();i++){
                    //Insert row at end
                    int rowCount = ui->orderBookSellTableWidget->rowCount();
                    ui->orderBookSellTableWidget->insertRow(rowCount);

                    ui->orderBookSellTableWidget->setItem(rowCount,ORDERBOOK_HEADER_QUANTITY,        new QTableWidgetItem(exchangeService.sellOrderBook.at(i).value("Quantity").toString()));
                    ui->orderBookSellTableWidget->setItem(rowCount,ORDERBOOK_HEADER_RATE,            new QTableWidgetItem(exchangeService.sellOrderBook.at(i).value("Rate").toString()));
                    ui->orderBookSellTableWidget->setItem(rowCount,ORDERBOOK_HEADER_TOTAL,           new QTableWidgetItem(exchangeService.sellOrderBook.at(i).value("Total").toString()));
                }
            }else{
                qDebug()<<"Failed To Get Order Book";
            }


            break;
        }
    } // End Switch
}

void MainWindow::on_openOrdersTableWidget_cellClicked(int row, int column)
{
    if(column==OPENORDERS_HEADER_CANCEL){

        QString cancelText = ui->openOrdersTableWidget->item(row,OPENORDERS_HEADER_CANCEL)->text();

        if(cancelText!="Canceling..."){
            QString uuid = ui->openOrdersTableWidget->item(row,OPENORDERS_HEADER_UUID)->text();
            qDebug()<<uuid;

            QMessageBox::StandardButton reply;
            QString ordertext = "Are you sure you want to cancel this order: ( ";
            ordertext.append(uuid);
            ordertext.append(" ) ?");
            reply = QMessageBox::question(this, "Cancel?",ordertext , QMessageBox::Yes|QMessageBox::No);
            if (reply == QMessageBox::Yes) {

                exchange exchangeService(this);
                exchangeService.selectedExchange = selectedExchange;//Set exchange to bittrex.

                bool worked = exchangeService.cancel(apikey,apisecret,uuid);
                if(worked){
                    ui->openOrdersTableWidget->setItem(row,OPENORDERS_HEADER_CANCEL, new QTableWidgetItem("Canceling..."));
                }else{
                    qDebug()<<"Failed to cancel order!";
                }
            } else {
                qDebug() << "Aborting Cancel...";
            }
        }

    }
}

void MainWindow::on_buyOrderTypeComboBox_currentIndexChanged(const QString &arg1)
{
    if(arg1=="Market"){
        ui->bid_comboBox->setEnabled(false);
        ui->bid_lineEdit->setEnabled(false);
        buyMarketActive = true;
    }else{
        ui->bid_comboBox->setEnabled(true);
        ui->bid_lineEdit->setEnabled(true);
        buyMarketActive = false;
    }

    if(arg1 == "Conditional"){
        ui->buyWhenPriceLabel->show();
        ui->buyWhenPriceComboBox->show();
        ui->buyWhenPriceLineEdit->show();
        ui->buyWhenPriceBTCLabel->show();
    }else{
        ui->buyWhenPriceLabel->hide();
        ui->buyWhenPriceComboBox->hide();
        ui->buyWhenPriceLineEdit->hide();
        ui->buyWhenPriceBTCLabel->hide();
    }
}

void MainWindow::on_sellMaxButton_toggled(bool checked)
{
    QVariant number;
    number = ui->sellAskLineEdit->text();

    if(checked){
        sellMaxActive = true;
        ui->sellUnitsLineEdit->setText(cloakAvailable.toString());
    }else{
        sellMaxActive = false;
    }
    //calculateTotalBuyPrice();
}

void MainWindow::on_sellAskComboBox_currentIndexChanged(const QString &arg1)
{
    QVariant number;

    if(arg1 == "Price"){
        number = ui->sellAskLineEdit->text();
    }else if(arg1=="Ask"){
        number = askprice;
        ui->sellAskLineEdit->setText(number.toString());
    }else if(arg1=="Last"){
        number = lastprice;
        ui->sellAskLineEdit->setText(number.toString());
    }else if(arg1=="Bid"){
        number = bidprice;
        ui->sellAskLineEdit->setText(number.toString());
    }

    if(sellMaxActive){
        ui->sellUnitsLineEdit->setText(cloakAvailable.toString());
    }
    calculateTotalSellPrice();
}

void MainWindow::on_sellAskLineEdit_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1);

    if(sellMaxActive){
        ui->sellUnitsLineEdit->setText(cloakAvailable.toString());
    }
    calculateTotalSellPrice();
    int index = ui->sellAskComboBox->findText("Price");
    if(index > -1)
        ui->sellAskComboBox->setCurrentIndex(index);
}

void MainWindow::on_sellOrderTypeComboBox_currentIndexChanged(const QString &arg1)
{
    if(arg1=="Market"){
        ui->sellAskComboBox->setEnabled(false);
        ui->sellAskLineEdit->setEnabled(false);
        buyMarketActive = true;
    }else{
        ui->sellAskComboBox->setEnabled(true);
        ui->sellAskLineEdit->setEnabled(true);
        buyMarketActive = false;
    }

    if(arg1 == "Conditional"){
        ui->sellWhenPriceIsLabel->show();
        ui->sellWhenPriceIsComboBox->show();
        ui->sellWhenPriceIsLineEdit->show();
        ui->sellWhenPriceIsBTCLabel->show();
    }else{
        ui->sellWhenPriceIsLabel->hide();
        ui->sellWhenPriceIsComboBox->hide();
        ui->sellWhenPriceIsLineEdit->hide();
        ui->sellWhenPriceIsBTCLabel->hide();
    }
}

void MainWindow::on_sellUnitsLineEdit_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1);

    if(sellMaxActive){
        ui->sellUnitsLineEdit->setText(cloakAvailable.toString());
    }
    calculateTotalSellPrice();
}

void MainWindow::on_sellCloakCoinButton_clicked()
{
    if(validateSell()){
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Sell?", "Are you sure you want to sell?", QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {

            exchange exchangeService(this);
            exchangeService.selectedExchange = selectedExchange;

            QString quantity    = ui->sellUnitsLineEdit->text();
            QString rate        = ui->sellAskLineEdit->text();

            if(ui->sellOrderTypeComboBox->currentText()=="Limit"){
                bool worked = exchangeService.sellLimit(apikey,apisecret,quantity,rate);

                if(worked){
		    QMessageBox::information(this, "Sell Order (Limit) Placed Successfully", "Success");
                    qDebug()<<"Sell Order (Limit) Placed Succesfully!";
                }else{
		    QMessageBox::information(this, "Sell Order (Limit) Failed", "Sell Failed");
                    qDebug()<<"Sell Order (Limit) Failed";
                }
            }
            if(ui->sellOrderTypeComboBox->currentText()=="Market"){
                bool worked = exchangeService.sellMarket(apikey,apisecret,quantity);

                if(worked){
		    QMessageBox::information(this, "Sell Order (Market) Placed Successfully", "Sell Placed Successfully");
                    qDebug()<<"Sell Order (Market) Placed Succesfully!";
                }else{
		    QMessageBox::information(this, "Sell Order (Market) Failed", "Sell Failed");
                    qDebug()<<"Sell Order (Market) Failed";
                }
            }
        } else {
            qDebug() << "Sell request canceled...";
        }
    }else{
        qDebug()<<"Sell request invalid!";
    }
}

void MainWindow::on_withdrawBalanceToWalletButton_clicked()
{
    QString withdrawAddress = ui->balanceBTCWithdrawAddressLineEdit->text();
    QString amount = ui->balanceBTCWithdrawAmountLineEdit->text();

    exchange exchangeService(this);
    exchangeService.selectedExchange = selectedExchange;

    QMessageBox::StandardButton reply;
    QString withdrawMessage = "Are you sure you want to withdraw \n(";
    withdrawMessage.append(amount);
    withdrawMessage.append(" CLOAK )\nTo Address:\n");
    withdrawMessage.append(withdrawAddress);
    withdrawMessage.append("\n\nThis CAN NOT be undone.");

    reply = QMessageBox::question(this, "Withdraw?", withdrawMessage, QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        bool worked = exchangeService.withdraw(apikey,apisecret,withdrawAddress,amount);
        if(worked){
	    QMessageBox::information(this, "Withdraw Successful", "Withdraw Successful");
            qDebug()<<"Withdraw Succesfull!";
            verifyExchange();//will display updated balance~
            updateWithdrawHistory();//update withdraw history.
        }else{
	    QMessageBox::information(this, "Withdraw Failed", "Withdraw Failed");
            qDebug()<<"Withdraw Failed!";
        }
    }else{
        QMessageBox::information(this, "Withdraw cancelled", "Withdraw cancelled");
        qDebug()<<"Withdraw Cancelled!";
    }
}
