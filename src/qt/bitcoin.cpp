/*
 * W.J. van der Laan 2011-2012
 */
#include "bitcoingui.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "guiconstants.h"

#include "init.h"
#include "ui_interface.h"
#include "qtipcserver.h"
#include "splashscreen.h"

#include "utilitydialog.h"
#include "winshutdownmonitor.h"

#include "filedownloader.h"

#include <QApplication>
#include <QMessageBox>
#include <QTextCodec>
#include <QLocale>
#include <QTranslator>
#include <QSplashScreen>
#include <QTimer>
#include <QThread>
#include <QLibraryInfo>
#include <QFile>

#include "compat.h"

#include "bitcoin.h"

#include "miniz.c"


#if defined(BITCOIN_NEED_QT_PLUGINS) && !defined(_BITCOIN_QT_PLUGINS_INCLUDED)
#define _BITCOIN_QT_PLUGINS_INCLUDED
#define __INSURE__
#include <QtPlugin>
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#endif

// Need a global reference for the notifications to find the GUI
static BitcoinGUI *guiref;
static QSplashScreen *splashref;
static QFont cloakFont;
static bool downloadingBlockchain;
static boost::filesystem::path dataDir;
static boost::filesystem::path zipPath;
static FILE* fileBcZip;

BitcoinGUI* GuiRef;

BitcoinCore::BitcoinCore():
    QObject()
{
}

void BitcoinCore::handleRunawayException(std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    emit runawayException(QString::fromStdString(strMiscWarning));
}

void BitcoinCore::initialize()
{
    try
    {
        printf("Running AppInit2 in thread\n");
        int rv = AppInit2(threadGroup);
        if(rv)
        {
            /* Start a dummy RPC thread if no RPC thread is active yet
             * to handle timeouts.
             */
            //StartDummyRPCThread();
        }
        emit initializeResult(rv);
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(NULL);
    }
}

void BitcoinCore::shutdown()
{
    try
    {
        printf("Running Shutdown in thread\n");
        threadGroup.interrupt_all();
        threadGroup.join_all();
        Shutdown(0);
        printf("Shutdown finished\n");
        emit shutdownResult(1);
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(NULL);
    }
}

void BitcoinApplication::createOptionsModel()
{
    optionsModel = new OptionsModel();
    optionsModel->Upgrade(); // Must be done after AppInit2
}

void BitcoinApplication::createWalletModel()
{
    clientModel = new ClientModel(optionsModel);
    walletModel = new WalletModel(pwalletMain, optionsModel);

    window->setClientModel(clientModel);
    window->setWalletModel(walletModel);
}

void BitcoinApplication::createWindow(bool isaTestNet)
{
    window = new BitcoinGUI(isaTestNet, 0);
    GuiRef = window;

    // set gui ref for threadsafe messages etc
    guiref = window;
}

int BitcoinApplication::unpackDownloadedBlockchain()
{
    fclose(fileBcZip);
    uiInterface.InitMessage("Extracting blockchain.");
    int res = unzipBlockchain();
    if (res != 0){
        QMessageBox::warning(0, "CloakCoin", QString("Failed to extract blockchain archive (error %1)").arg(res));
    }
    uiInterface.InitMessage("Blockchain extracted!");
    downloadingBlockchain = false;
    return res;
}

void BitcoinApplication::updateDownloadBlockchainProgress(qint64 bytesRead, qint64 bytesTotal)
{
    char xx[2555];
    float pct = ((float)bytesRead/bytesTotal)*100.0f;
    sprintf(xx, "Downloading %.2f%%",pct);

    uiInterface.InitMessage(std::string(xx));
    qApp->processEvents();
}

void BitcoinApplication::gotBytesDownloadBlockchain(const char* data, int dataSize)
{
    fwrite((const void*) data, sizeof(char), dataSize, fileBcZip);
}

