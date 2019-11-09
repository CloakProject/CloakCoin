// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLET_H
#define BITCOIN_WALLET_H

#include <string>
#include <vector>

#include <stdlib.h>

#include "main.h"
#include "key.h"
#include "keystore.h"
#include "script.h"
#include "ui_interface.h"
#include "util.h"
#include "walletdb.h"
#include "stealth.h"
#include "enigma/enigmaann.h"
#include "enigma/enigma.h"
#include "hdkey.h"

extern bool fWalletUnlockMintOnly;
class CAccountingEntry;
class CWalletTx;
class CReserveKey;
class COutput;
class CCoinControl;
class COffer;
class CCloakingInputsOutputs;

typedef std::map<CKeyID, CStealthKeyMetadata> StealthKeyMetaMap;
typedef std::map<std::string, std::string> mapValue_t;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_LATEST = 60000
};



/** A key pool entry */
class CKeyPool
{
public:
    int64 nTime;
    CPubKey vchPubKey;

    CKeyPool()
    {
        nTime = GetTime();
    }

    CKeyPool(const CPubKey& vchPubKeyIn)
    {
        nTime = GetTime();
        vchPubKey = vchPubKeyIn;
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
    )
};

/** A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore
{
private:
    CWalletDB *pwalletdbEncryption;

    // the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    // the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

public:
    bool SelectCoins(int64 nTargetValue, unsigned int nSpendTime, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64& nValueRet, const CCoinControl *coinControl=NULL) const;

    mutable CCriticalSection cs_wallet;
    mutable CCriticalSection cs_activeEnigmaAddresses;

    bool fFileBacked;
    std::string strWalletFile;

    std::set<int64> setKeyPool;
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

    std::set<CStealthAddress> stealthAddresses;
    std::set<std::string> activeEnigmaAddresses;
    StealthKeyMetaMap mapStealthKeyMeta;
    uint32_t nStealth, nFoundStealth, nFoundEnigma; // for reporting, zero before use

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    // enigma
    CCriticalSection cs_mapCloakingInputsOutputs;
    CCriticalSection cs_mapCloakingRequests;
    CCriticalSection cs_mapOurCloakingRequests;
    CCriticalSection cs_mapCloakingKeys;
    CCriticalSection cs_sEnigmaRequests;
    CCriticalSection cs_sSignResponses;
    CCriticalSection cs_sSignHashes;
    CCriticalSection cs_CloakingAccepts;
    CCriticalSection cs_CloakingRequest;
    CCriticalSection cs_CloakingSecrets;
    CCriticalSection cs_CloakDataHashes; // hashes for encrypted data packets. used to avoid reprocessing data repeatedly
    set<uint256> sCloakDataHashes;

    // pending input/output objects, ready to be added to a thirdparty enigma request or
    // to a request of ours if no other requests are available from peers.
    map<uint256, CCloakingInputsOutputs> mapCloakingInputsOutputs; // key = CCloakingIdentifier hash

    // requests we have created, that others may participate in
    map<uint256, CCloakingRequest> mapOurCloakingRequests; // key = CCloakingIdentifier hash

    // requests others have created, that we may participate in
    map<uint256, CCloakingRequest> mapCloakingRequests; // key = CCloakingIdentifier hash

    set<CCloakingSignResponse> sSignResponses;
    set<uint256> sRequestHashes;
    set<uint256> sSignRequestHashes;

    int64 enigmaFeesEarnt;
    bool shouldRelockAfterEnigmaSend;
    int64 minEnigmaRelockTime;

    // enigma acceptances from other nodes taking part in one of our requests, or our own accepts for others requests.
    map<uint256, CCloakingAcceptResponse> sCloakingAccepts;

    CWallet()
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nBlocksUntilEnigma = 0;
        enigmaFeesEarnt = 0;
        shouldRelockAfterEnigmaSend = false;
        minEnigmaRelockTime = 0;
    }

    CWallet(std::string strWalletFileIn)
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        strWalletFile = strWalletFileIn;
        fFileBacked = true;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nBlocksUntilEnigma = 0;
        enigmaFeesEarnt = 0;
        shouldRelockAfterEnigmaSend = false;
        minEnigmaRelockTime = 0;
    }

    std::map<uint256, CWalletTx> mapWallet;
    int64 nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<CTxDestination, std::string> mapAddressBook;

    CPubKey vchDefaultKey;

    bool EnigmaAvailable();
    std::set<uint256> setEnigmaProcessed;
    int64 EnigmaCount();
    bool ProcessEnigma();
    bool WriteBlocksUntilEnigma();
    bool ProcessRemoval(std::set<uint256> removalQueue);
    int64 nBlocksUntilEnigma;

    // check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) { return nWalletMaxVersion >= wf; }

    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl=NULL, const int maxCoinAgeDays=-1) const;
    bool SelectCoinsMinConf(int64 nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64& nValueRet) const;
    // keystore implementation
    // Generate a new key
    CPubKey GenerateNewKey();
    // Adds a key to the store, and saves it to disk.
    bool AddKey(const CKey& key);
    // Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key) { return CCryptoKeyStore::AddKey(key); }

    bool LoadMinVersion(int nVersion) { nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }

    // Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    // Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret) { SetMinVersion(FEATURE_WALLETCRYPT); return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret); }
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript) { return CCryptoKeyStore::AddCScript(redeemScript); }

    bool IsValidPassphrase(const SecureString& strWalletPassphrase);
    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);
    bool EncryptWalletData(const SecureString& strDataPassphrase);
    bool DecryptWalletData(const SecureString& strDataPassphrase);
    DBErrors ZapWalletTx();

    /** Increment the next transaction order id
        @return next transaction order id
     */
    int64 IncOrderPosNext(CWalletDB *pwalletdb = NULL);

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64, TxPair > TxItems;

    /** Get the wallet's activity log
        @return multimap of ordered transactions and accounting entries
        @warning Returned pointers are *only* valid within the scope of passed acentries
     */
    TxItems OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount = "");

    void MarkDirty();
    bool HandleEnigmaWtxAsCloaker(const CWalletTx& wtxIn);
    bool AddToWallet(const CWalletTx& wtxIn);
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate = false, bool fFindBlock = false);
    bool EraseFromWallet(uint256 hash);
    void WalletUpdateSpent(const CTransaction& prevout);
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    int ScanForWalletTransaction(const uint256& hashTx);
    void ReacceptWalletTransactions();
    void ResendWalletTransactions();
    bool SaveEnigmaFeesEarntToDisk();
    bool SetEnigmaAddressActive(std::string address, bool isActive);
    bool SaveActiveEnigmaAddressesToDisk();
    //void ResendEnigmaTransactions();
    int64 GetBalance() const;
    int64 GetEnigmaReservedBalance() const;
    int64 GetUnconfirmedBalance() const;
    int64 GetImmatureBalance() const;
    int64 GetStake() const;
    int64 GetNewMint() const;
    bool CreateTransaction(const std::vector<std::pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, int& nChangePos, std::string& strFailReason, bool bAnonymize, const CCoinControl *coinControl=NULL, const std::string& txData="");
    bool CreateTransaction(CScript scriptPubKey, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, std::string& strFailReason, bool bAnonymize, const CCoinControl *coinControl=NULL, const std::string& txData="");
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);
    bool GetStakeWeight(const CKeyStore& keystore, uint64& nMinWeight, uint64& nMaxWeight, uint64& nWeight);
    bool CreateCoinStake(const CKeyStore& keystore, unsigned int nBits, int64 nSearchInterval, CTransaction& txNew);
    std::string SendMoney(CScript scriptPubKey, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, bool bAnonymize, bool fAskFee=false, const std::string& txData="");
    std::string SendMoneyToDestination(const CTxDestination &address, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, bool bAnonymize, bool fAskFee=false, const std::string& txData="");
    std::string SendMoneyEnigma(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, bool fAskFee, int numParticipantsRequired = 3);
    std::string SendMoneyToDestinationEnigma(const CTxDestination &address, int64 nValue, CWalletTx& wtxNew, bool bAnonymize, bool fAskFee=false);
    std::string SendData(CWalletTx& wtxNew, bool fAskFee, const std::string& txData);

    vector<CBitcoinAddress> GetExistingAddresses();
    bool GetStealthAddress(CStealthAddress& addressOut, std::string enigmaAddress);
    bool GetEnigmaAddress(CStealthAddress &address);
    bool GetEnigmaChangeAddresses(const CStealthAddress &sxAddress, const ec_point &stealthRootKey, const uint256 &cloakerHash, uint64 numChangeAddresses, vector<CScript> &scriptsOut, vector<ec_secret> &secretsOut, uint64 offset=0);

    bool NewStealthAddress(std::string& sError, std::string& sLabel, CStealthAddress& sxAddr);
    bool AddStealthAddress(CStealthAddress& sxAddr);
    bool UnlockStealthAddresses(const CKeyingMaterial& vMasterKeyIn);
    bool UpdateStealthAddress(std::string &addr, std::string &label, bool addIfNotExist);

    bool CreateStealthTransaction(CScript scriptPubKey, int64 nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, const CCoinControl* coinControl=NULL);
    std::string SendStealthMoney(CScript scriptPubKey, int64 nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, bool fAskFee=false);
    bool SendStealthMoneyToDestination(CStealthAddress& sxAddress, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, std::string& sError, bool fAskFee=false);
    bool FindStealthTransactionForOutput(const CStealthAddress& stealthAddress, const CTxOut& txout, const CTxOut& txoutB, std::vector<uint8_t> vchEphemPK, mapValue_t& mapNarr, CScript::const_iterator itTxA, int32_t nOutputId, bool isEnigmaAddress);
    int64 FindEnigmaTransactions(const CTransaction& tx, const CTxOut& txoutStealthKey, std::vector<uint8_t> vchEphemPK, mapValue_t& mapNarr);
    int64 FindStealthTransactions(const CTransaction& tx, mapValue_t& mapNarr, int64& stealthAmountOut, int64& enigmaAmountOut);
    bool GetStealthEphemPK(const CTransaction& tx, mapValue_t& mapNarr, std::vector<uint8_t>& vchEphemPK, std::vector<uint8_t>& vchENarr, CScript::const_iterator& itTxA, CTxOut& stealthReturn);
    uint64 GetEnigmaOutputsAmounts(const CTransaction& tx, std::vector<uint8_t>& vchEphemPK);
    int SecondsSinceWalletLastStaked();

    bool NewKeyPool();
    bool TopUpKeyPool();
    int64 AddReserveKey(const CKeyPool& keypool);
    void ReserveKeyFromKeyPool(int64& nIndex, CKeyPool& keypool);
    void KeepKey(int64 nIndex);
    void ReturnKey(int64 nIndex);
    bool GetKeyFromPool(CPubKey &key, bool fAllowReuse=true);
    int64 GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress);

    std::set< std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, int64> GetAddressBalances();

    void ExpireOldEnigma();

    bool IsMine(const CTxIn& txin) const;
    int64 GetDebit(const CTxIn& txin) const;
    bool IsMine(const CTxOut& txout) const
    {
        return ::IsMine(*this, txout.scriptPubKey);
    }
    int64 GetCredit(const CTxOut& txout) const
    {
        if (!MoneyRange(txout.nValue))
            throw std::runtime_error("CWallet::GetCredit() : value out of range");
        return (IsMine(txout) ? txout.nValue : 0);
    }
    bool IsChange(const CTxOut& txout) const;
    int64 GetChange(const CTxOut& txout) const
    {
        if (!MoneyRange(txout.nValue))
            throw std::runtime_error("CWallet::GetChange() : value out of range");
        return (IsChange(txout) ? txout.nValue : 0);
    }
    bool IsMine(const CTransaction& tx) const
    {
        BOOST_FOREACH(const CTxOut& txout, tx.vout)
            if (IsMine(txout))
                return true;
        return false;
    }
    bool IsFromMe(const CTransaction& tx) const
    {
        return (GetDebit(tx) > 0);
    }
    int64 GetDebit(const CTransaction& tx) const
    {
        int64 nDebit = 0;
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
        {
            nDebit += GetDebit(txin);
            if (!MoneyRange(nDebit))
                throw std::runtime_error("CWallet::GetDebit() : value out of range");
        }
        return nDebit;
    }
    int64 GetCredit(const CTransaction& tx) const
    {
        int64 nCredit = 0;
        BOOST_FOREACH(const CTxOut& txout, tx.vout)
        {
            nCredit += GetCredit(txout);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWallet::GetCredit() : value out of range");
        }
        return nCredit;
    }
    int64 GetChange(const CTransaction& tx) const
    {
        int64 nChange = 0;
        BOOST_FOREACH(const CTxOut& txout, tx.vout)
        {
            nChange += GetChange(txout);
            if (!MoneyRange(nChange))
                throw std::runtime_error("CWallet::GetChange() : value out of range");
        }
        return nChange;
    }
    void SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);

    bool SetAddressBookName(const CTxDestination& address, const std::string& strName);

    bool DelAddressBookName(const CTxDestination& address);

    void UpdatedTransaction(const uint256 &hashTx);

    bool GetCloakingOutputs(CCloakingInputsOutputs& inOuts, int64 nValue, int64 nChange, uint256 requestHash);

    void PrintWallet(const CBlock& block);

    void Inventory(const uint256 &hash)
    {
        {
            LOCK(cs_wallet);
            std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
            if (mi != mapRequestCount.end())
                (*mi).second++;
        }
    }

    int GetKeyPoolSize()
    {
        return setKeyPool.size();
    }

    bool GetTransaction(const uint256 &hashTx, CWalletTx& wtx);

    bool SetDefaultKey(const CPubKey &vchPubKey);

    // signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    // change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    // get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { return nWalletVersion; }

    void FixSpentCoins(int& nMismatchSpent, int64& nBalanceInQuestion, bool fCheckOnly = false);
    void DisableTransaction(const CTransaction &tx);

    // a enigma request that we created or are participating in has been updated.
    void EnigmaRequestUpdated(const CCloakingRequest* request);

    /** Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination &address, const std::string &label, bool isMine, ChangeType status)> NotifyAddressBookChanged;

    /** Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx, ChangeType status)> NotifyTransactionChanged;


    /** Offer list entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTransaction *txn,  COffer &offer, ChangeType status)> NotifyOfferListChanged;

    /** Enigma participation changed
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CCloakingRequest *request)> NotifyEnigmaChanged;

};

/** A key allocated from the key pool. */
class CReserveKey
{
protected:
    CWallet* pwallet;
    int64 nIndex;
    CPubKey vchPubKey;
public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        if (!fShutdown)
            ReturnKey();
    }

    void ReturnKey();
    CPubKey GetReservedKey();
    bool GetReservedKey(CPubKey &pubkey);
    void KeepKey();
};


