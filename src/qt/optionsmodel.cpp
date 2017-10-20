#include "optionsmodel.h"
#include "bitcoinunits.h"
#include <QSettings>

#include "init.h"
#include "walletdb.h"
#include "guiutil.h"

extern int nEnigmaReservedBalancePercent;
extern int nCloakShieldNumRoutes;
extern int nCloakShieldNumNodes;
extern int nCloakShieldNumHops;
extern bool fOnionRouteAll;
extern bool fEnigma;
extern bool fStaking;

extern bool fEnigmaAutoRetry;
extern bool fEnableOnionRouting;

OptionsModel::OptionsModel(QObject *parent) :
    QAbstractListModel(parent)
{
    Init();
}

// todo: set toggles elsewhere
bool static ApplyCloakingStakingSettings()
{
    return true;
}

bool static ApplyEnigmaReverseBalancePercent()
{
    return true;
}

bool static ApplyProxySettings()
{
    QSettings settings;
    CService addrProxy(settings.value("addrProxy", "127.0.0.1:9050").toString().toStdString());
    int nSocksVersion(settings.value("nSocksVersion", 5).toInt());
    if (!settings.value("fUseProxy", false).toBool()) {
        addrProxy = CService();
        nSocksVersion = 0;
        return false;
    }
    if (nSocksVersion && !addrProxy.IsValid())
        return false;
    if (!IsLimited(NET_IPV4))
        SetProxy(NET_IPV4, addrProxy, nSocksVersion);
    if (nSocksVersion > 4) {
#ifdef USE_IPV6
        if (!IsLimited(NET_IPV6))
            SetProxy(NET_IPV6, addrProxy, nSocksVersion);
#endif
        SetNameProxy(addrProxy, nSocksVersion);
    }
    return true;
}

void OptionsModel::Init()
{
    QSettings settings;

    // These are Qt-only settings:

    fEnableOnionRouting = true;

    this->nDisplayUnit = settings.value("nDisplayUnit", BitcoinUnits::BTC).toInt();
    bDisplayAddresses = settings.value("bDisplayAddresses", false).toBool();
    fMinimizeToTray = settings.value("fMinimizeToTray", false).toBool();
    fMinimizeOnClose = settings.value("fMinimizeOnClose", false).toBool();
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    nTransactionFee = settings.value("nTransactionFee").toLongLong();

    language = settings.value("language", "").toString();
    nEnigmaReservedBalancePercent = settings.value("nEnigmaReservedBalancePercent", 50).toInt();
    nCloakShieldNumRoutes = settings.value("nCloakShieldNumRoutes", 3).toInt();
    nCloakShieldNumNodes = settings.value("nCloakShieldNumNodes", 3).toInt();
    nCloakShieldNumHops = settings.value("nCloakShieldNumHops", 3).toInt();
    fOnionRouteAll = settings.value("fOnionRouteAll", false).toBool();
    //fStaking = settings.value("bEnableStaking", true).toBool();
    fEnigmaAutoRetry = settings.value("fEnigmaAutoRetry", true).toBool();
    fEnableOnionRouting = settings.value("fEnableOnionRouting", true).toBool() && !fDisableOnionRouting;

    // These are shared with core Bitcoin; we want
    // command-line options to override the GUI settings:
    if (settings.contains("fUseUPnP"))
        SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool());
    if (settings.contains("addrProxy") && settings.value("fUseProxy").toBool())
        SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString());
    if (settings.contains("nSocksVersion") && settings.value("fUseProxy").toBool())
        SoftSetArg("-socks", settings.value("nSocksVersion").toString().toStdString());
    if (settings.contains("detachDB"))
        SoftSetBoolArg("-detachdb", settings.value("detachDB").toBool());
    if (!language.isEmpty())
        SoftSetArg("-lang", language.toStdString());
}

