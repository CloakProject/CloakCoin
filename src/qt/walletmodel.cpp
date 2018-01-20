#include "walletmodel.h"
#include "enigmatablemodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"
#include "offertablemodel.h"

#include "ui_interface.h"
#include "wallet.h"
#include "walletdb.h" // for BackupWallet
#include "base58.h"

#include "bitcoinrpc.h"
#include "enigma/enigmaann.h"
#include "enigma/enigma.h"

#include <QSet>
#include <QTimer>

// pending input/output objects, ready to be added to a thirdparty posa3 request or
// to a request of ours if no other requests are available from peers.
extern map<uint256, CCloakingInputsOutputs> mapCloakingInputsOutputs; // key = CCloakingIdentifier hash
extern CCriticalSection cs_mapCloakingInputsOutputs;

class COfferDB;
extern COfferDB *pofferdb;
int GetOfferExpirationDepth(int nHeight);
class COfferAccept;

extern bool fWalletUnlockMintOnly;

WalletModel::WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
    transactionTableModel(0),
    cachedBalance(0), cachedStake(0), cachedUnconfirmedBalance(0), cachedImmatureBalance(0),
    cachedNumTransactions(0),
    cachedEncryptionStatus(Unencrypted),
    cachedPosaReserveBalance(0),
    cachedNumBlocks(0)
{
    addressTableModel = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);
    enigmaTableModel = new EnigmaTableModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

qint64 WalletModel::getBalance() const
{
    return wallet->GetBalance();
}

qint64 WalletModel::getEnigmaFeesEarnt() const
{
    return wallet->enigmaFeesEarnt;
}

qint64 WalletModel::getEnigmaReservedBalance() const
{
    return wallet->GetEnigmaReservedBalance();
}

qint64 WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}

qint64 WalletModel::getStake() const
{
    return wallet->GetStake();
}

qint64 WalletModel::getImmatureBalance() const
{
    return wallet->GetImmatureBalance();
}

int WalletModel::getNumTransactions() const
{
    int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::pollBalanceChanged()
{
    if (fShutdown)
        return;

    if(nBestHeight != cachedNumBlocks)
    {
        // Balance and number of transactions might have changed
        cachedNumBlocks = nBestHeight;
        checkBalanceChanged();
    }
}

void WalletModel::checkBalanceChanged()
{
    qint64 newBalance = getBalance();
    qint64 newStake = getStake();
    qint64 newUnconfirmedBalance = getUnconfirmedBalance();
    qint64 newImmatureBalance = getImmatureBalance();
    qint64 newPosaReservedBalance = getEnigmaReservedBalance();

    if(cachedBalance != newBalance || cachedStake != newStake || cachedUnconfirmedBalance != newUnconfirmedBalance || cachedImmatureBalance != newImmatureBalance || cachedPosaReserveBalance != newPosaReservedBalance)
    {
        cachedBalance = newBalance;
        cachedStake = newStake;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance = newImmatureBalance;
        cachedPosaReserveBalance = newPosaReservedBalance;
        emit balanceChanged(newBalance, newStake, newUnconfirmedBalance, newImmatureBalance, newPosaReservedBalance);
    }
}

void WalletModel::updateTransaction(const QString &hash, int status)
{
    if(transactionTableModel)
        transactionTableModel->updateTransaction(hash, status);

    // Balance and number of transactions might have changed
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if(cachedNumTransactions != newNumTransactions)
    {
        cachedNumTransactions = newNumTransactions;
        emit numTransactionsChanged(newNumTransactions);
    }
}

void WalletModel::updateAddressBook(const QString &address, const QString &label, bool isMine, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, status);
}

bool WalletModel::validateAddress(const QString &address)
{
    if (IsStealthAddress(address.toStdString())){
        CStealthAddress sa;
        return sa.SetEncoded(address.toStdString());
    }else{
        CBitcoinAddress addressParsed(address.toStdString());
        return addressParsed.IsValid();
    }
}

