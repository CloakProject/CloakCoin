#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "httpsocket.h"
#include "tickertimer.h"
#include "QJsonValue.h"
#include "QJsonDocument.h"
#include "QJsonArray.h"
#include "QJsonObject.h"
#include "QJsonParseError.h"
#include "jsonsaverloader.h"
#include <QMessageBox>
#include <QErrorMessage>

#define TAB_API             0
#define TAB_BUY             1
#define TAB_SELL            2
#define TAB_OPENORDERS      3
#define TAB_ORDERHISTORY    4
#define TAB_ORDERBOOK       5
#define TAB_MARKETHISTORY   6
#define TAB_BALANCE         7

#define ORDERHISTORY_HEADER_TIME               0
#define ORDERHISTORY_HEADER_TYPE               1
#define ORDERHISTORY_HEADER_BIDASK             2
#define ORDERHISTORY_HEADER_COMMISSION         3
#define ORDERHISTORY_HEADER_COSTPROCEEDS       4
#define ORDERHISTORY_HEADER_QUANTITY           5
#define ORDERHISTORY_HEADER_QUANTITYREMAINING  6

#define OPENORDERS_HEADER_TIME                  0
#define OPENORDERS_HEADER_TYPE                  1
#define OPENORDERS_HEADER_BIDASK                2
#define OPENORDERS_HEADER_QUANTITY              3
#define OPENORDERS_HEADER_QUANTITYREMAINING     4
#define OPENORDERS_HEADER_UUID                  5
#define OPENORDERS_HEADER_CANCEL                6

#define BALANCE_HEADER_TIME     0
#define BALANCE_HEADER_AMOUNT   1
#define BALANCE_HEADER_ADDRESS  2

#define MARKETHISTORY_HEADER_TIME           0
#define MARKETHISTORY_HEADER_ORDERTYPE      1
#define MARKETHISTORY_HEADER_FILLTYPE       2
#define MARKETHISTORY_HEADER_QUANTITY       3
#define MARKETHISTORY_HEADER_BIDASK         4
#define MARKETHISTORY_HEADER_TOTAL          5

#define ORDERBOOK_HEADER_QUANTITY           0
#define ORDERBOOK_HEADER_RATE               1
#define ORDERBOOK_HEADER_TOTAL              2

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public:
    void stopUpdaterTimer();
    void verifyExchange();
    void updateWithdrawHistory();
    void calculateMaxBuyUnits(double price);
    void calculateTotalBuyPrice();
    void calculateTotalSellPrice();
    bool validateBuy();
    bool validateSell();
    void testWorking();
    void initializeCloakTrade();

private slots:
    void tickerResponseFinished();

//    void on_actionView_Exchange_triggered();

//    void on_actionCloak_Send_Coins_triggered();

    void on_saveButton_clicked();

    void on_checkButton_clicked();

    void tickerTimerUpdate();

    void on_checkBox_toggled(bool checked);

    void on_units_maxButton_toggled(bool checked);

    void on_bid_comboBox_currentIndexChanged(const QString &arg1);

    void on_buy_cloakbutton_clicked();

    void on_bid_lineEdit_textChanged(const QString &arg1);

    void on_bid_lineEdit_textEdited(const QString &arg1);

    void on_units_maxLineEdit_textEdited(const QString &arg1);

    void on_updateBalanceButton_clicked();

    void on_exchangeWidget_currentChanged(int index);

    void on_openOrdersTableWidget_cellClicked(int row, int column);

    void on_buyOrderTypeComboBox_currentIndexChanged(const QString &arg1);

    void on_sellMaxButton_toggled(bool checked);

    void on_sellAskComboBox_currentIndexChanged(const QString &arg1);

    void on_sellAskLineEdit_textEdited(const QString &arg1);

    void on_sellOrderTypeComboBox_currentIndexChanged(const QString &arg1);

    void on_sellUnitsLineEdit_textEdited(const QString &arg1);

    void on_sellCloakCoinButton_clicked();

    void on_withdrawBalanceToWalletButton_clicked();

private:
    Ui::MainWindow *ui;
    httpsocket *socket;
    httpsocket *tickerSocket;
    tickertimer *theTickerTimer;
    QVariant bidprice;
    QVariant askprice;
    QVariant lastprice;
    bool useTickertimer;
    bool apiverified;
    QString apikey;
    QString apisecret;
    QVariant btcAvailable;
    QVariant cloakAvailable;
    bool buyMaxActive;
    bool sellMaxActive;
    QVariant totalbuyprice;
    QVariant totalsellprice;
    bool buyMarketActive;
    bool sellMarketActive;
    int selectedExchange;
    bool isInitialized;
    bool isTickerUpdating;
};

#endif // MAINWINDOW_H