bool OptionsModel::Upgrade()
{
    QSettings settings;

    if (settings.contains("bImportFinished"))
        return false; // Already upgraded

    settings.setValue("bImportFinished", true);

    // create defaults
    settings.setValue("nDisplayUnit", BitcoinUnits::BTC);
    settings.setValue("bDisplayAddresses", false);
    settings.setValue("fMinimizeToTray", false);
    settings.setValue("fMinimizeOnClose", false);
    settings.setValue("fCoinControlFeatures", false);
    settings.setValue("nTransactionFee", 0);
    settings.setValue("language", "");
    settings.setValue("nEnigmaReservedBalancePercent", 50);
    settings.setValue("nCloakShieldNumRoutes", 3);
    settings.setValue("nCloakShieldNumNodes", 3);
    settings.setValue("nCloakShieldNumHops", 3);
    settings.setValue("fOnionRouteAll", false);
    //fStaking = settings.value("bEnableStaking", true).toBool();
    settings.setValue("fEnigmaAutoRetry", true);
    settings.setValue("fEnableOnionRouting", true);

    // Move settings from old wallet.dat (if any):
    CWalletDB walletdb("wallet.dat");

    QList<QString> intOptions;
    intOptions << "nDisplayUnit" << "nTransactionFee";
    foreach(QString key, intOptions)
    {
        int value = 0;
        if (walletdb.ReadSetting(key.toStdString(), value))
        {
            settings.setValue(key, value);
            walletdb.EraseSetting(key.toStdString());
        }
    }
    QList<QString> boolOptions;
    boolOptions << "bDisplayAddresses" << "fMinimizeToTray" << "fMinimizeOnClose" << "fUseProxy" << "fUseUPnP" << "bEnableCloaking" << "bEnableStaking" << "fOnionRouteAll" << "fEnigmaAutoRetry" << "fEnableOnionRouting";
    foreach(QString key, boolOptions)
    {
        bool value = false;
        if (walletdb.ReadSetting(key.toStdString(), value))
        {
            settings.setValue(key, value);
            walletdb.EraseSetting(key.toStdString());
        }
    }
    try
    {
        CAddress addrProxyAddress;
        if (walletdb.ReadSetting("addrProxy", addrProxyAddress))
        {
            settings.setValue("addrProxy", addrProxyAddress.ToStringIPPort().c_str());
            walletdb.EraseSetting("addrProxy");
        }
    }
    catch (std::ios_base::failure &e)
    {
        // 0.6.0rc1 saved this as a CService, which causes failure when parsing as a CAddress
        CService addrProxy;
        if (walletdb.ReadSetting("addrProxy", addrProxy))
        {
            settings.setValue("addrProxy", addrProxy.ToStringIPPort().c_str());
            walletdb.EraseSetting("addrProxy");
        }
    }
    ApplyProxySettings();
    ApplyEnigmaReverseBalancePercent();
    ApplyCloakingStakingSettings();
    settings.sync();
    Init();

    //settings.sync();

    return true;
}