typedef std::map<std::string, std::string> mapValue_t;


static void ReadOrderPos(int64& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static void WriteOrderPos(const int64& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}


/** A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    std::vector<CMerkleTx> vtxPrev;
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived;  // time received by this node
    unsigned int nTimeSmart;
    char fFromMe;
    std::string strFromAccount;
    std::vector<char> vfSpent; // which outputs are already spent
    std::vector<char> vfEnigmaReserved; // which outputs are reserved for Enigma usage
    int64 nOrderPos;  // position in ordered transaction list

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fEnigmaReservedCached;
    mutable bool fChangeCached;
    mutable int64 nDebitCached;
    mutable int64 nCreditCached;
    mutable int64 nAvailableCreditCached;
    mutable int64 nEnigmaReservedCached;
    mutable int64 nChangeCached;

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(const CWallet* pwalletIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        vtxPrev.clear();
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        vfSpent.clear();
        vfEnigmaReserved.clear();
        fDebitCached = false;
        fCreditCached = false;
        fAvailableCreditCached = false;
        fEnigmaReservedCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nEnigmaReservedCached = 0;
        nAvailableCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;

    }

    IMPLEMENT_SERIALIZE
    (
        CWalletTx* pthis = const_cast<CWalletTx*>(this);
        if (fRead)
            pthis->Init(NULL);
        char fSpent = false;

        if (!fRead)
        {
            pthis->mapValue["fromaccount"] = pthis->strFromAccount;

            std::string str;
            BOOST_FOREACH(char f, vfSpent)
            {
                str += (f ? '1' : '0');
                if (f)
                    fSpent = true;
            }
            pthis->mapValue["spent"] = str;

            WriteOrderPos(pthis->nOrderPos, pthis->mapValue);

            if (nTimeSmart)
                pthis->mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        nSerSize += SerReadWrite(s, *(CMerkleTx*)this, nType, nVersion,ser_action);
        READWRITE(vtxPrev);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (fRead)
        {
            pthis->strFromAccount = pthis->mapValue["fromaccount"];

            if (mapValue.count("spent"))
                BOOST_FOREACH(char c, pthis->mapValue["spent"])
                    pthis->vfSpent.push_back(c != '0');
            else
                pthis->vfSpent.assign(vout.size(), fSpent);

            ReadOrderPos(pthis->nOrderPos, pthis->mapValue);

            pthis->nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(pthis->mapValue["timesmart"]) : 0;
        }

        pthis->mapValue.erase("fromaccount");
        pthis->mapValue.erase("version");
        pthis->mapValue.erase("spent");
        pthis->mapValue.erase("n");
        pthis->mapValue.erase("timesmart");
    )

    // marks certain txout's as spent
    // returns true if any update took place
    bool UpdateSpent(const std::vector<char>& vfNewSpent)
    {
        bool fReturn = false;
        for (unsigned int i = 0; i < vfNewSpent.size(); i++)
        {
            if (i == vfSpent.size())
                break;

            if (vfNewSpent[i] && !vfSpent[i])
            {
                vfSpent[i] = true;
                vfEnigmaReserved[i] = false; // as coin is spent, unmark it as reserved for enigma
                fReturn = true;
                fAvailableCreditCached = false;
                fEnigmaReservedCached = false;
            }
        }
        return fReturn;
    }

    // make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    void MarkSpent(unsigned int nOut)
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::MarkSpent() : nOut out of range");
        vfSpent.resize(vout.size());
        if (!vfSpent[nOut])
        {
            vfSpent[nOut] = true;
            fAvailableCreditCached = false;
        }
    }

    void MarkUnspent(unsigned int nOut)
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::MarkUnspent() : nOut out of range");
        vfSpent.resize(vout.size());
        if (vfSpent[nOut])
        {
            vfSpent[nOut] = false;
            fAvailableCreditCached = false;
        }
    }

    void MarkEnigmaReserved(unsigned int nOut)
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::MarkEnigmaReserved() : nOut out of range");
        vfEnigmaReserved.resize(vout.size());
        if (!vfEnigmaReserved[nOut])
        {
            vfEnigmaReserved[nOut] = true;
            fEnigmaReservedCached = false;
        }
    }

    void MarkNotEnigmaReserved(unsigned int nOut)
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::MarkNotEnigmaReserved() : nOut out of range");
        vfEnigmaReserved.resize(vout.size());
        if (vfEnigmaReserved[nOut])
        {
            vfEnigmaReserved[nOut] = false;
            fEnigmaReservedCached = false;
        }
    }

    bool IsEnigmaReserved(unsigned int nOut) const
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::IsEnigmaReserved() : nOut out of range");
        if (nOut >= vfEnigmaReserved.size())
            return false;
        return (!!vfEnigmaReserved[nOut]);
    }

    bool IsSpent(unsigned int nOut) const
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::IsSpent() : nOut out of range");
        if (nOut >= vfSpent.size())
            return false;
        return (!!vfSpent[nOut]);
    }

    int64 GetDebit() const
    {
        if (vin.empty())
            return 0;
        if (fDebitCached)
            return nDebitCached;
        nDebitCached = pwallet->GetDebit(*this);
        fDebitCached = true;
        return nDebitCached;
    }

    int64 GetEnigmaFeeEarnt(const CCloakingRequest req, const CCloakingInputsOutputs inOuts) const
    {
        // we only include an output if it's present in our supplied outputs
        // otherwise it could be a payment to us as part of a enigma tx we helped
        // to cloak

        // TODO: add new output detection

        int64 nCredit = 0;

        BOOST_FOREACH(const CTxOut txoutInOuts, inOuts.vout)
        {
            BOOST_FOREACH(const CTxOut txoutTx, vout)
            {
                if (txoutTx.scriptPubKey == txoutInOuts.scriptPubKey)
                {
                    nCredit += pwallet->GetCredit(txoutTx);
                }
            }
        }

        return nCredit - GetDebit();
    }

    int64 GetCredit(bool fUseCache=true) const
    {
        // Must wait until coinbase is safely deep enough in the chain before valuing it
        if ((IsCoinBase() || IsCoinStake()) && GetBlocksToMaturity() > 0)
            return 0;

        // GetBalance can assume transactions in mapWallet won't change
        if (fUseCache && fCreditCached)
            return nCreditCached;
        nCreditCached = pwallet->GetCredit(*this);
        fCreditCached = true;
        return nCreditCached;
    }

    int64 GetEnigmaReservedAmount(bool fUseCache=true) const
    {
        // Must wait until coinbase is safely deep enough in the chain before valuing it
        if ((IsCoinBase() || IsCoinStake()) && GetBlocksToMaturity() > 0)
            return 0;

        if (fUseCache && fEnigmaReservedCached)
            return nEnigmaReservedCached;

        // check each output and tally the amount reserved for posa
        int64 nReserved = 0;
        for (unsigned int i = 0; i < vout.size(); i++)
        {
            if (!IsSpent(i) && IsEnigmaReserved(i))
            {
                const CTxOut &txout = vout[i];
                nReserved += pwallet->GetCredit(txout);
                if (!MoneyRange(nReserved))
                    throw std::runtime_error("CWalletTx::GetEnigmaReservedAmount() : value out of range");
            }
        }

        nEnigmaReservedCached = nReserved;
        fEnigmaReservedCached = true;
        return nReserved;
    }

    int64 GetAvailableCredit(bool fUseCache=true) const
    {
        // Must wait until coinbase is safely deep enough in the chain before valuing it
        if ((IsCoinBase() || IsCoinStake()) && GetBlocksToMaturity() > 0)
            return 0;

        if (fUseCache && fAvailableCreditCached)
            return nAvailableCreditCached;

        int64 nCredit = 0;
        for (unsigned int i = 0; i < vout.size(); i++)
        {
            if (!IsSpent(i) )
            {
                const CTxOut &txout = vout[i];
                nCredit += pwallet->GetCredit(txout);
                if (!MoneyRange(nCredit))
                    throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
            }
        }

        nAvailableCreditCached = nCredit;
        fAvailableCreditCached = true;
        return nCredit;
    }


    int64 GetChange() const
    {
        if (fChangeCached)
            return nChangeCached;
        nChangeCached = pwallet->GetChange(*this);
        fChangeCached = true;
        return nChangeCached;
    }

    void GetAmounts(int64& nGeneratedImmature, int64& nGeneratedMature, std::list<std::pair<CTxDestination, int64> >& listReceived,
                    std::list<std::pair<CTxDestination, int64> >& listSent, int64& nFee, std::string& strSentAccount) const;

    void GetAccountAmounts(const std::string& strAccount, int64& nGeneratedImmature, int64& nGeneratedMature, int64& nReceived,
                           int64& nSent, int64& nFee) const;

    bool IsFromMe() const
    {
        return (GetDebit() > 0);
    }

    // detect if a transaction is enigma
    bool IsEnigma() const
    {
        bool fundsFromMe = false;
        bool fundsFromOthers = false;
        bool fundsToMe = false;
        bool fundsToOthers = false;

        BOOST_FOREACH(const CTxIn& txin, vin){
            if (pwallet->IsMine(txin))
                fundsFromMe = true;
            else
                fundsFromOthers = true;
        }

        BOOST_FOREACH(const CTxOut& txout, vout){
            if (pwallet->IsMine(txout))
                fundsToMe = true;
            else
                fundsToOthers = true;
        }

        vector<unsigned char> vchEphemPK;
        opcodetype opCode;
        CTxOut txout = this->vout[this->vout.size()-1];
        CScript::const_iterator itTxA = txout.scriptPubKey.begin();

        if (!txout.scriptPubKey.GetOp(itTxA, opCode, vchEphemPK) || opCode != OP_RETURN){
            return false;
        }
        return fundsFromMe && fundsFromOthers && fundsToMe && fundsToOthers;
    }

    bool IsConfirmed() const
    {
        // Quick answer in most cases
        if (!IsFinal())
            return false;
        if (GetDepthInMainChain() >= 1)
            return true;
        if (!IsFromMe()) // using wtx's cached debit
            return false;

        // If no confirmations but it's from us, we can still
        // consider it confirmed if all dependencies are confirmed
        std::map<uint256, const CMerkleTx*> mapPrev;
        std::vector<const CMerkleTx*> vWorkQueue;
        vWorkQueue.reserve(vtxPrev.size()+1);
        vWorkQueue.push_back(this);
        for (unsigned int i = 0; i < vWorkQueue.size(); i++)
        {
            const CMerkleTx* ptx = vWorkQueue[i];

            if (!ptx->IsFinal())
                return false;
            if (ptx->GetDepthInMainChain() >= 1)
                continue;
            if (!pwallet->IsFromMe(*ptx))
                return false;

            if (mapPrev.empty())
            {
                BOOST_FOREACH(const CMerkleTx& tx, vtxPrev)
                    mapPrev[tx.GetHash()] = &tx;
            }

            BOOST_FOREACH(const CTxIn& txin, ptx->vin)
            {
                if (!mapPrev.count(txin.prevout.hash))
                    return false;
                vWorkQueue.push_back(mapPrev[txin.prevout.hash]);
            }
        }
        return true;
    }

    bool WriteToDisk();

    int64 GetTxTime() const;
    int GetRequestCount() const;

    void AddSupportingTransactions(CTxDB& txdb);

    bool AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs=true);
    bool AcceptWalletTransaction();

    void RelayWalletTransaction(CTxDB& txdb);
    void RelayWalletTransaction();
    DBErrors ZapWalletTx();
};

class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn;
    }

    std::string ToString() const
    {
        return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString().substr(0,10).c_str(), i, nDepth, FormatMoney(tx->vout[i].nValue).c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};


/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64 nTimeCreated;
    int64 nTimeExpires;
    std::string strComment;
    //// todo: add something to note what created it (user, getnewaddress, change)
    ////   maybe should have a map<string, string> property map

    CWalletKey(int64 nExpires=0)
    {
        nTimeCreated = (nExpires ? GetTime() : 0);
        nTimeExpires = nExpires;
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(strComment);
    )
};


/** Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    )
};



/** Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    int64 nCreditDebit;
    int64 nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64 nOrderPos;  // position in ordered transaction list
    uint64 nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
    }

    IMPLEMENT_SERIALIZE
    (
        CAccountingEntry& me = *const_cast<CAccountingEntry*>(this);
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        // Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(strOtherAccount);

        if (!fRead)
        {
            WriteOrderPos(nOrderPos, me.mapValue);

            if (!(mapValue.empty() && _ssExtra.empty()))
            {
                CDataStream ss(nType, nVersion);
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                me.strComment.append(ss.str());
            }
        }

        READWRITE(strComment);

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (fRead)
        {
            me.mapValue.clear();
            if (std::string::npos != nSepPos)
            {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), nType, nVersion);
                ss >> me.mapValue;
                me._ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(me.nOrderPos, me.mapValue);
        }
        if (std::string::npos != nSepPos)
            me.strComment.erase(nSepPos);

        me.mapValue.erase("n");
    )

private:
    std::vector<char> _ssExtra;
};

bool GetWalletFile(CWallet* pwallet, std::string &strWalletFileOut);

#endif