void WalletModel::updateEnigma()
{
    checkBalanceChanged();
}

WalletModel::SendCoinsReturn WalletModel::sendCoinsEnigma(const QList<SendCoinsRecipient> &recipients, int numParticipants, int numSplits, int timeoutSecs, const CCoinControl *coinControl)
{
    qint64 total = 0;
    QSet<QString> setAddress;
    QString hex;

    if(recipients.empty())
    {
        return OK;
    }

    // Pre-check input data for validity
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        if(!IsStealthAddress(rcp.address.toStdString()))
        {
            return InvalidAddress;
        }
        setAddress.insert(rcp.address);

        if(rcp.amount < MIN_TXOUT_AMOUNT)
        {
            return InvalidAmount;
        }
        total += rcp.amount;
    }

    // 1.8% Enigma fee
    // we initially reserve 10 * [numParticipants] * min fee, so that we have enough to cover the fees
    // if participants supply a large number of inputs, which bumps the tx size and boosts the fee.
    // any usused fee is redirected to one of our change addresses at tx creation time.
    //quint64 feeAndReward = total /* numSplits*/ * (POSA_3_TOTAL_FEE_PERCENT * 0.01) + (MIN_TX_FEE*20);

    int64 reward = (int64)(total * (ENIGMA_TOTAL_FEE_PERCENT * 0.01));
    int64 reserveForFees = (MIN_TX_FEE * 10 * numParticipants);

    if(recipients.size() > setAddress.size()){
        return DuplicateAddress;
    }

    int64 nBalance = 0;
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, coinControl, ENIGMA_MAX_COINAGE_DAYS);

    BOOST_FOREACH(const COutput& out, vCoins)
        nBalance += out.tx->vout[out.i].nValue;

    if(total > nBalance)
    {
        OutputDebugStringF("WalletModel::sendCoinsEnigma - AmountExceedsBalance - need to stake??\n");
        return AmountExceedsBalance;
    }

    if((total + (qint64)reward + (qint64)reserveForFees) > nBalance)
    {
        return SendCoinsReturn(AmountWithFeeExceedsBalance, nTransactionFee);
    }

    // Sendmany
    int64 nValue = 0;
    CCloakingInputsOutputs inOuts;
    foreach(const SendCoinsRecipient &rcp, recipients){
        nValue += rcp.amount;
    }

    if (recipients.empty() || nValue < 0)
        return InvalidAddress;

    CCloakingRequest* req = CCloakingRequest::CreateForBroadcast(numParticipants-1, numSplits, timeoutSecs, CCloakShield::GetShield()->GetRoutingKey(), nValue);
    foreach(const SendCoinsRecipient &rcp, recipients){
        // add outputs / fill vouts
        CStealthAddress address;
        if (!IsStealthAddress(rcp.address.toStdString())){
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid CloakCoin address");
        }
        address.SetEncoded(rcp.address.toStdString());

        vector<CScript> scripts;
        uint256 standardNonce = uint256(HexStr(req->stealthRootKey)); // recipients always use the root stealth nonce

        vector<ec_secret> secrets;
        printf("** generating recipient one-time stealth addresses **\n");
        pwalletMain->GetEnigmaChangeAddresses(address, req->stealthRootKey, standardNonce, GetRandRange(2, 3), scripts, secrets);
        vector<int64> amounts = SplitAmount(rcp.amount, MIN_TXOUT_AMOUNT, static_cast<int>(scripts.size()));
        for (unsigned int i = 0; i<(unsigned int)amounts.size(); i++){
            inOuts.vout.push_back(CTxOut(amounts.at(i), scripts.at(i)));
        }
    }

    // Choose coins to use
    set<pair<const CWalletTx*,unsigned int> > setCoins;
    int64 nValueIn = 0;

    if (!wallet->SelectCoins(nValue + reward + reserveForFees, GetTime(), setCoins, nValueIn, coinControl))
        return InvalidAmount;

    inOuts.nSendAmount = nValue;
    inOuts.nInputAmount = nValueIn;

    // add inputs / fill vins and mark the coins as reserved for posa
    BOOST_FOREACH(const PAIRTYPE(const CWalletTx*, unsigned int)& coin, setCoins)
    {
        CWalletTx& wtxIn = pwalletMain->mapWallet[coin.first->GetHash()];
        wtxIn.MarkEnigmaReserved(coin.second); // mark as reserved for posa
        inOuts.vin.push_back(CTxIn(coin.first->GetHash(), coin.second));
    }

    //int64 nChange = nValueIn - nTotalValue;// - nValue - nFeeRet;
    int64 nChange = nValueIn - nValue - reward;

    OutputDebugStringF(" nChange %" PRI64d " nValueIn %" PRI64d " nValue %" PRI64d " reward %" PRI64d " fee %" PRI64d "", nChange, nValueIn , nValue, reward, reserveForFees );

    // store enigma address against request so we can assign our change later
    pwalletMain->GetEnigmaAddress(req->senderAddress);

    // handle change
    //pwalletMain->GetCloakingOutputs(inOuts, nValue, nChange, req->GetIdHash());

    // handle the inputOutput object
    if (Enigma::SendEnigma(inOuts, req))
    {
        // Add addresses / update labels that we've sent to to the address book
        foreach(const SendCoinsRecipient &rcp, recipients)
        {
            std::string strAddress = rcp.address.toStdString();
            CStealthAddress sa;
            sa.SetEncoded(strAddress);
            CTxDestination dest = sa;
            std::string strLabel = rcp.label.toStdString();
            {
                LOCK(wallet->cs_wallet);

                std::map<CTxDestination, std::string>::iterator mi = wallet->mapAddressBook.find(dest);

                // Check if we have a new address or an updated label
                if (mi == wallet->mapAddressBook.end() || mi->second != strLabel)
                {
                    wallet->SetAddressBookName(sa, strLabel);
                }
            }
        }
    }
    return SendCoinsReturn(OK, 0, "");
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient> &recipients, bool bAnonymize, const CCoinControl *coinControl)
{
    qint64 total = 0;
    qint64 stealth = 0;
    QSet<QString> setAddress;
    QString hex;

    if(recipients.empty())
    {
        return OK;
    }

    // Pre-check input data for validity
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        if(!validateAddress(rcp.address))
        {
            return InvalidAddress;
        }
        setAddress.insert(rcp.address);

        if (IsStealthAddress(rcp.address.toStdString())){
            stealth++;
        }

        if(rcp.amount <= 0)
        {
            return InvalidAmount;
        }
        total += rcp.amount;
    }

    // add fee for stealth returns
    total += stealth * MIN_TXOUT_AMOUNT;

    if(recipients.size() > setAddress.size())
    {
        return DuplicateAddress;
    }

    int64 nBalance = 0;
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, coinControl);

    BOOST_FOREACH(const COutput& out, vCoins)
            nBalance += out.tx->vout[out.i].nValue;

    if(total > nBalance)
    {
        return AmountExceedsBalance;
    }

    if((total + nTransactionFee) > nBalance)
    {
        return SendCoinsReturn(AmountWithFeeExceedsBalance, nTransactionFee);
    }

    LOCK2(cs_main, wallet->cs_wallet);
    {
        // Sendmany
        std::vector<std::pair<CScript, int64> > vecSend;
        foreach(const SendCoinsRecipient &rcp, recipients)
        {
            if (IsStealthAddress(rcp.address.toStdString())){
                CStealthAddress sxAddr;
                if (sxAddr.SetEncoded(rcp.address.toStdString()))
                {
                    ec_secret ephem_secret;
                    ec_secret secretShared;
                    ec_point pkSendTo;
                    ec_point ephem_pubkey;

                    if (GenerateRandomSecret(ephem_secret) != 0)
                    {
                        printf("GenerateRandomSecret failed.\n");
                        return Aborted;
                    }
                    if (StealthSecret(ephem_secret, sxAddr.scan_pubkey, sxAddr.spend_pubkey, secretShared, pkSendTo) != 0)
                    {
                        printf("Could not generate receiving public key.\n");
                        return Aborted;
                    }

                    CPubKey cpkTo(pkSendTo);
                    if (!cpkTo.IsValid())
                    {
                        printf("Invalid public key generated.\n");
                        return Aborted;
                    }

                    CKeyID ckidTo = cpkTo.GetID();
                    CBitcoinAddress addrTo(ckidTo);

                    if (SecretToPublicKey(ephem_secret, ephem_pubkey) != 0)
                    {
                        printf("Could not generate ephem public key.\n");
                        return Aborted;
                    }

                    CScript scriptPubKey;
                    scriptPubKey.SetDestination(addrTo.Get());
                    vecSend.push_back(make_pair(scriptPubKey, rcp.amount));

                    CScript scriptP = CScript() << OP_RETURN << ephem_pubkey;
                    vecSend.push_back(make_pair(scriptP, MIN_TXOUT_AMOUNT));

                    continue;
                }; // else drop through to normal
            }else{
                CScript scriptPubKey;
                scriptPubKey.SetDestination(CBitcoinAddress(rcp.address.toStdString()).Get());
                vecSend.push_back(make_pair(scriptPubKey, rcp.amount));
            }
        }

        CWalletTx wtx;
        CReserveKey keyChange(wallet);
        int64 nFeeRequired = 0;
        std::string strFailReason;
        std::string data;
        int nChangePos;
        bool fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, nChangePos, strFailReason, bAnonymize, coinControl, data);

        if(!fCreated)
        {
            if((total + nFeeRequired) > nBalance) // FIXME: could cause collisions in the future
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance, nFeeRequired);
            }
            emit error(tr("Send Coins"), QString::fromStdString(strFailReason), true);
            return TransactionCreationFailed;
        }
        if(!uiInterface.ThreadSafeAskFee(nFeeRequired, tr("Sending...").toStdString()))
        {
            return Aborted;
        }
        if(!wallet->CommitTransaction(wtx, keyChange))
        {
            return TransactionCommitFailed;
        }
        hex = QString::fromStdString(wtx.GetHash().GetHex());
    }

    // Add addresses / update labels that we've sent to to the address book
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        std::string strAddress = rcp.address.toStdString();
        CTxDestination dest = CBitcoinAddress(strAddress).Get();
        std::string strLabel = rcp.label.toStdString();
        {
            LOCK(wallet->cs_wallet);

            std::map<CTxDestination, std::string>::iterator mi = wallet->mapAddressBook.find(dest);

            // Check if we have a new address or an updated label
            if (mi == wallet->mapAddressBook.end() || mi->second != strLabel)
            {
                wallet->SetAddressBookName(dest, strLabel);
            }
        }
    }

    return SendCoinsReturn(OK, 0, hex);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