int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            return QVariant(GUIUtil::GetStartOnSystemStartup());
        case MinimizeToTray:
            return QVariant(fMinimizeToTray);
        case MapPortUPnP:
            return settings.value("fUseUPnP", GetBoolArg("-upnp", true));
        case MinimizeOnClose:
            return QVariant(fMinimizeOnClose);
        case EnableCloaking:
            return settings.value("bEnableCloaking", true);
        case EnableStaking:
            return settings.value("bEnableStaking", true);
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {
            proxyType proxy;
            if (GetProxy(NET_IPV4, proxy))
                return QVariant(QString::fromStdString(proxy.first.ToStringIP()));
            else
                return QVariant(QString::fromStdString("127.0.0.1"));
        }
        case ProxyPort: {
            proxyType proxy;
            if (GetProxy(NET_IPV4, proxy))
                return QVariant(proxy.first.GetPort());
            else
                return QVariant(9050);
        }
        case ProxySocksVersion:
            return settings.value("nSocksVersion", 5);
        case Fee:
            return QVariant(nTransactionFee);
        case DisplayUnit:
            return QVariant(nDisplayUnit);
        case DisplayAddresses:
            return QVariant(bDisplayAddresses);
        case DetachDatabases:
            return QVariant(bitdb.GetDetach());
        case Language:
            return settings.value("language", "");
        case CoinControlFeatures:
            return QVariant(fCoinControlFeatures);
        case EnigmaReserve:
            return QVariant(nEnigmaReservedBalancePercent);
        case CloakShieldNumRoutes:
            return QVariant(nCloakShieldNumRoutes);
        case CloakShieldNumNodes:
            return QVariant(nCloakShieldNumNodes);
        case CloakShieldNumHops:
            return QVariant(nCloakShieldNumHops);
        case EnableOnionRouteAll:
            return QVariant(fOnionRouteAll);
        case EnableOnionRouting:
            return QVariant(fEnableOnionRouting);
        case EnigmaAutoRetry:
            return QVariant(fEnigmaAutoRetry);
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case MapPortUPnP:
            fUseUPnP = value.toBool();
            settings.setValue("fUseUPnP", fUseUPnP);
            MapPort();
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;
        case EnableCloaking:
            settings.setValue("bEnableCloaking", value.toBool());
            fEnigma = mapArgs.count("-enigma") ? GetBoolArg("-enigma", true) : value.toBool();
            ApplyCloakingStakingSettings();
            break;
        case EnableStaking:
            settings.setValue("bEnableStaking", value.toBool());
            if (mapArgs.count("-staking")) {
                // if an explicit public IP is specified, do not try to find others
                SoftSetBoolArg("-discover", false);
            }
            fStaking = mapArgs.count("-staking") ? GetBoolArg("-staking", true) : value.toBool();
            ApplyCloakingStakingSettings();
            break;
        case EnigmaReserve:
            settings.setValue("nPosaReservedBalancePercent", value.toInt());
            nEnigmaReservedBalancePercent = value.toInt();
            ApplyEnigmaReverseBalancePercent();
            break;
        case CloakShieldNumRoutes:
            settings.setValue("nCloakShieldNumRoutes", value.toInt());
            nCloakShieldNumRoutes = value.toInt();
            break;
        case CloakShieldNumNodes:
            settings.setValue("nCloakShieldNumNodes", value.toInt());
            nCloakShieldNumNodes = value.toInt();
            break;
        case CloakShieldNumHops:
            settings.setValue("nCloakShieldNumHops", value.toInt());
            nCloakShieldNumHops = value.toInt();
            break;
        case EnableOnionRouteAll:
            fOnionRouteAll = value.toBool();
            settings.setValue("fOnionRouteAll", fOnionRouteAll);
            break;
        case EnableOnionRouting:
            fEnableOnionRouting = value.toBool();
            settings.setValue("fEnableOnionRouting", fEnableOnionRouting);
            break;
        case EnigmaAutoRetry:
            fEnigmaAutoRetry = value.toBool();
            settings.setValue("fEnigmaAutoRetry", fEnigmaAutoRetry);
            break;
        case ProxyUse:
            settings.setValue("fUseProxy", value.toBool());
            ApplyProxySettings();
            break;
        case ProxyIP: {
            proxyType proxy;
            proxy.first = CService("127.0.0.1", 9050);
            GetProxy(NET_IPV4, proxy);

            CNetAddr addr(value.toString().toStdString());
            proxy.first.SetIP(addr);
            settings.setValue("addrProxy", proxy.first.ToStringIPPort().c_str());
            successful = ApplyProxySettings();
        }
        break;
        case ProxyPort: {
            proxyType proxy;
            proxy.first = CService("127.0.0.1", 9050);
            GetProxy(NET_IPV4, proxy);

            proxy.first.SetPort(value.toInt());
            settings.setValue("addrProxy", proxy.first.ToStringIPPort().c_str());
            successful = ApplyProxySettings();
        }
        break;
        case ProxySocksVersion: {
            proxyType proxy;
            proxy.second = 5;
            GetProxy(NET_IPV4, proxy);

            proxy.second = value.toInt();
            settings.setValue("nSocksVersion", proxy.second);
            successful = ApplyProxySettings();
        }
        break;
        case Fee:
            nTransactionFee = value.toLongLong();
            settings.setValue("nTransactionFee", nTransactionFee);
            emit transactionFeeChanged(nTransactionFee);
            break;
        case DisplayUnit:
            nDisplayUnit = value.toInt();
            settings.setValue("nDisplayUnit", nDisplayUnit);
            emit displayUnitChanged(nDisplayUnit);
            break;
        case DisplayAddresses:
            bDisplayAddresses = value.toBool();
            settings.setValue("bDisplayAddresses", bDisplayAddresses);
            break;
        case DetachDatabases: {
            bool fDetachDB = value.toBool();
            bitdb.SetDetach(fDetachDB);
            settings.setValue("detachDB", fDetachDB);
            }
            break;
        case Language:
            settings.setValue("language", value);
            break;
        case CoinControlFeatures: {
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            emit coinControlFeaturesChanged(fCoinControlFeatures);
            }
            break;
        default:
            break;
        }
    }
    emit dataChanged(index, index);

    return successful;
}

qint64 OptionsModel::getTransactionFee()
{
    return nTransactionFee;
}

bool OptionsModel::getCoinControlFeatures()
{
     return fCoinControlFeatures;
}
 

bool OptionsModel::getMinimizeToTray()
{
    return fMinimizeToTray;
}

bool OptionsModel::getMinimizeOnClose()
{
    return fMinimizeOnClose;
}

int OptionsModel::getDisplayUnit()
{
    return nDisplayUnit;
}

bool OptionsModel::getDisplayAddresses()
{
    return bDisplayAddresses;
}

int OptionsModel::getEnigmaReservePercent()
{
    return nEnigmaReservedBalancePercent;
}

int OptionsModel::getCloakShieldNumRoutes()
{
    return nCloakShieldNumRoutes;
}

int OptionsModel::getCloakShieldNumNodes()
{
    return nCloakShieldNumNodes;
}

int OptionsModel::getCloakShieldNumHops()
{
    return nCloakShieldNumHops;
}

bool OptionsModel::getEnableOnionRouteAll()
{
    return fOnionRouteAll;
}

bool OptionsModel::getEnableOnionRouting()
{
    return fEnableOnionRouting;
}

bool OptionsModel::getEnableEnigmaAutoRetry()
{
    return fEnigmaAutoRetry;
}