int BitcoinApplication::unzipBlockchain()
{
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    mz_bool status = mz_zip_reader_init_file(&zip_archive, zipPath.string().c_str(), 0);

    if (!status)
    {
        return -1;
    }

    int numFiles = (int)mz_zip_reader_get_num_files(&zip_archive);

    if (numFiles == 0)
    {
        return -2;
    }

    void* p;
    size_t uncomp_size;
    char xx[2555];
    float pct;

    for (int i = 0; i < numFiles; i++)
    {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
        {
           mz_zip_reader_end(&zip_archive);
           return -3;
        }

        boost::filesystem::path path = GetDataDir() / file_stat.m_filename;

        if (mz_zip_reader_is_file_a_directory(&zip_archive, i))
        {
            filesystem::create_directory(path.string().c_str());

        }else{

            p = mz_zip_reader_extract_to_heap(&zip_archive, i, &uncomp_size, 0);
            FILE* fileOut = fopen(path.string().c_str(), "wb+");

            if (fileOut == NULL) {
                return (102);
            }

            /*
            int buffer = 1024*100;
            void* p2 = p;
            int offset = 0;
            float fileRatio = 100.0f / numFiles;
            float filePercent = 0.0f;

            while ((uint)p2 < ((uint)p) + uncomp_size)
            {
                fwrite(p2, sizeof(char), uncomp_size, fileOut);
                p2+=buffer;
                filePercent = ((uint)p2 - (uint)p / uncomp_size);
                pct = ((float)i/numFiles)*100.0f;
                //pct += fileRatio * filePercent;
                sprintf(xx, "Extracting %.2f%%",pct);
                uiInterface.InitMessage(std::string(xx));
                QApplication::processEvents();
            }
            */

            fwrite(p, sizeof(char), uncomp_size, fileOut);
            pct = ((float)i/numFiles)*100.0f;
            //pct += fileRatio * filePercent;
            sprintf(xx, "Extracting %.2f%%",pct);
            uiInterface.InitMessage(std::string(xx));
            QApplication::processEvents();

            fclose(fileOut);
            mz_free(p);
        }
    }

    // Close the archive, freeing any resources it was using
    mz_zip_reader_end(&zip_archive);
    return 0;
}

void BitcoinApplication::createSplashScreen(bool isaTestNet)
{
    downloadingBlockchain = false;
    SplashScreen *splash = new SplashScreen(0, isaTestNet);
    splash->show();
    if (!isaTestNet)
    {
        dataDir = GetDataDir(false);
        boost::filesystem::path pathLevelDb = dataDir;
        pathLevelDb /= "txleveldb";

        if (!boost::filesystem::is_directory(pathLevelDb))
        {
            downloadingBlockchain = QMessageBox::Yes == QMessageBox::question(splash, "CloakCoin",
                                  QString("Can't find local blockchain files. Would you like to auto-download them?"), QMessageBox::Yes, QMessageBox::No);

            if (downloadingBlockchain){
                zipPath = dataDir /= "blockchain.zip";
                fileBcZip = fopen(zipPath.string().c_str(), "wb+");
                if (fileBcZip == NULL){
                    QMessageBox::warning(0, "CloakCoin", QString("Failed to open blockchain file for writing: ").append(QString::fromStdString(zipPath.string())));
                }

                uiInterface.InitMessage(_("Downloading blockchain data..."));


                FileDownloader* downloader = new FileDownloader(QUrl("http://82.211.30.193/wallet/v2/cloak_ldb.zip"));

                //FileDownloader* downloader = new FileDownloader(QUrl("https://www.dropbox.com/s/aui31rlxk1s6xy9/blockchain_ldb.zip"));

                //FileDownloader* downloader = new FileDownloader(QUrl("https://backend.cloakcoin.com/wallet/v2/cloak_ldb.zip"));
                connect(downloader, SIGNAL(downloaded()), this, SLOT(unpackDownloadedBlockchain()));
                connect(downloader, SIGNAL(gotBytes(const char*,int)), this, SLOT(gotBytesDownloadBlockchain(const char*, int)));
                connect(downloader, SIGNAL(progressUpdated(qint64, qint64)), this, SLOT(updateDownloadBlockchainProgress(qint64, qint64)));
                //int res = unpackDownloadedBlockchain();
            }
        }
    }
    connect(this, SIGNAL(splashFinished(QWidget*)), splash, SLOT(slotFinish(QWidget*)));
}

