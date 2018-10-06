/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"
#include "bitcoinrpc.h"
#include "cloaksend.h"
#include "mainwindow.h"
#include "enigmastatuspage.h"
#include "enigmatablemodel.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#include <QDesktopServices>
#include <QTimer>
#include <QDragEnterEvent>
#include <QUrl>
#include <QStyle>
#include <QFontDatabase>

#include <iostream>

extern CWallet *pwalletMain;
extern int64 nLastCoinStakeSearchInterval;
extern unsigned int nStakeTargetSpacing;
extern bool fWalletUnlockMintOnly;
extern int testnetNumber;
extern bool lightCloakShieldIcon;

// stats for status bar
extern int nCloakerCountAccepted;
extern int nCloakerCountSigned;
extern int nCloakerCountExpired;
extern int nCloakerCountRefused;
extern int nCloakerCountCompleted;

#define SHOW_CLOAKING_ICONS false

BitcoinGUI::BitcoinGUI(bool fIsTestnet, QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    lockWalletToggleAction(0),
    enigmaEnabledToggleAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0)
{
    resize(850, 550);
    setWindowTitle(tr("CLOAK Core v2.2.2.0 rEvolution"));
#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
    // Accept D&D of URIs
    setAcceptDrops(true);

    setObjectName("cloakWallet");
    /*
    setStyleSheet("#cloakWallet { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 #eeeeee, stop:1.0 #fefefe); } "
                  "QToolTip { color: #cecece; background-color: #333333;  border:0px;} ");
                  */

    setStyleSheet("#cloakWallet { } "
                  "QToolTip { color: #cecece; background-color: #333333;  border:0px;} ");

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create tabs
    overviewPage = new OverviewPage();

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(this);

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    cloakTradePage  = new MainWindow(this);

    enigmaStatusPage = new EnigmaStatusPage(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    centralWidget->addWidget(cloakTradePage);
    centralWidget->addWidget(enigmaStatusPage);

    setCentralWidget(centralWidget);

    // Create status bar
    statusBar();

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    labelCloakShieldIcon = new QLabel();
    labelPosaIcon = new QLabel();
    labelEncryptionIcon = new QLabel();
    labelMintingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();

    if (SHOW_CLOAKING_ICONS)
    {
        labelCloakerAcceptedIcon = new QLabel();
        labelCloakerSignedIcon = new QLabel();
        labelCloakerRefusedIcon = new QLabel();
        labelCloakerExpiredIcon = new QLabel();
        //labelCloakerCompletedIcon = new QLabel();

        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelCloakerAcceptedIcon);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelCloakerSignedIcon);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelCloakerRefusedIcon);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelCloakerExpiredIcon);
        frameBlocksLayout->addStretch();
    }
    //frameBlocksLayout->addWidget(labelCloakerCompletedIcon);

    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelPosaIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelCloakShieldIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelMintingIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    labelCloakShieldIcon->setObjectName("labelCloakShieldIcon");
    labelEncryptionIcon->setObjectName("labelEncryptionIcon");
    labelConnectionsIcon->setObjectName("labelConnectionsIcon");
    labelBlocksIcon->setObjectName("labelBlocksIcon");
    labelPosaIcon->setObjectName("labelPosaIcon");
    labelMintingIcon->setObjectName("labelMintingIcon");

    if (SHOW_CLOAKING_ICONS)
    {
        labelCloakerAcceptedIcon->setObjectName("labelCloakerAcceptedIcon");
        labelCloakerSignedIcon->setObjectName("labelCloakerSignedIcon");
        labelCloakerRefusedIcon->setObjectName("labelCloakerRefusedIcon");
        labelCloakerExpiredIcon->setObjectName("labelCloakerExpiredIcon");
    }
    //labelCloakerCompletedIcon->setObjectName("labelCloakerCompletedIcon");

    labelCloakShieldIcon->setStyleSheet("#labelCloakShieldIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelPosaIcon->setStyleSheet("#labelPosaIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelEncryptionIcon->setStyleSheet("#labelEncryptionIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelConnectionsIcon->setStyleSheet("#labelConnectionsIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelBlocksIcon->setStyleSheet("#labelBlocksIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelMintingIcon->setStyleSheet("#labelMintingIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");

    if (SHOW_CLOAKING_ICONS)
    {
        labelCloakerAcceptedIcon->setStyleSheet("#labelCloakerAcceptedIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
        labelCloakerSignedIcon->setStyleSheet("#labelCloakerSignedIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
        labelCloakerRefusedIcon->setStyleSheet("#labelCloakerRefusedIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
        labelCloakerExpiredIcon->setStyleSheet("#labelCloakerExpiredIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    }
    //labelCloakerCompletedIcon->setStyleSheet("#labelCloakerCompletedIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");

    // cloaker info
    int id = QFontDatabase::addApplicationFont(":/fonts/font_cloak");
    QString cloakFontFamily = QFontDatabase::applicationFontFamilies(id).at(0);
    GUIUtil::TexImageMaker::SetFont(QFont(cloakFontFamily, 8));

    if (SHOW_CLOAKING_ICONS)
    {
        QPixmap pm1 = GUIUtil::TexImageMaker::MakeTextImageIcon(QIcon(":/icons/cloakerAccepted").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE), "25", 0, 0, true, true );
        labelCloakerAcceptedIcon->setPixmap(pm1);
        labelCloakerAcceptedIcon->setEnabled(false);
        labelCloakerSignedIcon->setPixmap(QIcon(":/icons/cloakerSigned").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelCloakerSignedIcon->setEnabled(false);
        labelCloakerRefusedIcon->setPixmap(QIcon(":/icons/cloakerRefused").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelCloakerRefusedIcon->setEnabled(false);
        labelCloakerExpiredIcon->setPixmap(QIcon(":/icons/cloakerExpired").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelCloakerExpiredIcon->setEnabled(false);
    }
    //labelCloakerCompletedIcon->setPixmap(QIcon(":/icons/cloakerCompleted").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    //labelCloakerCompletedIcon->setEnabled(false);

    // Set shield pixmap
    labelCloakShieldIcon->setPixmap(QIcon(":/icons/shield").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelCloakShieldIcon->setEnabled(false);

    // Set posa pixmap
    labelPosaIcon->setPixmap(QIcon(":/icons/posa").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelPosaIcon->setEnabled(false);

    // Set minting pixmap
    labelMintingIcon->setPixmap(QIcon(":/icons/minting").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelMintingIcon->setEnabled(false);
    // Add timer to update minting icon
    QTimer *timerMintingIcon = new QTimer(labelMintingIcon);
    timerMintingIcon->start(MODEL_UPDATE_DELAY);
    connect(timerMintingIcon, SIGNAL(timeout()), this, SLOT(updateMintingIcon())); // will also update posa icon
    // Add timer to update minting weights
    QTimer *timerMintingWeights = new QTimer(labelMintingIcon);
    timerMintingWeights->start(30 * 1000);
    connect(timerMintingWeights, SIGNAL(timeout()), this, SLOT(updateMintingWeights()));
    // Set initial values for user and network weights
    nWeight=0;
    nNetworkWeight = 0;

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBarLabel->setObjectName("progressBarLabel");
    progressBarLabel->setStyleSheet("#progressBarLabel {color:#ffffff;}");
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = qApp->style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8c5c5; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);
    statusBar()->setObjectName("cloakStatusBar");
    statusBar()->setStyleSheet("#cloakStatusBar { border-top-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #4B2F32, stop:0.5 #8B4F62, stop:1.0 #8B4F62); "
                               "border-top-width: 2px; border-top-style: inset; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #663333, stop:0.5 #321616, stop:1.0 #110000); "
                               "background-image: url(:images/shadowbar); background-repeat: repeat-x; background-position: bottom center; color: #ff9999; } "
                               "QToolTip { color: #ff9999; background-color: #331515; border-width: 1px;border-color:#CA0D0D;}"
                               "QStatusBar::item { border: 0px solid black };");

    syncIconMovie = new QMovie(":/movies/update_spinner", "mng", this);
    // this->setStyleSheet("background-color: #effbef;");

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    gotoOverviewPage();

    guiLoaded = false;
}

BitcoinGUI::~BitcoinGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif
}

void BitcoinGUI::resizeEvent(QResizeEvent *)
{
    QString path = QDir::currentPath().append(":/images/background.png");
    //QPixmap bkgnd(path);
    QPixmap bkgnd(":/images/background");
    bkgnd = bkgnd.scaled(this->size(), Qt::IgnoreAspectRatio);
    QPalette palette;
    palette.setBrush(QPalette::Background, bkgnd);
    this->setPalette(palette);
}

void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendCoinsAction->setToolTip(tr("Send coins to a CloakCoin address"));
    sendCoinsAction->setStatusTip(tr("Send coins to a CloakCoin address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive coins"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setStatusTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setStatusTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);

    enigmaStatusAction = new QAction(QIcon(":/icons/posaTabIcon"), tr("&Enigma Stats"), this);
    enigmaStatusAction->setToolTip(tr("Browse Enigma Cloaking operation history"));
    enigmaStatusAction->setStatusTip(tr("Browse Enigma Cloaking operation history"));
    enigmaStatusAction->setCheckable(true);
    tabGroup->addAction(enigmaStatusAction);

    /*
    openCloakTradeAction = new QAction(QIcon(":/icons/exchange"), tr("&Cloak Trade"), this);
    openCloakTradeAction->setToolTip(tr("Open the CloakTrade Exchange Interface"));
    openCloakTradeAction->setStatusTip(tr("Open the CloakTrade Exchange Interface"));
    openCloakTradeAction->setCheckable(true);
    tabGroup->addAction(openCloakTradeAction);
    */

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));
    //connect(openCloakTradeAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    //connect(openCloakTradeAction, SIGNAL(triggered()), this, SLOT(gotoCloakTrade()));
    connect(enigmaStatusAction, SIGNAL(triggered()), this, SLOT(gotoEnigmaStatusPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About CloakCoin"), this);
    aboutAction->setToolTip(tr("Show information about CloakCoin"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for CloakCoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    lockWalletToggleAction = new QAction(QIcon(":/icons/lock_open"), tr("Unlock for Minting"), this);

    enigmaEnabledToggleAction = new QAction(QIcon(":/icons/lock_open"), tr("Enable Engima"), this);
    enigmaEnabledToggleAction->setToolTip(tr("Enable Enigma Sending and Cloaking functionality"));

    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);

    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));

    unlockMintingAction = new QAction(tr("Unlock Minting Only"), this);
    unlockMintingAction->setToolTip(tr("Unlock the wallet for Cloaking and minting"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(lockWalletToggleAction, SIGNAL(triggered()), this, SLOT(lockWalletToggle()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
    connect(unlockMintingAction, SIGNAL(triggered()), this, SLOT(lockWalletToggle()));
    connect(enigmaEnabledToggleAction, SIGNAL(triggered()), this, SLOT(enigmaEnabledToggle()));
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addAction(lockWalletToggleAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);

    // QString ss("QMenuBar::item { background-color: #effbef; color: black }");
    // appMenuBar->setStyleSheet(ss);
}

void BitcoinGUI::createToolBars()
{
    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));

    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
    toolbar->addAction(enigmaStatusAction);
    toolbar->addAction(enigmaEnabledToggleAction);



#if ONEMARKET_ENABLED == 1
    toolbar->addAction(offerListAction);
    toolbar->addAction(openCloakTradeAction);
#endif

    toolbar->addAction(lockWalletToggleAction);
    toolbar->addAction(exportAction);
    toolbar->setObjectName("tabsToolbar");
    toolbar->setStyleSheet("QToolButton { min-height:48px;color:#ffffff;border:none;margin:0px;padding:0px;} "
                           "QToolButton:hover { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #8E4c4c, stop:1.0 #351413); margin:0px; padding:0px; border:none; }"
                           "QToolButton:checked { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #472324, stop:1.0 #351413); margin:0px; padding:0px; border-right-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #3b1919, stop:0.5 #6c3636, stop:1.0 #6c3636);border-right-width:2px;border-right-style:inset; border-left-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #6c3539, stop:0.5 #6c3539, stop:1.0 #3b1616);border-left-width:2px;border-left-style:inset; }"
                           "QToolButton:pressed { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #472324, stop:1.0 #351413); margin:0px; padding:0px; border:none;}"
                           "QToolButton:selected { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #472324, stop:1.0 #351413); margin:0px;padding:0px;border:none; }"
                           "#tabsToolbar { min-height:48px; color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #6e3737, stop:1.0 #4e2b2b); margin:0px; padding:0px; border-top-color: rgba(160, 80, 80, 191); border-top-width: 1px; border-top-style: inset; }"
                           "QToolBar::handle { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #6e3b30, stop:1.0 #4e2b2c); }");
}

void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        QString instanceText = QString("[%1]").arg(instanceNumber);

        if (clientModel->isTestNet())
        {
            if (testnetNumber == 5)
                setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]") + instanceText);
            else
                setWindowTitle(windowTitle() + QString(" ") + tr("[devnet]") + instanceText);
        }else{
#ifdef QT_DEBUG
           // setWindowTitle(windowTitle() + QString(" ") + tr("[live]") + instanceText);
#endif
        }

        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("CloakCoin client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        rpcConsole->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if(walletModel)
    {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);

        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);

        // set posa page model
        enigmaStatusPage->setModel(walletModel->getEnigmaTableModel());

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

void BitcoinGUI::createTrayIcon()
{
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("CloakCoin client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
        this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    trayIconMenu = dockIconHandler->dockMenu();
    dockIconHandler->setMainWindow((QMainWindow*)this);
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon);
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to CloakCoin network.", "", count));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty())
        {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    }
    else
    {
        if (strStatusBarWarnings.isEmpty())
            progressBarLabel->setVisible(false);

        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    // Override progressBarLabel text and hide progress bar, when we have warnings to display
    if (!strStatusBarWarnings.isEmpty())
    {
        progressBarLabel->setText(strStatusBarWarnings);
        progressBarLabel->setVisible(true);
        progressBar->setVisible(false);
    }

    tooltip = tr("Current difficulty is %1.").arg(clientModel->GetDifficulty()) + QString("<br>") + tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
        text = tr("%n second(s) ago","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago","",secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
    fSyncing = false;
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        fSyncing = true;
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();

        overviewPage->showOutOfSyncWarning(true);
    }

    if(!text.isEmpty())
    {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
            !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

void BitcoinGUI::decryptWallet()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::DecryptOnStart);
    if (dlg.exec() != QDialog::Accepted)
        qApp->quit();
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
        "which goes to the nodes that process your transaction and helps to support the network.  "
        "Do you want to pay the fee?").arg(
        BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
        this, tr("Confirm transaction fee"), strMessage,
        QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
        .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
            .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
            .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
            .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
            TransactionTableModel::ToAddress, parent)
            .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
            (amount)<0 ? tr("Sent transaction") :
            tr("Incoming transaction"),
            tr("Date: %1\n"
            "Amount: %2\n"
            "Type: %3\n"
            "Address: %4\n")
            .arg(date)
            .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
            .arg(type)
            .arg(address), icon);
  }
}

void BitcoinGUI::gotoCloakTrade()
{
    openCloakTradeAction->setChecked(true);
    centralWidget->setCurrentWidget(cloakTradePage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);

    //cloakTradePage->initializeCloakTrade();
}


void BitcoinGUI::gotoOneMarket()
{
#if ONEMARKET_ENABLED == 1
    openOneMarketAction->setChecked(true);
    centralWidget->setCurrentWidget(oneMarketPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);

    oneMarketPage->initializeOneMarket();
#endif
}

void BitcoinGUI::gotoEnigmaStatusPage()
{
    enigmaStatusAction->setChecked(true);
    centralWidget->setCurrentWidget(enigmaStatusPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            if (sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            gotoSendCoinsPage();
        else
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid CloakCoin address or malformed URI parameters."));
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    }
    else
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid CloakCoin address or malformed URI parameters."));
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        encryptWalletAction->setEnabled(true);
        changePassphraseAction->setEnabled(false);
        lockWalletToggleAction->setText(tr("&Unlock For Minting"));
        lockWalletToggleAction->setToolTip(tr("You must <b>encrypt</b> your wallet first, before you can Lock/Unlock wallet for minting"));
        lockWalletToggleAction->setEnabled(false);
        //unlockMintingAction->setEnabled(false);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>."));
        encryptWalletAction->setChecked(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        changePassphraseAction->setEnabled(true);
        lockWalletToggleAction->setVisible(true);
        lockWalletToggleAction->setIcon(QIcon(":/icons/lock_closed"));
        lockWalletToggleAction->setText(tr("&Lock Wallet"));
        lockWalletToggleAction->setToolTip(tr("Lock wallet"));
        //unlockMintingAction->setEnabled(false);
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>."));
        encryptWalletAction->setChecked(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        changePassphraseAction->setEnabled(true);
        lockWalletToggleAction->setVisible(true);
        lockWalletToggleAction->setIcon(QIcon(":/icons/lock_open"));
        lockWalletToggleAction->setText(tr("&Unlock For Minting"));
        lockWalletToggleAction->setToolTip(tr("Unlock wallet for minting"));
        //unlockMintingAction->setEnabled(true);
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
        AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::backupWallet()
{
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(!filename.isEmpty()) {
        if(!walletModel->backupWallet(filename)) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
}

void BitcoinGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::unlockWallet()
{
    if(!walletModel)
        return;

    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked || fWalletUnlockMintOnly)
    {
        AskPassphraseDialog::Mode mode = AskPassphraseDialog::Unlock;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void BitcoinGUI::enigmaEnabledToggle()
{
    if(!walletModel)
        return;

    fEnigma = !fEnigma;
    setEnigmaGuiEnabled(fEnigma);
}

void BitcoinGUI::lockWalletToggle()
{
    if(!walletModel)
        return;

    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog::Mode mode = AskPassphraseDialog::UnlockMinting;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
    else if(walletModel->getEncryptionStatus() == WalletModel::Unlocked)
    {
        walletModel->setWalletLocked(true);
        fWalletUnlockMintOnly = false;
    }
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if (!guiLoaded)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::updateMintingIcon()
{
    if (fShutdown)
        return;

    int64 nEnigmaNodeCount = 0;

    // todo: need lock? (segfault)
    LOCK(cs_mapEnigmaAnns);
    for (map<uint160, CEnigmaAnnouncement>::iterator mi = mapEnigmaAnns.begin(); mi != mapEnigmaAnns.end(); mi++)
    {
        const CEnigmaAnnouncement& ann = (*mi).second;
        if(!ann.IsMine())
            nEnigmaNodeCount++;
    }

    labelPosaIcon->setPixmap(QIcon(QString(":/icons/posa%1").arg(std::min(nEnigmaNodeCount, (int64)26))).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));

    if (SHOW_CLOAKING_ICONS)
    {
        labelCloakerAcceptedIcon->setToolTip(tr("Accepted %1 Cloaking Requests.").arg(nCloakerCountAccepted));
        labelCloakerAcceptedIcon->setEnabled(nCloakerCountAccepted);

        labelCloakerSignedIcon->setToolTip(tr("Signed %1 Cloaking Requests.").arg(nCloakerCountSigned));
        labelCloakerSignedIcon->setEnabled(nCloakerCountSigned);

        labelCloakerExpiredIcon->setToolTip(tr("%1 Cloaking Requests have timed out.").arg(nCloakerCountExpired));
        labelCloakerExpiredIcon->setEnabled(nCloakerCountExpired);

        labelCloakerRefusedIcon->setToolTip(tr("Refused to sign %1 Cloaking Requests.").arg(nCloakerCountRefused));
        labelCloakerRefusedIcon->setEnabled(nCloakerCountRefused);
    }

    //labelCloakerCompletedIcon->setToolTip(tr("%1 Successful Enigma Cloakings.").arg(nCloakerCountCompleted));
    //labelCloakerCompletedIcon->setEnabled(nCloakerCountCompleted);

    // light cloakshield icon if we have enough nodes to onion route
    lightCloakShieldIcon = nEnigmaNodeCount >= CCloakShield::GetShield()->NumNodesRequired();

    // Update enigma Status
    if(!fEnigma)
    {
        labelPosaIcon->setToolTip(tr("Enigma is disabled."));
        labelPosaIcon->setEnabled(false);
    }
    else if(fSyncing)
    {
        labelPosaIcon->setToolTip(tr("Not connected to Enigma because wallet is syncing."));
        labelPosaIcon->setEnabled(false);
    }
    else if(nEnigmaNodeCount < 1)
    {
        labelPosaIcon->setToolTip(tr("Not connected to Enigma network, <br>less than 1 node available <br>(%1 anons).").arg(nEnigmaNodeCount));
        labelPosaIcon->setPixmap(QIcon(":/icons/posa").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelPosaIcon->setEnabled(false);
    }
    else
    {
        labelPosaIcon->setToolTip(tr("Connected.<br><b>%1</b> anons.<br><b>%2</b> cloakings.").arg(nEnigmaNodeCount).arg(pwalletMain->EnigmaCount()));
        labelPosaIcon->setEnabled(true);
    }

    if (pwalletMain && pwalletMain->IsLocked())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is locked."));
        labelMintingIcon->setEnabled(false);
    }
    else if(!fStaking)
    {
        labelMintingIcon->setToolTip(tr("Not minting because staking is disabled."));
        labelMintingIcon->setEnabled(false);
    }
    else if (vNodes.empty())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is offline."));
        labelMintingIcon->setEnabled(false);
    }
    else if (IsInitialBlockDownload())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is syncing."));
        labelMintingIcon->setEnabled(false);
    }
    else if (!nWeight)
    {
        labelMintingIcon->setToolTip(tr("Not minting because you don't have mature coins."));
        labelMintingIcon->setEnabled(false);
    }
    else if (nLastCoinStakeSearchInterval)
    {
        uint64 nEstimateTime = nStakeTargetSpacing * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }

        labelMintingIcon->setEnabled(true);
        labelMintingIcon->setToolTip(tr("Minting.<br>Your weight is %1.<br>Network weight is %2.<br>Expected time to earn reward is %3.").arg(nWeight).arg(nNetworkWeight).arg(text));
    }
    else
    {
        labelMintingIcon->setToolTip(tr("Not minting."));
        labelMintingIcon->setEnabled(false);
    }

    if (lightCloakShieldIcon)
    {
        labelCloakShieldIcon->setToolTip(tr("CloakShield available."));
    }else{
        labelCloakShieldIcon->setToolTip(tr("CloakShield unavailable."));
    }
    labelCloakShieldIcon->setEnabled(lightCloakShieldIcon);
}

void BitcoinGUI::setEnigmaGuiEnabled(bool enabled)
{
    enigmaEnabledToggleAction->setIcon(enabled ? QIcon(":/icons/lock_closed") : QIcon(":/icons/lock_open"));
    enigmaEnabledToggleAction->setText(enabled ? tr("&Disable Enigma") : tr("&Enable Enigma"));
    enigmaEnabledToggleAction->setToolTip(enabled ? tr("Disable Enigma Sending and Cloaking functionality") : tr("Enable Enigma Sending and Cloaking functionality"));
    sendCoinsPage->setEnigmaEnabled(enabled);
}

void BitcoinGUI::updateMintingWeights()
{
    if (fShutdown)
        return;

    // Only update if we have the network's current number of blocks, or weight(s) are zero (fixes lagging GUI)
    if ((clientModel && clientModel->getNumBlocks() == clientModel->getNumBlocksOfPeers()) || !nWeight || !nNetworkWeight)
    {
        nWeight = 0;

        if (pwalletMain)
            pwalletMain->GetStakeWeight(*pwalletMain, nMinMax, nMinMax, nWeight);

        nNetworkWeight = GetPoSKernelPS();
    }
}
