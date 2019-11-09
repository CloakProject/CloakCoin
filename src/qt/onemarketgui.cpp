#include "onemarketgui.h"
#include "ui_onemarket.h"
#include "conemarketcreatelisting.h"

OneMarket::OneMarket(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::OneMarket)
{
    ui->setupUi(this);
}

OneMarket::~OneMarket()
{
    delete ui;
}

void OneMarket::initializeOneMarket()
{
    qDebug()<<"One Market Initialized!";
    isInitialized = true;
}

void OneMarket::on_createListingsSubmitButton_clicked()
{
    qDebug()<<"qDebug -> Create Listing Push";
    printf("PrintF -> CreateListing Push\n");

   //Search Address Manager
   COneMarketCreateListing createListing;
   createListing.SetMessage(ui->createListingsTitleLineEdit->text().toStdString());

   //Push Create Listing Message To All Of Them!
   LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes){
            qDebug()<<"QDebug() -> Pushed To Connected Node...\n";
            printf("PrintF -> Pushed To Connected Node...\n");
            createListing.RelayTo(pnode);
        }

}