EnigmaTableModel *WalletModel::getEnigmaTableModel()
{
    return enigmaTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if(encrypted)
    {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::setWalletDataEncrypted(bool encrypt, const SecureString &passPhrase)
{
    if (encrypt)
    {
        return wallet->EncryptWalletData(passPhrase);
    }
    else
    {
        return wallet->DecryptWalletData(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString &filename)
{
    return BackupWallet(*wallet, filename.toLocal8Bit().data());
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel, CCryptoKeyStore *wallet)
{
    OutputDebugStringF("NotifyKeyStoreStatusChanged\n");
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel, CWallet *wallet, const CTxDestination &address, const std::string &label, bool isMine, ChangeType status)
{

    std::string addStr;
    if (address.which() == CTxDestination_CStealthAddress){
        addStr = boost::get<CStealthAddress>(address).Encoded();
    }else{
        addStr = CBitcoinAddress(address).ToString();
    }

    OutputDebugStringF("NotifyAddressBookChanged %s %s isMine=%i status=%i\n", addStr.c_str(), label.c_str(), isMine, status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(addStr)),
                              Q_ARG(QString, QString::fromStdString(label)),
                              Q_ARG(bool, isMine),
                              Q_ARG(int, status));
}

static void NotifyEnigmaChanged(WalletModel *walletmodel, CWallet *wallet, const CCloakingRequest* request)
{
    OutputDebugStringF("NotifyPosaChanged requestId=%s\n", request->GetIdHash().ToString().c_str());
    QMetaObject::invokeMethod(walletmodel, "updateEnigma", Qt::QueuedConnection);
}

static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    OutputDebugStringF("NotifyTransactionChanged %s status=%i\n", hash.GetHex().c_str(), status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(hash.GetHex())),
                              Q_ARG(int, status));
}

void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    wallet->NotifyEnigmaChanged.connect(boost::bind(NotifyEnigmaChanged, this, _1, _2));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    wallet->NotifyEnigmaChanged.disconnect(boost::bind(NotifyEnigmaChanged, this, _1, _2));
}

