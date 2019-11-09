// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Cloak developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "splashscreen.h"

#include "clientversion.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <QApplication>
#include <QPainter>
#include <QDesktopWidget>
#include <QFontDatabase>

SplashScreen::SplashScreen(Qt::WindowFlags f, bool isTestNet) : QWidget(0, f), curAlignment(0)
{
    // set reference point, paddings
    int paddingLeft             = 60;
    int paddingTop              = 430;

    int shadowOffset = 1;

    float fontFactor            = 1.2;

    QColor colorPen = QColor(164,164,164, 255);
    QColor colorShadow = QColor(92,92,92,96);

    // define text to place
    QString titleText       = tr("CLOAK Core");
    QString enigmaText       = QString(tr("ENIGMA Engine v%1")).arg(QString::fromStdString(ENIGMA_ENGINE_VER));
    QString versionText     = QString(tr("Version %1")).arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightTextBtc   = QChar(0xA9)+QString(" 2009-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("Bitcoin Developers"));
    QString copyrightTextCloak   = QChar(0xA9)+QString(" 2014-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("CloakCoin Developers"));
    QString testnetAddText  = QString(tr("[testnet]")); // define text to place as single text object
    QString devnetAddText  = QString(tr("[devnet]")); // define text to place as single text object

    QString splashName = ":/images/splash";

    pixmap = QPixmap(splashName);

    QPainter pixPaint(&pixmap);
    pixPaint.setPen(QColor(255,255,255));

    int id = QFontDatabase::addApplicationFont(":/fonts/font_cloak");
    QString cloakFontFamily = QFontDatabase::applicationFontFamilies(id).at(0);
    QString font            = "Arial";

    // check font size and drawing with
    pixPaint.setFont(QFont(font, 28*fontFactor));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth  = fm.width(titleText);
    if(titleTextWidth > 160) {
        // strange font rendering, Arial probably not found
        fontFactor = 0.75;
    }

    // cloak core
    pixPaint.setFont(QFont(cloakFontFamily, 28*fontFactor));
    fm = pixPaint.fontMetrics();
    titleTextWidth  = fm.width(titleText);

    pixPaint.setPen(colorShadow);
    pixPaint.drawText(paddingLeft+shadowOffset,paddingTop+shadowOffset,titleText);

    pixPaint.setPen(colorPen);
    pixPaint.drawText(paddingLeft,paddingTop,titleText);

    // version
    paddingTop+=17;
    pixPaint.setFont(QFont(cloakFontFamily, 18*fontFactor));
    pixPaint.setPen(colorShadow);
    pixPaint.drawText(paddingLeft+shadowOffset,paddingTop+shadowOffset,versionText);

    pixPaint.setPen(colorPen);
    pixPaint.drawText(paddingLeft,paddingTop,versionText);

    // enigma engine
    paddingTop+=17;
    pixPaint.setFont(QFont(cloakFontFamily, 15*fontFactor));
    fm = pixPaint.fontMetrics();
    titleTextWidth  = fm.width(enigmaText);

    pixPaint.setPen(colorShadow);
    pixPaint.drawText(paddingLeft+shadowOffset,paddingTop+shadowOffset,enigmaText);

    pixPaint.setPen(colorPen);
    pixPaint.drawText(paddingLeft,paddingTop,enigmaText);

    // draw copyright stuff
    paddingTop+=17;
    pixPaint.setFont(QFont(font, 10*fontFactor));
    pixPaint.setPen(colorShadow); // shadow
    pixPaint.drawText(paddingLeft+shadowOffset,paddingTop+shadowOffset,copyrightTextBtc);
    pixPaint.drawText(paddingLeft+shadowOffset,paddingTop+shadowOffset+12,copyrightTextCloak);

    pixPaint.setPen(colorPen);
    pixPaint.setFont(QFont(font, 10*fontFactor));
    pixPaint.drawText(paddingLeft,paddingTop,copyrightTextBtc);
    pixPaint.drawText(paddingLeft,paddingTop+12,copyrightTextCloak);

    if (FORCE_TESTNET)
        testnetNumber = FORCE_TESTNET;

    // draw testnet string if testnet is on
    if(isTestNet) {
        if (testnetNumber == 5)
        {
            // test net
            QFont boldFont = QFont(cloakFontFamily, 24*fontFactor);
            //boldFont.setWeight(QFont::Bold);
            pixPaint.setFont(boldFont);
            fm = pixPaint.fontMetrics();
            int testnetAddTextWidth  = fm.width(testnetAddText);
            pixPaint.setPen(colorShadow);
            pixPaint.drawText(pixmap.width()-testnetAddTextWidth-20+shadowOffset,25+shadowOffset,testnetAddText);
            pixPaint.setPen(colorPen);
            pixPaint.drawText(pixmap.width()-testnetAddTextWidth-20,25,testnetAddText);
        }else{
            // dev net
            QFont boldFont = QFont(cloakFontFamily, 24*fontFactor);
            //boldFont.setWeight(QFont::Bold);
            pixPaint.setFont(boldFont);
            fm = pixPaint.fontMetrics();
            int testnetAddTextWidth  = fm.width(devnetAddText);
            pixPaint.setPen(colorShadow);
            pixPaint.drawText(pixmap.width()-testnetAddTextWidth-20+shadowOffset,25+shadowOffset,devnetAddText);
            pixPaint.setPen(colorPen);
            pixPaint.drawText(pixmap.width()-testnetAddTextWidth-20,25,devnetAddText);
        }
    }

    pixPaint.end();

    // Set window title
    if(isTestNet)
       setWindowTitle(titleText + " " + testnetAddText);
    else
       setWindowTitle(titleText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), pixmap.size());
    resize(r.size());
    setFixedSize(r.size());
    move(QApplication::desktop()->screenGeometry().center() - r.center());

    subscribeToCoreSignals();
}

void SplashScreen::mousePressEvent(QMouseEvent *)
{
    //hide();
}

SplashScreen::~SplashScreen()
{
    unsubscribeFromCoreSignals();
}

void SplashScreen::slotFinish(QWidget *mainWin)
{
    hide();
    deleteLater();
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(111,111,111)));
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(5, 5, -5, -5);
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress)
{
    InitMessage(splash, title + strprintf("%d", nProgress) + "%");
}

#ifdef ENABLE_WALLET
static void ConnectWallet(SplashScreen *splash, CWallet* wallet)
{
    wallet->ShowProgress.connect(boost::bind(ShowProgress, splash, _1, _2));
}
#endif

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.InitMessage.connect(boost::bind(InitMessage, this, _1));
#ifdef ENABLE_WALLET
    uiInterface.LoadWallet.connect(boost::bind(ConnectWallet, this, _1));
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.InitMessage.disconnect(boost::bind(InitMessage, this, _1));
#ifdef ENABLE_WALLET
    if(pwalletMain)
        pwalletMain->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
#endif
}
