#ifndef BITCOIN_H
#define BITCOIN_H

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

#include <QApplication>
#include <QMessageBox>
#include <QTextCodec>
#include <QLocale>
#include <QTranslator>
#include <QSplashScreen>
#include <QTimer>
#include <QThread>
#include <QLibraryInfo>

/** Class encapsulating Cloak Core startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class BitcoinCore: public QObject
{
    Q_OBJECT
public:
    explicit BitcoinCore();

public slots:
    void initialize();
    void shutdown();

signals:
    void initializeResult(int retval);
    void shutdownResult(int retval);
    void runawayException(const QString &message);

private:
    boost::thread_group threadGroup;

    /// Pass fatal exception message to UI thread
    void handleRunawayException(std::exception *e);
};

/** Main Cloak application object */
class BitcoinApplication: public QApplication
{
    Q_OBJECT
public:
    explicit BitcoinApplication(int &argc, char **argv);
    ~BitcoinApplication();

#ifdef ENABLE_WALLET
    /// Create payment server
    void createPaymentServer();
#endif
    /// Create options model
    void createOptionsModel();
    /// Create wallet model
    void createWalletModel();
    /// Create main window
    void createWindow(bool isaTestNet);
    /// Create splash screen
    void createSplashScreen(bool isaTestNet);
    /// Request core initialization
    void requestInitialize();
    /// Request core shutdown
    void requestShutdown();
    /// Get process return value
    int getReturnValue() { return returnValue; }
    /// Get window identifier of QMainWindow (BitcoinGUI)
    WId getMainWinId() const;

public slots:
    void initializeResult(int retval);
    void shutdownResult(int retval);
    /// Handle runaway exceptions. Shows a message box with the problem and quits the program.
    void handleRunawayException(const QString &message);
    void gotBytesDownloadBlockchain(const char* data, int dataSize);
    int unpackDownloadedBlockchain();
    void updateDownloadBlockchainProgress(qint64 bytesRead, qint64 bytesTotal);

signals:
    void requestedInitialize();
    void requestedShutdown();
    void stopThread();
    void splashFinished(QWidget *window);

private:
    QThread *coreThread;
    QThread *downloadThread;
    OptionsModel *optionsModel;
    ClientModel *clientModel;
    WalletModel *walletModel;
    BitcoinGUI *window;
    QTimer *pollShutdownTimer;

    //CCriticalSection cs_BlockchainDownload;
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer;
    WalletModel *walletModel;
#endif
    int returnValue;

    void startThread();
    int unzipBlockchain();
};

#endif // BITCOIN_H