void BitcoinApplication::startThread()
{
    coreThread = new QThread(this);
    BitcoinCore *executor = new BitcoinCore();
    executor->moveToThread(coreThread);

    /*  communication to and from thread */
    connect(executor, SIGNAL(initializeResult(int)), this, SLOT(initializeResult(int)));
    connect(executor, SIGNAL(shutdownResult(int)), this, SLOT(shutdownResult(int)));
    connect(executor, SIGNAL(runawayException(QString)), this, SLOT(handleRunawayException(QString)));
    connect(this, SIGNAL(requestedInitialize()), executor, SLOT(initialize()));
    connect(this, SIGNAL(requestedShutdown()), executor, SLOT(shutdown()));
    /*  make sure executor object is deleted in its own thread */
    connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopThread()), coreThread, SLOT(quit()));

    coreThread->start();
}

BitcoinApplication::BitcoinApplication(int &argc, char **argv):
    QApplication(argc, argv),
    coreThread(0),
    optionsModel(0),
    clientModel(0),
    window(0),
    pollShutdownTimer(0),
#ifdef ENABLE_WALLET
    paymentServer(0),
    walletModel(0),
#endif
    returnValue(0)
{
    setQuitOnLastWindowClosed(false);
    startThread();
}

BitcoinApplication::~BitcoinApplication()
{
    printf("Stopping thread\n");
    emit stopThread();
    coreThread->wait();
    printf("Stopped thread\n");

    delete window;
    window = 0;
#ifdef ENABLE_WALLET
    delete paymentServer;
    paymentServer = 0;
#endif
    delete optionsModel;
    optionsModel = 0;
}

static void ThreadSafeMessageBox(const std::string& message, const std::string& caption, int style)
{
    // Message from network thread
    if(guiref)
    {
        bool modal = (style & CClientUIInterface::MODAL);
        // in case of modal message, use blocking connection to wait for user to click OK
        QMetaObject::invokeMethod(guiref, "error",
                                   modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                                   Q_ARG(QString, QString::fromStdString(caption)),
                                   Q_ARG(QString, QString::fromStdString(message)),
                                   Q_ARG(bool, modal));
    }
    else
    {
        printf("%s: %s\n", caption.c_str(), message.c_str());
        fprintf(stderr, "%s: %s\n", caption.c_str(), message.c_str());
    }
}

static bool ThreadSafeAskFee(int64 nFeeRequired, const std::string& strCaption)
{
    if(!guiref)
        return false;
    if(nFeeRequired < MIN_TX_FEE || nFeeRequired <= nTransactionFee || fDaemon)
        return true;
    bool payFee = false;

    QMetaObject::invokeMethod(guiref, "askFee", GUIUtil::blockingGUIThreadConnection(),
                               Q_ARG(qint64, nFeeRequired),
                               Q_ARG(bool*, &payFee));

    return payFee;
}

static void ThreadSafeHandleURI(const std::string& strURI)
{
    if(!guiref)
        return;

    QMetaObject::invokeMethod(guiref, "handleURI", GUIUtil::blockingGUIThreadConnection(),
                               Q_ARG(QString, QString::fromStdString(strURI)));
}

static void InitMessage(const std::string &message)
{
    if(splashref)
    {
        splashref->showMessage(QString::fromStdString(message), Qt::AlignBottom|Qt::AlignHCenter, QColor(70,71,71));
        QApplication::instance()->processEvents();
    }
}

static void QueueShutdown()
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("cloakcoin-core", psz).toStdString();
}


void BitcoinApplication::requestInitialize()
{
    printf("Requesting initialize\n");
    emit requestedInitialize();
}

void BitcoinApplication::requestShutdown()
{
    if (GetBoolArg("-enigma", true)){
        Enigma::CloakShieldShutdown();
    }

    // wait a min before shutting down to give the quit message time for transmission
#ifdef Q_OS_WIN
    Sleep(uint(1000));
#else
    struct timespec ts = { 1, 0};
    nanosleep(&ts, NULL);
#endif

    printf("Requesting shutdown\n");
    window->hide();
    window->setClientModel(0);
    //pollShutdownTimer->stop();

#ifdef ENABLE_WALLET
    window->removeAllWallets();
    delete walletModel;
    walletModel = 0;
#endif
    delete clientModel;
    clientModel = 0;

    // Show a simple window indicating shutdown status
    ShutdownWindow::showShutdownWindow(window);

    // Request shutdown from core thread
    emit requestedShutdown();
}

