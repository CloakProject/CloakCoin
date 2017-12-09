#include "enigmastatuspage.h"
#include "ui_enigmastatuspage.h"

#include "enigmatablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "enigma/enigma.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>
#include <QTimer>

// stats for status bar
extern int nCloakerCountAccepted;
extern int nCloakerCountSigned;
extern int nCloakerCountExpired;
extern int nCloakerCountRefused;
extern int nCloakerCountCompleted;
extern int64 nCloakFeesEarnt;

extern map<uint256, CCloakingRequest> mapOurCloakingRequests;
extern CCriticalSection cs_mapOurCloakingRequests;


EnigmaStatusPage::EnigmaStatusPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EnigmaStatusPage),
    model(0),
    optionsModel(0)
{
    ui->setupUi(this);

    // Actions
    contextMenu = new QMenu();

    QAction* cancelSendAction = new QAction(tr("Cancel Enigma Send"), this);
    contextMenu->addAction(cancelSendAction);

    QAction* copyTxAction = new QAction(tr("Copy Tx ID"), this);
    contextMenu->addAction(copyTxAction);

    connect(cancelSendAction, SIGNAL(triggered()), this, SLOT(cancelSend()));
    connect(copyTxAction, SIGNAL(triggered()), this, SLOT(copyTxId()));
    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(ShowContextMenu(QPoint)));

    // Add timer to update cloaking assist info
    QTimer *timerCloakingAssists = new QTimer(this);
    timerCloakingAssists->start(1 * 1000);
    connect(timerCloakingAssists, SIGNAL(timeout()), this, SLOT(updateCloakingAssistCounts()));
}

EnigmaStatusPage::~EnigmaStatusPage()
{
    delete ui;
}

void EnigmaStatusPage::ShowContextMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void EnigmaStatusPage::setModel(EnigmaTableModel *model)
{
    this->model = model;
    if(!model) return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterRole(EnigmaTableModel::TypeRole);
    ui->tableView->setModel(proxyModel);
    //ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    // Set column widths
#if QT_VERSION < 0x050000
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::Type, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::InitiatedOn, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::ParticipantsRequired, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::ParticipantsAcquired, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::ParticipantsSigned, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::ExpiresInMs, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::TxId, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(EnigmaTableModel::ReqId, QHeaderView::Stretch);
#else
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::Type, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::InitiatedOn, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::ParticipantsRequired, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::ParticipantsAcquired, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::ParticipantsSigned, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::ExpiresInMs, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::TxId, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(EnigmaTableModel::ReqId, QHeaderView::Stretch);
#endif

    // hide request column
    ui->tableView->setColumnHidden(8, true);

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    // todo - check these below!
    //connect(model, SIGNAL(dataChanged(int)), this, SLOT(handleData(int)));
    //connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(ShowContextMenu(QPoint)));
}

void EnigmaStatusPage::updateCloakingAssistCounts()
{
    if (fShutdown)
        return;

    ui->labEnigmaAcceptedVal->setText(QString::number(nCloakerCountAccepted));
    ui->labEnigmaRefusedVal->setText(QString::number(nCloakerCountRefused));
    ui->labEnigmaExpiredVal->setText(QString::number(nCloakerCountExpired));
    ui->labEnigmaSignedVal->setText(QString::number(nCloakerCountSigned));
    ui->labEnigmaCompletedVal->setText(QString::number(nCloakerCountCompleted));
    ui->labEnigmaEarntVal->setText(QString("%1 CLOAK").arg(QString::fromStdString(FormatMoney(nCloakFeesEarnt))));
}

void EnigmaStatusPage::setOptionsModel(OptionsModel *optionsModel)
{
    this->optionsModel = optionsModel;
}

void EnigmaStatusPage::handleData(int index)
{
    ui->tableView->update();
}

void EnigmaStatusPage::copyTxId()
{
    GUIUtil::copyEntryData(ui->tableView, EnigmaTableModel::TxId);
}

void EnigmaStatusPage::cancelSend()
{
    QString reqId = GUIUtil::getEntryData(ui->tableView, EnigmaTableModel::ReqId);
    if (reqId.length() > 0)
    {
        uint256 identifier = uint256(reqId.toStdString());
        string ss = identifier.GetHex();
        CCloakingRequest* r;
        {
            typedef std::map<uint256, CCloakingRequest> CloakingMapType;
            LOCK(pwalletMain->cs_mapOurCloakingRequests);
            BOOST_FOREACH(CloakingMapType::value_type p, pwalletMain->mapOurCloakingRequests)
            {
                CCloakingRequest r = p.second;
                //printf("looking for %s found %s\n", identifier.GetHex().c_str(), r.identifier.GetHash().GetHex().c_str());

            }

            if(pwalletMain->mapOurCloakingRequests.count(identifier) > 0)
            {
                r = &pwalletMain->mapOurCloakingRequests[identifier];
                r->Abort();
            }

        }
    }
}