WalletModel::UnlockContext WalletModel::requestUnlock()
{
    return requestUnlock(false, 0);
}

WalletModel::UnlockContext WalletModel::requestUnlock(bool leaveUnlockedForMinting)
{
    return requestUnlock(leaveUnlockedForMinting, 0);
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock(bool leaveUnlockedForMinting, qint64 enigmaRelockTime)
{
    bool was_locked = getEncryptionStatus() == Locked;
    if(was_locked)
    {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    else if(fWalletUnlockMintOnly)
    {
        // Relock so the user must give the right passphrase to continue
        setWalletLocked(true);

        // Request UI to unlock wallet
        emit requireUnlock();
    }

    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    if (was_locked && leaveUnlockedForMinting){
        wallet->shouldRelockAfterEnigmaSend = true;
    }

    wallet->minEnigmaRelockTime = enigmaRelockTime;

    return UnlockContext(this, valid, was_locked && !leaveUnlockedForMinting);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

 bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
 {
     return wallet->GetPubKey(address, vchPubKeyOut);
 }

 // returns a list of COutputs from COutPoints
 void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
 {
     BOOST_FOREACH(const COutPoint& outpoint, vOutpoints)
     {
         if (!wallet->mapWallet.count(outpoint.hash)) continue;
         COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, wallet->mapWallet[outpoint.hash].GetDepthInMainChain());
         vOutputs.push_back(out);
     }
 }

 // AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address)
 void WalletModel::listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const
 {
     std::vector<COutput> vCoins;
     wallet->AvailableCoins(vCoins);
     std::vector<COutPoint> vLockedCoins;

     // add locked coins
     BOOST_FOREACH(const COutPoint& outpoint, vLockedCoins)
     {
         if (!wallet->mapWallet.count(outpoint.hash)) continue;
         COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, wallet->mapWallet[outpoint.hash].GetDepthInMainChain());
         vCoins.push_back(out);
     }

     BOOST_FOREACH(const COutput& out, vCoins)
     {
         COutput cout = out;

         while (wallet->IsChange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0 && wallet->IsMine(cout.tx->vin[0]))
         {
             if (!wallet->mapWallet.count(cout.tx->vin[0].prevout.hash)) break;
             cout = COutput(&wallet->mapWallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0);
         }

         CTxDestination address;
         if(!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address)) continue;
         mapCoins[CBitcoinAddress(address).ToString().c_str()].push_back(out);
     }
 }

 bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
 {
     return false;
 }

 void WalletModel::lockCoin(COutPoint& output)
 {
     return;
 }

 void WalletModel::unlockCoin(COutPoint& output)
 {
     return;
 }

 void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
 {
     return;
 }