void BitcoinApplication::initializeResult(int retval)
{
    printf("Initialization result: %i\n", retval);
    // Set exit result: 0 if successful, 1 if failure
    returnValue = retval ? 0 : 1;
    if(retval)
    {
#ifdef ENABLE_WALLET
        PaymentServer::LoadRootCAs();
        paymentServer->setOptionsModel(optionsModel);
#endif

        createOptionsModel();
        createWalletModel();

#ifdef ENABLE_WALLET
        if(pwalletMain)
        {
            walletModel = new WalletModel(pwalletMain, optionsModel);

            window->addWallet("~Default", walletModel);
            window->setCurrentWallet("~Default");

            connect(walletModel, SIGNAL(coinsSent(CWallet*,SendCoinsRecipient,QByteArray)),
                             paymentServer, SLOT(fetchPaymentACK(CWallet*,const SendCoinsRecipient&,QByteArray)));
        }
#endif

        // If -min option passed, start window minimized.
        if(GetBoolArg("-min", false))
        {
            window->showMinimized();
        }
        else
        {
            window->show();
        }
        window->guiLoaded = true;

        window->setEnigmaGuiEnabled(fEnigma);

        // check if we have staked within last X days and warn user if not and disable Enigma
        int secsSinceLastStake = !pwalletMain->SecondsSinceWalletLastStaked();
        if (secsSinceLastStake > 86400 * ENIGMA_MAX_COINAGE_DAYS)
        {
            fEnigma = false;
            window->setEnigmaGuiEnabled(false);
            QMessageBox::warning(0, tr("Staking Warning - Enigma Disabled"),
                tr("You have not staked for %1 days. Please stake and restart the wallet to enable Enigma cloaking.").arg(QString::number(secsSinceLastStake / 86400)),
                QMessageBox::Ok, QMessageBox::Ok);
        }

        emit splashFinished(window);


#ifdef ENABLE_WALLET
        // Now that initialization/startup is done, process any command-line
        // cloak: URIs or payment requests:
        connect(paymentServer, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
                         window, SLOT(handlePaymentRequest(SendCoinsRecipient)));
        connect(window, SIGNAL(receivedURI(QString)),
                         paymentServer, SLOT(handleURIOrFile(QString)));
        connect(paymentServer, SIGNAL(message(QString,QString,unsigned int)),
                         window, SLOT(message(QString,QString,unsigned int)));
        QTimer::singleShot(100, paymentServer, SLOT(uiReady()));
#endif
    } else {
        quit(); // Exit main loop
    }
}

void BitcoinApplication::shutdownResult(int retval)
{
    printf("Shutdown result: %i\n", retval);
    quit(); // Exit main loop after shutdown finished
}

/* Handle runaway exceptions. Shows a message box with the problem and quits the program.
 */
void BitcoinApplication::handleRunawayException(const QString &message)
{
    QMessageBox::critical(0, "Runaway exception", BitcoinGUI::tr("A fatal error occurred. Cloak can no longer continue safely and will quit.") + QString("\n\n") + message);
    ::exit(1);
}

WId BitcoinApplication::getMainWinId() const
{
    if (!window)
        return 0;

    return window->winId();
}

#ifndef BITCOIN_QT_TEST
int main(int argc, char *argv[])
{
    SetupEnvironment();

    // Do this early as we don't want to bother initializing if we are just calling IPC
    ipcScanRelay(argc, argv);

    // Internal string conversion is all UTF-8
    //QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    //QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());

    Q_INIT_RESOURCE(bitcoin);
    BitcoinApplication app(argc, argv);

    // Install global event filter that makes sure that long tooltips can be word-wrapped
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));

    // Command-line options take precedence:
    ParseParameters(argc, argv);

    if (mapArgs.count("-conf")){
        // read conf file first
        ReadConfigFile(mapArgs, mapMultiArgs);
    }

    // ... then bitcoin.conf:
    if (!boost::filesystem::is_directory(GetDataDir(false)))
    {
        // This message can not be translated, as translation is not initialized yet
        // (which not yet possible because lang=XX can be overridden in bitcoin.conf in the data directory)
        QMessageBox::critical(0, "CloakCoin",
                              QString("Error: Specified data directory \"%1\" does not exist.").arg(QString::fromStdString(mapArgs["-datadir"])));
        return 1;
    }

    if (mapArgs.count("-conf") == 0){
        // read conf file last
        ReadConfigFile(mapArgs, mapMultiArgs);
    }


    bool isaTestNet = fTestNet = GetBoolArg("-testnet");

    // Application identification (must be set before OptionsModel is initialized,
    // as it is used to locate QSettings)
    app.setOrganizationName("CloakCoin");
    app.setOrganizationDomain("CloakCoin.su");
    //if(GetBoolArg("-testnet")) // Separate UI settings for testnet
    if(fTestNet)
        app.setApplicationName("CloakCoin-Qt-testnet");
    else
        app.setApplicationName("CloakCoin-Qt");

    // Get desired locale (e.g. "de_DE") from command line or use system locale
    QString lang_territory = QString::fromStdString(GetArg("-lang", QLocale::system().name().toStdString()));
    QString lang = lang_territory;
    // Convert to "de" only by truncating "_DE"
    lang.truncate(lang_territory.lastIndexOf('_'));

    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslatorBase);

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // Load e.g. bitcoin_de.qm (shortcut "de" needs to be defined in bitcoin.qrc)
    if (translatorBase.load(lang, ":/translations/"))
        app.installTranslator(&translatorBase);

    // Load e.g. bitcoin_de_DE.qm (shortcut "de_DE" needs to be defined in bitcoin.qrc)
    if (translator.load(lang_territory, ":/translations/"))
        app.installTranslator(&translator);

    // Subscribe to global signals from core
    uiInterface.ThreadSafeMessageBox.connect(ThreadSafeMessageBox);
    uiInterface.ThreadSafeAskFee.connect(ThreadSafeAskFee);
    uiInterface.ThreadSafeHandleURI.connect(ThreadSafeHandleURI);
    uiInterface.InitMessage.connect(InitMessage);
    uiInterface.QueueShutdown.connect(QueueShutdown);
    uiInterface.Translate.connect(Translate);

    // Show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (mapArgs.count("-?") || mapArgs.count("--help"))
    {
        GUIUtil::HelpMessageBox help;
        help.showOrPrint();
        return 1;
    }

    if (GetBoolArg("-splash", true) && !GetBoolArg("-min"))
    {
        app.createSplashScreen(isaTestNet);
    }

    app.processEvents();
    while(downloadingBlockchain){
        app.processEvents();
        uint ms = 500;
        #ifdef Q_OS_WIN
            Sleep(uint(ms));
        #else
            struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
            nanosleep(&ts, NULL);
        #endif
    }

    app.setQuitOnLastWindowClosed(false);

    try
    {
        app.createWindow(isaTestNet);
        app.requestInitialize();
#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
        WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("Cloak Core didn't yet exit safely..."), (HWND)app.getMainWinId());
#endif
        // Place this here as guiref has to be defined if we don't want to lose URIs
        ipcInit(argc, argv);

        app.exec();
        app.requestShutdown();
        app.exec();

        /*
        // Regenerate startup link, to fix links to old versions
        if (GUIUtil::GetStartOnSystemStartup())
            GUIUtil::SetStartOnSystemStartup(true);

        boost::thread_group threadGroup;
        BitcoinGUI window;
        guiref = &window;
        if(AppInit2(threadGroup))
        {
            {
                // Put this in a block, so that the Model objects are cleaned up before
                // calling Shutdown().

                optionsModel.Upgrade(); // Must be done after AppInit2

                if (splashref)
                    splash.finish(&window);

                ClientModel clientModel(&optionsModel);
                WalletModel walletModel(pwalletMain, &optionsModel);

                window.setClientModel(&clientModel);
                window.setWalletModel(&walletModel);

                // If -min option passed, start window minimized.
                if(GetBoolArg("-min"))
                {
                    window.showMinimized();
                }
                else
                {
                    window.show();
                }

                // Place this here as guiref has to be defined if we don't want to lose URIs
                ipcInit(argc, argv);

                app.exec();

                window.hide();
                window.setClientModel(0);
                window.setWalletModel(0);
                guiref = 0;
            }
            // Shutdown the core and its threads, but don't exit Bitcoin-Qt here
            Shutdown(NULL);
        }
        else
        {
            return 1;
        }*/
    } catch (std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    } catch (...) {
        PrintExceptionContinue(NULL, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    }
    return 0;
}
#endif // BITCOIN_QT_TEST
