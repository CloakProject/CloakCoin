// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"
#include "init.h"
#include "main.h"
#include "wallet.h"
#include "walletdb.h"
#include "crypter.h"
#include "script.h"
#include "ui_interface.h"
#include "base58.h"
#include "kernel.h"
#include "coincontrol.h"
#include "bitcoinrpc.h"
#include "enigma/enigmaann.h"
#include <boost/algorithm/string/replace.hpp>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <boost/algorithm/string.hpp>
#include "enigma/enigma.h"

#ifdef QT_GUI
#include <QApplication>
#endif

using namespace std;
extern int nStakeMaxAge;
extern CBitcoinAddress GetAccountAddress(string strAccount, bool bForceNew);
extern int nPosaReservedBalancePercent;
extern int nCloakerCountExpired;

std::set<uint256> enigmaQueue; // a list of CWalletTx's
CCriticalSection cs_enigmaQueue; // lock around the queue
CCriticalSection cs_processEnigma; // lock around processing, in case of re-entrancy etc.
bool isEnigmaProcessing = false;
int64 nEnigmaProcessed = 0;
extern CCriticalSection cs_EnigmaAcks;
extern set<uint256> sEnigmaCheckUpAcks;
extern map<uint256, CNode*> mapEnigmaAnnNodes;

// stats for status bar
extern int nCloakerCountAccepted;
extern int nCloakerCountSigned;
extern int nCloakerCountExpired;
extern int nCloakerCountRefused;
extern int nCloakerCountCompleted;
extern int64 nCloakFeesEarnt;

//////////////////////////////////////////////////////////////////////////////
//
// mapWallet
//

struct CompareValueOnly
{
    bool operator()(const pair<int64, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<int64, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

CPubKey CWallet::GenerateNewKey()
{
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    if (!AddKey(key))
        throw std::runtime_error("CWallet::GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool CWallet::AddKey(const CKey& key)
{
    if (!CCryptoKeyStore::AddKey(key))
        return false;
    if (!fFileBacked)
        return true;
    if (!IsCrypted())
        return CWalletDB(strWalletFile).WriteKey(key.GetPubKey(), key.GetPrivKey());
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey, const vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey, vchCryptedSecret);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey, vchCryptedSecret);
    }
    return false;
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

// ppcoin: optional setting to unlock wallet for block minting only;
//         serves to disable the trivial sendmoney when OS account compromised
bool fWalletUnlockMintOnly = false;

bool CWallet::IsValidPassphrase(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (!CCryptoKeyStore::IsValidKey(vMasterKey))
                return false;
        }

        return true;
    }
    return false;
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    if (!IsLocked())
        return false;

    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (!CCryptoKeyStore::Unlock(vMasterKey))
                return false;
        }

        UnlockStealthAddresses(vMasterKey);
        return true;
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        BOOST_FOREACH(MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
            {
                int64 nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                printf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteBestBlock(loc);
}

// This class implements an addrIncoming entry that causes pre-0.4
// clients to crash on startup if reading a private-key-encrypted wallet.
class CCorruptAddress
{
public:
    IMPLEMENT_SERIALIZE
    (
        if (nType & SER_DISK)
            READWRITE(nVersion);
    )
};

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion >= 40000)
        {
            // Versions prior to 0.4.0 did not support the "minversion" record.
            // Use a CCorruptAddress to make them crash instead.
            CCorruptAddress corruptAddress;
            pwalletdb->WriteSetting("addrIncoming", corruptAddress);
        }
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;
    RandAddSeedPerfmon();

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    RAND_bytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    RandAddSeedPerfmon();
    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    RAND_bytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64 nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    printf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin())
                return false;
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked)
                pwalletdbEncryption->TxnAbort();
            exit(1); //We now probably have half of our keys encrypted in memory, and half not...die and let the user reload their unencrypted wallet.
        }


        std::set<CStealthAddress>::iterator it;
        for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
        {
            if (!it->IsOwned())
                continue; // stealth address is not owned
            // -- CStealthAddress is only sorted on spend_pubkey
            CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);

            if (fDebug)
                printf("Encrypting stealth key %s\n", sxAddr.Encoded().c_str());

            std::vector<unsigned char> vchCryptedSecret;

            CSecret vchSecret;
            vchSecret.resize(32);
            memcpy(&vchSecret[0], &sxAddr.spend_secret[0], 32);

            uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
            if (!EncryptSecret(vMasterKey, vchSecret, iv, vchCryptedSecret))
            {
                printf("Error: Failed encrypting stealth key %s\n", sxAddr.Encoded().c_str());
                continue;
            };

            sxAddr.spend_secret = vchCryptedSecret;
            pwalletdbEncryption->WriteStealthAddress(sxAddr);
        };


        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit())
                exit(1); //We now have keys encrypted in memory, but no on disk...die to avoid confusion and let the user reload their unencrypted wallet.

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);

    }
    NotifyStatusChanged(this);

    return true;
}

bool CWallet::EncryptWalletData(const SecureString& strDataPassphrase)
{
    if (!strDataPassphrase.empty())
    {

        CKeyingMaterial vMasterKey;
        RandAddSeedPerfmon();

        vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
        RAND_bytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

        CMasterKey kMasterKey;

        RandAddSeedPerfmon();
        kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
        RAND_bytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

        CCrypter crypter;
        int64 nStartTime = GetTimeMillis();
        crypter.SetKeyFromPassphrase(strDataPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
        kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

        nStartTime = GetTimeMillis();
        crypter.SetKeyFromPassphrase(strDataPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
        kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

        if (kMasterKey.nDeriveIterations < 25000)
            kMasterKey.nDeriveIterations = 25000;

        printf("Encrypting Wallet Data with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

        if (!crypter.SetDataKeyFromPassphrase(strDataPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
            return false;
        if (!crypter.EncryptWalletFile(kMasterKey))
            return false;

        return true;
    }
    else
    {
        return false;
    }
}

bool CWallet::DecryptWalletData(const SecureString& strDataPassphrase)
{
    if (!strDataPassphrase.empty())
    {
        LOCK(cs_wallet); //make sure to restrict access to wallet critical section

        boost::filesystem::path inputPath = GetDataDir() / "wallet.dat";

        FILE *ifp = fopen(inputPath.string().c_str(), "rb");

        if ( NULL == ifp )
            return false;

        long offset = (sizeof("ANORAK_WAS_HERE")+WALLET_CRYPTO_SALT_SIZE+2*sizeof(unsigned int));
        fseek(ifp, -offset, SEEK_END);

        // declare header data
        std::vector<unsigned char> salt;
        unsigned int iterations;
        unsigned int method;
        unsigned char awh[16];

        salt.resize(WALLET_CRYPTO_SALT_SIZE);

        //  READ salt, iterations, method
        /*
        fwrite("ANORAK_WAS_HERE", sizeof("ANORAK_WAS_HERE"), 1, ofp);
        fwrite(kMasterKey.vchSalt, WALLET_CRYPTO_SALT_SIZE, 1, ofp);
        fwrite(kMasterKey.nDeriveIterations, sizeof(unsigned int), 1, ofp);
        fwrite(kMasterKey.nDerivationMethod, sizeof(unsigned int), 1, ofp);
        */

        fread(awh, sizeof(unsigned char), 16, ifp);
        fread(&salt[0], sizeof(unsigned char), WALLET_CRYPTO_SALT_SIZE, ifp);
        fread(&iterations, sizeof(unsigned int), 1, ifp);
        fread(&method, sizeof(unsigned int), 1, ifp);

        fclose(ifp);

        CCrypter crypter;

        // add decryption methods; after retrieving salt, derivation iterations and derivation method, run:
        if (!crypter.SetDataKeyFromPassphrase(strDataPassphrase, salt, iterations, method))
            return false;
        if (!crypter.DecryptWalletFile())
            return false;
        return true;
    }
    else
    {
        return false;
    }
}

int64 CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{
    int64 nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

CWallet::TxItems CWallet::OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount)
{
    CWalletDB walletdb(strWalletFile);

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-order multimap.
    TxItems txOrdered;

    // Note: maintaining indices in the database of (account,time) --> txid and (account, time) --> acentry
    // would make this much faster for applications that do this a lot.
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txOrdered.insert(make_pair(wtx->nOrderPos, TxPair(wtx, (CAccountingEntry*)0)));
    }
    acentries.clear();
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));
    }

    return txOrdered;
}

void CWallet::WalletUpdateSpent(const CTransaction &tx)
{
    // Anytime a signature is successfully verified, it's proof the outpoint is spent.
    // Update the wallet spent flag if it doesn't know due to wallet.dat being
    // restored from backup or the user making copies of wallet.dat.
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
        {
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                CWalletTx& wtx = (*mi).second;
                if (txin.prevout.n >= wtx.vout.size())
                    printf("WalletUpdateSpent: bad wtx %s\n", wtx.GetHash().ToString().c_str());
                else if (!wtx.IsSpent(txin.prevout.n) && IsMine(wtx.vout[txin.prevout.n]))
                {
                    printf("WalletUpdateSpent found spent coin %sMNT %s\n", FormatMoney(wtx.GetCredit()).c_str(), wtx.GetHash().ToString().c_str());
                    wtx.MarkSpent(txin.prevout.n);
                    wtx.WriteToDisk();
                    NotifyTransactionChanged(this, txin.prevout.hash, CT_UPDATED);
                }
            }
        }
    }
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn)
{
    OutputDebugStringF("AddToWalletx ");

    uint256 hash = wtxIn.GetHash();
    {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;

        if (fInsertedNew)
        {
            wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext();

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (wtxIn.hashBlock != 0)
            {
                if (mapBlockIndex.count(wtxIn.hashBlock))
                {
                    unsigned int latestNow = wtx.nTimeReceived;
                    unsigned int latestEntry = 0;
                    {
                        // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64 latestTolerated = latestNow + 300;
                        std::list<CAccountingEntry> acentries;
                        TxItems txOrdered = OrderedTxItems(acentries);
                        for (TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
                        {
                            CWalletTx *const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            CAccountingEntry *const pacentry = (*it).second.second;
                            int64 nSmartTime;
                            if (pwtx)
                            {
                                nSmartTime = pwtx->nTimeSmart;
                                if (!nSmartTime)
                                    nSmartTime = pwtx->nTimeReceived;
                            }
                            else
                                nSmartTime = pacentry->nTime;
                            if (nSmartTime <= latestTolerated)
                            {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }

                    unsigned int& blocktime = mapBlockIndex[wtxIn.hashBlock]->nTime;
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                }
                else
                    printf("AddToWallet() : found %s in block %s not in index\n",
                           wtxIn.GetHash().ToString().substr(0,10).c_str(),
                           wtxIn.hashBlock.ToString().c_str());
            }
        }

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (wtxIn.hashBlock != 0 && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
            fUpdated |= wtx.UpdateSpent(wtxIn.vfSpent);
        }

        //// debug print
        printf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString().substr(0,10).c_str(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk())
                return false;
#ifndef QT_GUI
        // If default receiving address gets used, replace it with a new one
        if (vchDefaultKey.IsValid()) {
            CScript scriptDefaultKey;
            scriptDefaultKey.SetDestination(vchDefaultKey.GetID());
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            {
                if (txout.scriptPubKey == scriptDefaultKey)
                {
                    CPubKey newDefaultKey;
                    if (GetKeyFromPool(newDefaultKey, false))
                    {
                        SetDefaultKey(newDefaultKey);
                        SetAddressBookName(vchDefaultKey.GetID(), "");
                    }
                }
            }
        }
#endif
        // since AddToWallet is called directly for self-originating transactions, check for consumption of own coins
        WalletUpdateSpent(wtx);

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);


        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if ( !strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }

        // notify an external script when a wallet transaction is received
        strCmd = GetArg("-txnotify", "");
        if ( !strCmd.empty() && fInsertedNew)
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }

        // notify an external script when a wallet transaction is confirmed
        strCmd = GetArg("-confirmnotify", "");

        if ( !strCmd.empty() && fUpdated)
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }
    }

    printf("AddToWallet: Success.\n");
    return true;
}

bool CWallet::EnigmaAvailable()
{
    int64 nEnigmaCount = 0;
    for (map<uint160, CEnigmaAnnouncement>::iterator mi = mapEnigmaAnns.begin(); mi != mapEnigmaAnns.end(); mi++)
    {
        const CEnigmaAnnouncement& ann = (*mi).second;
        if(!ann.IsMine())
            nEnigmaCount++;
    }

    return nEnigmaCount > 0;
}

int64 CWallet::EnigmaCount()
{
    return nEnigmaProcessed;
}

bool CWallet::ProcessRemoval(std::set<uint256> removalQueue)
{
    // Lock the enigma queue and process any removals
    LOCK(cs_enigmaQueue);
    BOOST_FOREACH(const uint256 hash, removalQueue)
    {
        enigmaQueue.erase(hash);
    }
    return true;
}

// expire any out-of-time active enigma requests (ours or ones that we participate in)
void CWallet::ExpireOldEnigma()
{
    typedef std::map<uint256, CCloakingRequest> map_t;
    int numActiveRequests = 0;

    TRY_LOCK(cs_mapOurCloakingRequests, lockOurReq);
    if (lockOurReq){
        map_t reqs;
        reqs.insert(mapOurCloakingRequests.begin(), mapOurCloakingRequests.end());
        int idx = 0;
        // abort our expired requests
        BOOST_FOREACH( map_t::value_type &cr, reqs ){
            CCloakingRequest* req = &cr.second;

            bool hasTx = req->txid.compare("0") == 0;
            bool expired = req->IsExpired();

            if (hasTx && !req->completed && !req->aborted && expired){
                if ((int)req->sParticipants.size() >= req->nParticipantsRequired){
                    // if participants didn't sign in time, ban them for a short while
                    BOOST_FOREACH(CCloakingParticipant p, req->sParticipants){
                        if (req->signedByAddresses.find(p.pubKey) == req->signedByAddresses.end()){
                            CCloakShield::GetShield()->BanPeer(p.pubKey, ENIGMA_BANSECS_NO_RESPONSE, "BANNED: failed to provide signature in time");
                        }
                    }
                }

                // retry or abort
                if (req->autoRetry && req->ShouldRetry()){
                    numActiveRequests++;
                    req->Retry();
                }else{
                    req->Abort();
                }
            }
            idx++;
        }

        // handle re-locking wallet
        if (shouldRelockAfterEnigmaSend && GetAdjustedTime() >= minEnigmaRelockTime && numActiveRequests == 0){
            shouldRelockAfterEnigmaSend = false;
            this->Lock();
        }
    }

    TRY_LOCK(cs_mapCloakingRequests, lockReq);
    if (lockReq){
        // abort stored expired requests from others
        BOOST_FOREACH( map_t::value_type &cr, mapCloakingRequests ){
            CCloakingRequest* req = &cr.second;
            bool shouldAbort = req->ShouldAbort();
            if (!req->aborted){
                if (shouldAbort){
                    // request from someone else didn't arrive for signing in time. we want to avoid
                    // repeated abuse, so just update their ban score
                    CCloakShield::GetShield()->BanPeer(req->identifier.pubKeyHex, ENIGMA_BANSECS_NO_RESPONSE, "BANNED: failed to request signature in time");
                    // mark aborted and save, this will also cancel the inputs
                    if (req->Abort())
                    {
                        nCloakerCountExpired++;
                    }
                }
            }
        }
    }
}

// Add a transaction to the wallet, or update it.
// pblock is optional, but should be provided if the transaction is known to be in a block.
// If fUpdate is true, existing transactions will be updated.
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fFindBlock)
{
    uint256 hash = tx.GetHash();
    {
        LOCK(cs_wallet);
        bool fExisted = mapWallet.count(hash);
        if (fExisted && !fUpdate) return false;

        // find out stealth or enigma outputs then generate and store the keys
        // ready for actual processing of the tx
        mapValue_t mapNarr;
        int64 stealthAmountUs = 0;
        int64 enigmaAmountUs = 0;
        FindStealthTransactions(tx, mapNarr, stealthAmountUs, enigmaAmountUs);

        int64 credit = GetCredit(tx);
        int64 debit = GetDebit(tx);

        if (enigmaAmountUs > 0 && debit > 0 && credit > debit){
            nCloakFeesEarnt += credit-debit-stealthAmountUs;
            enigmaFeesEarnt += credit-debit-stealthAmountUs;
            SaveEnigmaFeesEarntToDisk();
        }

        if (fExisted || IsMine(tx) || IsFromMe(tx))
        {
            CWalletTx wtx(this,tx);

            if (!mapNarr.empty())
                wtx.mapValue.insert(mapNarr.begin(), mapNarr.end());

            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(pblock);
            return AddToWallet(wtx);
        }
        else
            WalletUpdateSpent(tx);
    }
    return false;
}

bool CWallet::EraseFromWallet(uint256 hash)
{
    if (!fFileBacked)
        return false;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
    }
    return true;
}


bool CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]))
                    return true;
        }
    }
    return false;
}

int64 CWallet::GetDebit(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]))
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    CTxDestination address;

    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a TX_PUBKEYHASH that is mine but isn't in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (ExtractDestination(txout.scriptPubKey, address) && ::IsMine(*this, address))
    {
        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}

int64 CWalletTx::GetTxTime() const
{
    int64 n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase() || IsCoinStake())
        {
            // Generated block
            if (hashBlock != 0)
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && hashBlock != 0)
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(int64& nGeneratedImmature, int64& nGeneratedMature, list<pair<CTxDestination, int64> >& listReceived,
                           list<pair<CTxDestination, int64> >& listSent, int64& nFee, string& strSentAccount) const
{
    nGeneratedImmature = nGeneratedMature = nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Compute fee:
    int64 nDebit = GetDebit();
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        int64 nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        CTxDestination address;
        vector<unsigned char> vchPubKey;
        if (!ExtractDestination(txout.scriptPubKey, address))
        {
            if(!IsCoinBaseOrStake())
            {
                printf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                   this->GetHash().ToString().c_str());
            }
            continue;
        }

        // Don't report 'change' txouts
        if (nDebit > 0 && pwallet->IsChange(txout))
            continue;

        if (nDebit > 0)
            listSent.push_back(make_pair(address, txout.nValue));

        if (pwallet->IsMine(txout))
        {
            if(IsCoinBaseOrStake())
            {
                if(GetBlocksToMaturity() > 0)
                    nGeneratedImmature += txout.nValue;
                else
                    nGeneratedMature += txout.nValue;
            }
            else
                listReceived.push_back(make_pair(address, txout.nValue));
        }
    }

}

void CWalletTx::GetAccountAmounts(const string& strAccount, int64& nGeneratedImmature, int64& nGeneratedMature, int64& nReceived,
                                  int64& nSent, int64& nFee) const
{
    nGeneratedImmature = nGeneratedMature = nReceived = nSent = nFee = 0;

    int64 allGeneratedImmature, allGeneratedMature, allFee;
    string strSentAccount;
    list<pair<CTxDestination, int64> > listReceived;
    list<pair<CTxDestination, int64> > listSent;
    GetAmounts(allGeneratedImmature, allGeneratedMature, listReceived, listSent, allFee, strSentAccount);

    if (strAccount == strSentAccount)
    {
        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64)& s, listSent)
            nSent += s.second;
        nFee = allFee;
        nGeneratedImmature = allGeneratedImmature;
        nGeneratedMature = allGeneratedMature;
    }
    {
        LOCK(pwallet->cs_wallet);
        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64)& r, listReceived)
        {
            if (pwallet->mapAddressBook.count(r.first))
            {
                map<CTxDestination, string>::const_iterator mi = pwallet->mapAddressBook.find(r.first);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second == strAccount)
                    nReceived += r.second;
            }
            else if (strAccount.empty())
            {
                nReceived += r.second;
            }
        }
    }
}

void CWalletTx::AddSupportingTransactions(CTxDB& txdb)
{
    vtxPrev.clear();

    const int COPY_DEPTH = 3;
    if (SetMerkleBranch() < COPY_DEPTH)
    {
        vector<uint256> vWorkQueue;
        BOOST_FOREACH(const CTxIn& txin, vin)
            vWorkQueue.push_back(txin.prevout.hash);

        // This critsect is OK because txdb is already open
        {
            LOCK(pwallet->cs_wallet);
            map<uint256, const CMerkleTx*> mapWalletPrev;
            set<uint256> setAlreadyDone;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hash = vWorkQueue[i];
                if (setAlreadyDone.count(hash))
                    continue;
                setAlreadyDone.insert(hash);

                CMerkleTx tx;
                map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(hash);
                if (mi != pwallet->mapWallet.end())
                {
                    tx = (*mi).second;
                    BOOST_FOREACH(const CMerkleTx& txWalletPrev, (*mi).second.vtxPrev)
                        mapWalletPrev[txWalletPrev.GetHash()] = &txWalletPrev;
                }
                else if (mapWalletPrev.count(hash))
                {
                    tx = *mapWalletPrev[hash];
                }
                else if (!fClient && txdb.ReadDiskTx(hash, tx))
                {
                    ;
                }
                else
                {
                    printf("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                    continue;
                }

                int nDepth = tx.SetMerkleBranch();
                vtxPrev.push_back(tx);

                if (nDepth < COPY_DEPTH)
                {
                    BOOST_FOREACH(const CTxIn& txin, tx.vin)
                        vWorkQueue.push_back(txin.prevout.hash);
                }
            }
        }
    }

    reverse(vtxPrev.begin(), vtxPrev.end());
}

bool CWallet::WriteBlocksUntilEnigma()
{
    return CWalletDB(strWalletFile).WriteBlocksUntilEnigma(nBlocksUntilEnigma);
}

bool CWalletTx::WriteToDisk()
{
    return CWalletDB(pwallet->strWalletFile).WriteTx(GetHash(), *this);
}

// Scan the block chain (starting in pindexStart) for transactions
// from or to us. If fUpdate is true, found transactions that already
// exist in the wallet will be updated.
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int ret = 0;

    CBlockIndex* pindex = pindexStart;
    {
        LOCK(cs_wallet);
        while (pindex)
        {
            CBlock block;
            block.ReadFromDisk(pindex, true);
            BOOST_FOREACH(CTransaction& tx, block.vtx)
            {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                    ret++;
            }
            pindex = pindex->pnext;
        }
    }
    return ret;
}

int CWallet::ScanForWalletTransaction(const uint256& hashTx)
{
    CTransaction tx;
    tx.ReadFromDisk(COutPoint(hashTx, 0));
    if (AddToWalletIfInvolvingMe(tx, NULL, true, true))
        return 1;
    return 0;
}

void CWallet::ReacceptWalletTransactions()
{
    CTxDB txdb("r");
    bool fRepeat = true;
    while (fRepeat)
    {
        LOCK(cs_wallet);
        fRepeat = false;
        vector<CDiskTxPos> vMissingTx;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            if ((wtx.IsCoinBase() && wtx.IsSpent(0)) || (wtx.IsCoinStake() && wtx.IsSpent(1)))
                continue;

            CTxIndex txindex;
            bool fUpdated = false;
            if (txdb.ReadTxIndex(wtx.GetHash(), txindex))
            {
                // Update fSpent if a tx got spent somewhere else by a copy of wallet.dat
                if (txindex.vSpent.size() != wtx.vout.size())
                {
                    printf("ERROR: ReacceptWalletTransactions() : txindex.vSpent.size() %" PRIszu " != wtx.vout.size() %" PRIszu "\n", txindex.vSpent.size(), wtx.vout.size());
                    continue;
                }
                for (unsigned int i = 0; i < txindex.vSpent.size(); i++)
                {
                    if (wtx.IsSpent(i))
                        continue;
                    if (!txindex.vSpent[i].IsNull() && IsMine(wtx.vout[i]))
                    {
                        wtx.MarkSpent(i);
                        fUpdated = true;
                        vMissingTx.push_back(txindex.vSpent[i]);
                    }
                }
                if (fUpdated)
                {
                    printf("ReacceptWalletTransactions found spent coin %snvc %s\n", FormatMoney(wtx.GetCredit()).c_str(), wtx.GetHash().ToString().c_str());
                    wtx.MarkDirty();
                    wtx.WriteToDisk();
                }
            }
            else
            {
                // Re-accept any txes of ours that aren't already in a block
                // We now check the inputs as there may be a failed Enigma transaction in the wallet
                // which has some inputs used is a subsequent Enigma transaction.
                if (!(wtx.IsCoinBase() || wtx.IsCoinStake()))
                    wtx.AcceptWalletTransaction(txdb, true);
                    //wtx.AcceptWalletTransaction(txdb, false);
            }
        }
        if (!vMissingTx.empty())
        {
            // TODO: optimize this to scan just part of the block chain?
            if (ScanForWalletTransactions(pindexGenesisBlock))
                fRepeat = true;  // Found missing transactions: re-do re-accept.
        }
    }
}

void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    BOOST_FOREACH(const CMerkleTx& tx, vtxPrev)
    {
        if (!(tx.IsCoinBase() || tx.IsCoinStake()))
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
            {
                if(fOnionRouteAll && ONION_ROUTING_ENABLED)
                    CCloakShield::GetShield()->OnionRouteTx(tx);
                else
                    RelayMessage(CInv(MSG_TX, hash), (CTransaction)tx);
            }
        }
    }
    if (!(IsCoinBase() || IsCoinStake()))
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            printf("Relaying wtx %s\n", hash.ToString().substr(0,10).c_str());
            if(fOnionRouteAll && ONION_ROUTING_ENABLED)
                CCloakShield::GetShield()->OnionRouteTx(*this);
            else
                RelayMessage(CInv(MSG_TX, hash), (CTransaction)*this);
        }
    }
}

void CWalletTx::RelayWalletTransaction()
{
   CTxDB txdb("r");
   RelayWalletTransaction(txdb);
}


DBErrors CWallet::ZapWalletTx()
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    DBErrors nZapWalletTxRet = CWalletDB(strWalletFile,"cr+").ZapWalletTx(this);
    if (nZapWalletTxRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    if (nZapWalletTxRet != DB_LOAD_OK)
        return nZapWalletTxRet;

    return DB_LOAD_OK;
}

/*
void CWallet::ResendPosaTransactions()
{
    static int64 nNextPosaTime;
    if(GetTime() < nNextPosaTime)
        return;
    nNextPosaTime = GetTime() + 120;

    // Only do it if there's been a new block since last time
    static int64 nLastPosaTime;
    if (nTimeBestReceived < nLastPosaTime)
        return;
    nLastPosaTime = GetTime();

    // Rebroadcast any of our txes that aren't in a block yet
    CTxDB txdb("r");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            // skip coinbase/coinstake
            if (wtx.GetDepthInMainChain() < 1 && !wtx.IsCoinBaseOrStake())
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }
        BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
        {
            CWalletTx& wtx = *item.second;

            if(GetBoolArg("-printposa"))
                printf("ResendPosaTransactions() : Resending transaction %s\n", wtx.GetHash().ToString().c_str());

            //wtx.RelayWalletTransaction(txdb);
            uint256 hash = wtx.GetHash();
            printf("Relaying wtx %s\n", hash.ToString().substr(0,10).c_str());
            //RelayMessage(CInv(MSG_TX, hash), (CTransaction)wtx);
            CCloakShield::OnionRouteTx((CTransaction)wtx);
        }
    }
}
*/

bool CWallet::SaveEnigmaFeesEarntToDisk()
{
    try
    {
        return CWalletDB(pwalletMain->strWalletFile).WriteEnigmaFeesEarnt(enigmaFeesEarnt);
    }catch (std::exception& e){
        error("CWallet::SaveEnigmaFeesEarntToDisk: %s.", e.what());
    }
    return false;
}

bool CWallet::SaveActiveEnigmaAddressesToDisk()
{
    try
    {
        return CWalletDB(pwalletMain->strWalletFile).WriteActiveEnigmaAddresses(activeEnigmaAddresses);
    }catch (std::exception& e){
        error("CWallet::SaveActiveEnigmaAddressesToDisk: %s.", e.what());
    }
    return false;
}

bool CWallet::SetEnigmaAddressActive(std::string address, bool isActive)
{
    LOCK(cs_activeEnigmaAddresses);
    if (isActive){
        activeEnigmaAddresses.insert(address);
    }else{
        activeEnigmaAddresses.erase(address);
    }
    return SaveActiveEnigmaAddressesToDisk();
}

void CWallet::ResendWalletTransactions()
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    static int64 nNextTime;
    if (GetTime() < nNextTime)
        return;
    bool fFirst = (nNextTime == 0);
    nNextTime = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    static int64 nLastTime;
    if (nTimeBestReceived < nLastTime)
        return;
    nLastTime = GetTime();

    // Rebroadcast any of our txes that aren't in a block yet
    printf("ResendWalletTransactions()\n");
    CTxDB txdb("r");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            // also exclude coinstake/coinbase trasnactions are we don't want to broadcast these outside of a block
            if (nTimeBestReceived - (int64)wtx.nTimeReceived > 5 * 60 && !wtx.IsCoinBaseOrStake())
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }
        BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
        {
            CWalletTx& wtx = *item.second;
            if (wtx.CheckTransaction())
            {
                //assert(wtx.IsCoinBaseOrStake());
                wtx.RelayWalletTransaction(txdb);
            }
            else
                printf("ResendWalletTransactions() : CheckTransaction failed for transaction %s\n", wtx.GetHash().ToString().c_str());
        }
    }
}






//////////////////////////////////////////////////////////////////////////////
//
// Actions
//


int64 CWallet::GetBalance() const
{
    int64 nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsFinal() && pcoin->IsConfirmed())
                nTotal += pcoin->GetAvailableCredit();
        }
    }

    return nTotal;
}

int64 CWallet::GetEnigmaReservedBalance() const
{
    int64 nTotal = 0;
    {
      LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            int64 credit = pcoin->GetAvailableCredit();
            int64 reserved = pcoin->GetEnigmaReservedAmount();
            if (pcoin->IsFinal() && pcoin->IsConfirmed() && (credit == reserved))
                nTotal += pcoin->GetEnigmaReservedAmount();
        }
    }
    return nTotal;
}

int64 CWallet::GetUnconfirmedBalance() const
{
    int64 nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {

            const CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || !pcoin->IsConfirmed())
                nTotal += pcoin->GetAvailableCredit();
        }
    }
    return nTotal;
}

int64 CWallet::GetImmatureBalance() const
{
    int64 nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& pcoin = (*it).second;
            if (pcoin.IsCoinBase() && pcoin.GetBlocksToMaturity() > 0 && pcoin.IsInMainChain())
                nTotal += GetCredit(pcoin);
        }
    }
    return nTotal;
}

// populate vCoins with vector of spendable COutputs
void CWallet::AvailableCoins(vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl *coinControl, const int maxCoinAgeDays) const
{
    vCoins.clear();
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            //int64 nTxTime = pcoin->GetTxTime();

            if (!pcoin->IsFinal())
                continue;

            if (fOnlyConfirmed && !pcoin->IsConfirmed())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            if(pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
                if (!(pcoin->IsEnigmaReserved(i)) &&
                        !(pcoin->IsSpent(i)) && IsMine(pcoin->vout[i]) && pcoin->vout[i].nValue > 0 &&
                 (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                {
                    // check coin age days to ensure using coin would not lose coin age?
                    /*
                    if (maxCoinAgeDays > -1)
                    {
                        int nDayWeightDays = (min((GetAdjustedTime() - nTxTime), (int64) nStakeMaxAge) - nStakeMinAge) / 86400; // 86400 = secs per day
                        if (nDayWeightDays > maxCoinAgeDays)
                        {
                            printf("AvailableCoins - input excluded coinage days %d > max %d\n", nDayWeightDays, maxCoinAgeDays);
                            continue;
                        }
                    }*/
                    vCoins.push_back(COutput(pcoin, i, pcoin->GetDepthInMainChain()));
                }
        }
    }
}

static void ApproximateBestSubset(vector<pair<int64, pair<const CWalletTx*,unsigned int> > >vValue, int64 nTotalLower, int64 nTargetValue,
                                  vector<char>& vfBest, int64& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        int64 nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                if (nPass == 0 ? rand() % 2 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

// ppcoin: total coins staked (non-spendable until maturity)
int64 CWallet::GetStake() const
{
    int64 nTotal = 0;
    LOCK(cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

int64 CWallet::GetNewMint() const
{
    int64 nTotal = 0;
    LOCK(cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

bool CWallet::SelectCoinsMinConf(int64 nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, vector<COutput> vCoins, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<int64, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<int64>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<int64, pair<const CWalletTx*,unsigned int> > > vValue;
    int64 nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    BOOST_FOREACH(COutput output, vCoins)
    {
        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe() ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;

        if (pcoin->nTime > nSpendTime)
            continue;  // ppcoin: timestamp must not exceed spend time

        int64 n = pcoin->vout[i].nValue;

        pair<int64,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n == nTargetValue)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        }
        else if (n < nTargetValue + CENT)
        {
            vValue.push_back(coin);
            nTotalLower += n;
        }
        else if (n < coinLowestLarger.first)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    vector<char> vfBest;
    int64 nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + CENT)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + CENT, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + CENT) || coinLowestLarger.first <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        if (fDebug && GetBoolArg("-printpriority"))
        {
            //// debug print
            printf("SelectCoins() best subset: ");
            for (unsigned int i = 0; i < vValue.size(); i++)
                if (vfBest[i])
                    printf("%s ", FormatMoney(vValue[i].first).c_str());
            printf("total %s\n", FormatMoney(nBest).c_str());
        }
    }

    return true;
}

bool CWallet::SelectCoins(int64 nTargetValue, unsigned int nSpendTime, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64& nValueRet, const CCoinControl* coinControl) const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected())
    {
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    return (SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 6, vCoins, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 1, vCoins, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 0, 1, vCoins, setCoinsRet, nValueRet));
}

bool CWallet::CreateTransaction(const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, int32_t& nChangePos, std::string& strFailReason, bool bAnonymize, const CCoinControl* coinControl, const std::string& txData)
{
    int64 nValue = 0;
    BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
    {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    if (vecSend.empty() || nValue < 0)
        return false;

    wtxNew.BindWallet(this);

    // transaction data
    if (txData.size() > MAX_TX_DATA_SIZE) {
        strFailReason = _("txData is too long");
        return false;
    }

    wtxNew.data = vchFromString(txData.c_str());
    {
        LOCK2(cs_main, cs_wallet);
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            nFeeRet = nTransactionFee;

            while (true)
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;

                int64 nTotalValue = nValue + nFeeRet;
                int64 nFeeForOutputs = 0;
                double dPriority = 0;
                // vouts to the payees
                if (!bAnonymize)
                {
                    BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
                    {
                        // don't create an output for zero coins in data transaction
                        if (0 == s.second && wtxNew.data.size() > 0)
                            continue;

                        wtxNew.vout.push_back(CTxOut(s.second, s.first));
                    }
                }

                printf("ENIGMA: nFeeForOutputs: %" PRI64d "\n", nFeeForOutputs);
                nTotalValue = nTotalValue + nFeeForOutputs;

                printf("ENIGMA: nTotalValue: %" PRI64d "\n", nTotalValue);

                // Choose coins to use
                set<pair<const CWalletTx*,unsigned int> > setCoins;
                int64 nValueIn = 0;
                if (!SelectCoins(nTotalValue, wtxNew.nTime, setCoins, nValueIn, coinControl))
                    return false;

                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
                {
                    int64 nCredit = pcoin.first->vout[pcoin.second].nValue;
                    dPriority += (double)nCredit * pcoin.first->GetDepthInMainChain();
                }

                int64 nChange = nValueIn - nTotalValue;// - nValue - nFeeRet;
                // if sub-cent change is required, the fee must be raised to at least MIN_TX_FEE
                // or until nChange becomes zero
                // NOTE: this depends on the exact behaviour of GetMinFee
                if (nFeeRet < MIN_TX_FEE && nChange > 0 && nChange < CENT)
                {
                    int64 nMoveToFee = min(nChange, MIN_TX_FEE - nFeeRet);
                    nChange -= nMoveToFee;
                    nFeeRet += nMoveToFee;
                }

                // ppcoin: sub-cent change is moved to fee
                if (nChange > 0 && nChange < MIN_TXOUT_AMOUNT)
                {
                    nFeeRet += nChange;
                    nChange = 0;
                }

                if (nChange > 0)
                {
                    // Fill a vout to ourself
                    // TODO: pass in scriptChange instead of reservekey so
                    // change transaction isn't always pay-to-clockcoin-address
                    CScript scriptChange;

                     // coin control: send change to custom address
                     if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
                         scriptChange.SetDestination(coinControl->destChange);

                     // no coin control: send change to newly generated address
                     else
                     {
                         // Note: We use a new key here to keep it from being obvious which side is the change.
                         //  The drawback is that by not reusing a previous key, the change may be lost if a
                         //  backup is restored, if the backup doesn't have the new private key for the change.
                         //  If we reused the old key, it would be possible to add code to look for and
                         //  rediscover unknown transactions that were written with keys of ours to recover
                         //  post-backup change.

                         // Reserve a new key pair from key pool
                         CPubKey vchPubKey;
                         assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked

                         scriptChange.SetDestination(vchPubKey.GetID());
                     }

                     unsigned int nBytes = ::GetSerializeSize(scriptChange, SER_NETWORK, PROTOCOL_VERSION);
                     printf("CHANGESIZE %d", nBytes);
                    wtxNew.vout.push_back(CTxOut(nChange, scriptChange));
                }
                else
                    reservekey.ReturnKey();

                // Fill vin
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

                // Sign
                int nIn = 0;
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    if (!SignSignature(*this, *coin.first, wtxNew, nIn++))
                        return false;

                // Limit size
                unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK, PROTOCOL_VERSION);
                if (nBytes >= MAX_BLOCK_SIZE_GEN/5)
                    return false;
                dPriority /= nBytes;

                // Check that enough fee is included
                int64 nPayFee = nTransactionFee * (1 + (int64)nBytes / 1000);
                int64 nMinFee = wtxNew.GetMinFee(1, false, GMF_SEND, nBytes);

                if (nFeeRet < max(nPayFee, nMinFee))
                {
                    nFeeRet = max(nPayFee, nMinFee);
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;
                break;
            }
        }
    }

    if(GetBoolArg("-printenigma"))
    {
        std::stringstream sso;
        sso << 	wtxNew.vout.size();
        printf("ENIGMA: CreateTransaction(): vouts: %s\n", sso.str().c_str());
    }
    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, std::string& strFailReason, bool bAnonymize, const CCoinControl* coinControl, const std::string& txData)
{
    vector< pair<CScript, int64> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));

    if (sNarr.length() > 0)
    {
        std::vector<uint8_t> vNarr(sNarr.c_str(), sNarr.c_str() + sNarr.length());
        std::vector<uint8_t> vNDesc;

        vNDesc.resize(2);
        vNDesc[0] = 'n';
        vNDesc[1] = 'p';

        CScript scriptN = CScript() << OP_RETURN << vNDesc << OP_RETURN << vNarr;

        vecSend.push_back(make_pair(scriptN, 0));
    }

    // -- CreateTransaction won't place change between value and narr output.
    //    narration output will be for preceding output

    int nChangePos;
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePos, strFailReason, bAnonymize, coinControl, txData);
}


vector<CBitcoinAddress> CWallet::GetExistingAddresses()
{
    LOCK(pwalletMain->cs_wallet);
    vector<CBitcoinAddress> addresses;
    for(std::map<uint256, CWalletTx>::iterator it = this->mapWallet.begin(); it != this->mapWallet.end(); ++it)
    {
        CWalletTx wtx = it->second;
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            if (this->IsMine(txout))
            {
                CTxDestination address;
                if (ExtractDestination(txout.scriptPubKey, address) && ::IsMine(*pwalletMain, address))
                {
                    CBitcoinAddress findAddress(address);
                    if (addresses.empty() || std::find(addresses.begin(), addresses.end(), findAddress) == addresses.end())
                        addresses.push_back(findAddress);
                }
            }
        }
    }
    return addresses;
}


bool CWallet::GetStealthAddress(CStealthAddress& addressOut, std::string enigmaAddress)
{
    std::set<CStealthAddress>::iterator it;
    for (it = pwalletMain->stealthAddresses.begin(); it != pwalletMain->stealthAddresses.end(); ++it)
    {
        if (!it->IsOwned())
            continue;

        const CStealthAddress sa = *it;
        if (sa.Encoded().compare(enigmaAddress) == 0){
            addressOut = *it;
            return true;
        }
    }
    return false;
}

bool CWallet::GetEnigmaAddress(CStealthAddress &address)
{
    // select a random stealth address from our list of 'enigma enabled' stealth addresses
    set<std::string>::iterator it(activeEnigmaAddresses.begin());
    std::advance(it, GetRandInt(activeEnigmaAddresses.size()-1));
    GetStealthAddress(address, *it);
    return true;
}

bool CWallet::GetEnigmaChangeAddresses(const CStealthAddress &sxAddress, const ec_point &stealthRootKey, const uint256 &cloakerHash, uint64 numChangeAddresses, vector<CScript> &scriptsOut, vector<ec_secret> &secretsOut, uint64 offset)
{
    std::string sa = sxAddress.Encoded();

    if(GetBoolArg("-printenigma", false))
        printf("%s generating %d change addresses with hash %s\n", sa.c_str(), numChangeAddresses, cloakerHash.GetHex().c_str());

    for(int i=0; i<numChangeAddresses; i++){
        ec_secret secret;
        if (!GenerateEnigmaSecret(sxAddress, uint256(HexStr(stealthRootKey)), cloakerHash, i+offset, secret)){
            printf("CWallet:GetEnigmaChangeAddresses - GenerateRandomSecret failed for %d.", i);
            return false;
        }
        secretsOut.push_back(secret);
    }

    for(int i=0; i<numChangeAddresses; i++){
        // generate one-time stealth payment addresses for our 'change'
        ec_secret ephem_secret = secretsOut.at(i);
        ec_secret secretShared;
        ec_point pkSendTo;
        ec_point ephem_pubkey;

        if (StealthSecret(ephem_secret, sxAddress.scan_pubkey, sxAddress.spend_pubkey, secretShared, pkSendTo) != 0){
            printf("CWallet:GetEnigmaChangeAddresses - Could not generate receiving public key.");
            return false;
        };

        CPubKey cpkTo(pkSendTo);
        if (!cpkTo.IsValid()){
            printf("CWallet:GetEnigmaChangeAddresses - Invalid public key generated.");
            return false;
        };

        CKeyID ckidTo = cpkTo.GetID();

        CBitcoinAddress addrTo(ckidTo);

        if (SecretToPublicKey(ephem_secret, ephem_pubkey) != 0){
            printf("CWallet:GetEnigmaChangeAddresses - Could not generate ephem public key.");
            return false;
        };

        // -- Parse Bitcoin address
        CScript cs;
        cs.SetDestination(addrTo.Get());
        scriptsOut.push_back(cs);

        if(GetBoolArg("-printenigma", false))
            printf("CA: %s\n", addrTo.ToString().c_str());

    }
    return true;
}

bool CWallet::NewStealthAddress(std::string& sError, std::string& sLabel, CStealthAddress& sxAddr)
{
    ec_secret scan_secret;
    ec_secret spend_secret;

    if (GenerateRandomSecret(scan_secret) != 0
        || GenerateRandomSecret(spend_secret) != 0){
        sError = "GenerateRandomSecret failed.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    ec_point scan_pubkey, spend_pubkey;
    if (SecretToPublicKey(scan_secret, scan_pubkey) != 0){
        sError = "Could not get scan public key.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    if (SecretToPublicKey(spend_secret, spend_pubkey) != 0){
        sError = "Could not get spend public key.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    if (fDebug){
        printf("getnewstealthaddress: ");
        printf("scan_pubkey ");
        for (uint32_t i = 0; i < scan_pubkey.size(); ++i)
          printf("%02x", scan_pubkey[i]);
        printf("\n");

        printf("spend_pubkey ");
        for (uint32_t i = 0; i < spend_pubkey.size(); ++i)
          printf("%02x", spend_pubkey[i]);
        printf("\n");
    };


    sxAddr.label = sLabel;
    sxAddr.scan_pubkey = scan_pubkey;
    sxAddr.spend_pubkey = spend_pubkey;

    sxAddr.scan_secret.resize(32);
    memcpy(&sxAddr.scan_secret[0], &scan_secret.e[0], 32);
    sxAddr.spend_secret.resize(32);
    memcpy(&sxAddr.spend_secret[0], &spend_secret.e[0], 32);

    return true;
}

bool CWallet::AddStealthAddress(CStealthAddress& sxAddr)
{
    LOCK(cs_wallet);

    // must add before changing spend_secret
    stealthAddresses.insert(sxAddr);

    bool fOwned = sxAddr.scan_secret.size() == ec_secret_size;

    if (fOwned){
        // -- owned addresses can only be added when wallet is unlocked
        if (IsLocked()){
            printf("Error: CWallet::AddStealthAddress wallet must be unlocked.\n");
            stealthAddresses.erase(sxAddr);
            return false;
        };

        if (IsCrypted())
        {
            std::vector<unsigned char> vchCryptedSecret;
            CSecret vchSecret;
            vchSecret.resize(32);
            memcpy(&vchSecret[0], &sxAddr.spend_secret[0], 32);

            uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
            if (!EncryptSecret(vMasterKey, vchSecret, iv, vchCryptedSecret))
            {
                printf("Error: Failed encrypting stealth key %s\n", sxAddr.Encoded().c_str());
                stealthAddresses.erase(sxAddr);
                return false;
            };
            sxAddr.spend_secret = vchCryptedSecret;
        };
    };

    if (CWalletDB(strWalletFile).WriteStealthAddress(sxAddr)){
        NotifyAddressBookChanged(this, sxAddr, sxAddr.label, fOwned, CT_NEW);
        return true;
    }

    return false;
}

bool CWallet::UnlockStealthAddresses(const CKeyingMaterial& vMasterKeyIn)
{
    // -- decrypt spend_secret of stealth addresses
    std::set<CStealthAddress>::iterator it;
    for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
    {
        if (!it->IsOwned())
            continue; // stealth address is not owned

        // -- CStealthAddress are only sorted on spend_pubkey
        CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);

        if (fDebug)
            printf("Decrypting stealth key %s\n", sxAddr.Encoded().c_str());

        CSecret vchSecret;
        uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
        if(!DecryptSecret(vMasterKeyIn, sxAddr.spend_secret, iv, vchSecret)
            || vchSecret.size() != 32)
        {
            printf("Error: Failed decrypting stealth key %s\n", sxAddr.Encoded().c_str());
            continue;
        };

        ec_secret testSecret;
        memcpy(&testSecret.e[0], &vchSecret[0], 32);
        ec_point pkSpendTest;

        if (SecretToPublicKey(testSecret, pkSpendTest) != 0
            || pkSpendTest != sxAddr.spend_pubkey)
        {
            printf("Error: Failed decrypting stealth key, public key mismatch %s\n", sxAddr.Encoded().c_str());
            continue;
        };

        sxAddr.spend_secret.resize(32);
        memcpy(&sxAddr.spend_secret[0], &vchSecret[0], 32);
    };

    CryptedKeyMap::iterator mi = mapCryptedKeys.begin();
    for (; mi != mapCryptedKeys.end(); ++mi)
    {
        CPubKey &pubKey = (*mi).second.first;
        std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        if (vchCryptedSecret.size() != 0)
            continue;

        CKeyID ckid = pubKey.GetID();
        CBitcoinAddress addr(ckid);

        StealthKeyMetaMap::iterator mi = mapStealthKeyMeta.find(ckid);
        if (mi == mapStealthKeyMeta.end())
        {
            printf("Error: No metadata found to add secret for %s\n", addr.ToString().c_str());
            continue;
        };

        CStealthKeyMetadata& sxKeyMeta = mi->second;

        CStealthAddress sxFind;
        sxFind.scan_pubkey = sxKeyMeta.pkScan.Raw();

        std::set<CStealthAddress>::iterator si = stealthAddresses.find(sxFind);
        if (si == stealthAddresses.end())
        {
            printf("No stealth key found to add secret for %s\n", addr.ToString().c_str());
            continue;
        };

        if (fDebug)
            printf("Expanding secret for %s\n", addr.ToString().c_str());

        ec_secret sSpendR;
        ec_secret sSpend;
        ec_secret sScan;

        if (si->spend_secret.size() != ec_secret_size
            || si->scan_secret.size() != ec_secret_size)
        {
            printf("Stealth address has no secret key for %s\n", addr.ToString().c_str());
            continue;
        }
        memcpy(&sScan.e[0], &si->scan_secret[0], ec_secret_size);
        memcpy(&sSpend.e[0], &si->spend_secret[0], ec_secret_size);

        ec_point pkEphem = sxKeyMeta.pkEphem.Raw();
        if (StealthSecretSpend(sScan, pkEphem, sSpend, sSpendR) != 0)
        {
            printf("StealthSecretSpend() failed.\n");
            continue;
        };

        ec_point pkTestSpendR;
        if (SecretToPublicKey(sSpendR, pkTestSpendR) != 0)
        {
            printf("SecretToPublicKey() failed.\n");
            continue;
        };

        CSecret vchSecret;
        vchSecret.resize(ec_secret_size);

        memcpy(&vchSecret[0], &sSpendR.e[0], ec_secret_size);
        CKey ckey;

        try {
            ckey.SetSecret(vchSecret, true);
        } catch (std::exception& e) {
            printf("ckey.SetSecret() threw: %s.\n", e.what());
            continue;
        };

        CPubKey cpkT = ckey.GetPubKey();

        if (!cpkT.IsValid())
        {
            printf("cpkT is invalid.\n");
            continue;
        };

        if (cpkT != pubKey)
        {
            printf("Error: Generated secret does not match.\n");
            if (fDebug)
            {
                printf("cpkT   %s\n", HexStr(cpkT.Raw()).c_str());
                printf("pubKey %s\n", HexStr(pubKey.Raw()).c_str());
            };
            continue;
        };

        if (!ckey.IsValid())
        {
            printf("Reconstructed key is invalid.\n");
            continue;
        };

        if (fDebug)
        {
            CKeyID keyID = cpkT.GetID();
            CBitcoinAddress coinAddress(keyID);
            printf("Adding secret to key %s.\n", coinAddress.ToString().c_str());
        };

        if (!AddKey(ckey))
        {
            printf("AddKey failed.\n");
            continue;
        };

        if (!CWalletDB(strWalletFile).EraseStealthKeyMeta(ckid))
            printf("EraseStealthKeyMeta failed for %s\n", addr.ToString().c_str());
    };
    return true;
}

bool CWallet::UpdateStealthAddress(std::string &addr, std::string &label, bool addIfNotExist)
{
    if (fDebug)
        printf("UpdateStealthAddress %s\n", addr.c_str());

    CStealthAddress sxAddr;

    if (!sxAddr.SetEncoded(addr))
        return false;

    //std::set<CStealthAddress>::iterator it;
    //it = stealthAddresses.find(sxAddr);

    ChangeType nMode = CT_UPDATED;
    CStealthAddress sxFound;
    bool bFound = false;

    std::set<CStealthAddress>::iterator it;
    for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
    {
        if(it->Encoded() == addr)
        {
            bFound = true;
            sxFound = const_cast<CStealthAddress&>(*it);
            break;
        }
    }

    if(!bFound) //if (it == stealthAddresses.end())
    {
        if (addIfNotExist)
        {
            sxFound = sxAddr;
            sxFound.label = label;
            stealthAddresses.insert(sxFound);
            nMode = CT_NEW;
        } else
        {
            printf("UpdateStealthAddress %s, not in set\n", addr.c_str());
            return false;
        };
    } else
    {
        /*
        if (sxFound.label == label)
        {
            // no change
            return true;
        };
        */

        sxFound.label = label; //it->label = label; // update in .stealthAddresses

        if (sxFound.scan_secret.size() == ec_secret_size)
        {
            printf("UpdateStealthAddress: todo - update owned stealth address.\n");
            //return false;
        };
    };

    sxFound.label = label;

    if (!CWalletDB(strWalletFile).WriteStealthAddress(sxFound))
    {
        printf("UpdateStealthAddress(%s) Write to db failed.\n", addr.c_str());
        return false;
    };

    bool fOwned = sxFound.scan_secret.size() == ec_secret_size;
    NotifyAddressBookChanged(this, sxFound, sxFound.label, fOwned, nMode);

    return true;
}

bool CWallet::CreateStealthTransaction(CScript scriptPubKey, int64 nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet, const CCoinControl* coinControl)
{
    vector< pair<CScript, int64> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));

    // create zero output with op return and nonce
    CScript scriptP = CScript() << OP_RETURN << P;
    if (narr.size() > 0)
        scriptP = scriptP << OP_RETURN << narr;
    vecSend.push_back(make_pair(scriptP, MIN_TXOUT_AMOUNT));

    // -- shuffle inputs, change output won't mix enough as it must be not fully random for plantext narrations
    std::random_shuffle(vecSend.begin(), vecSend.end(), GetRandInt);

    int nChangePos;
    string strError;
    string txData;
    bool rv = CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePos, strError, false, coinControl, txData);

    // -- the change txn is inserted in a random pos, check here to match narr to output
    if (rv && narr.size() > 0)
    {
        for (unsigned int k = 0; k < wtxNew.vout.size(); ++k)
        {
            if (wtxNew.vout[k].scriptPubKey != scriptPubKey
                || wtxNew.vout[k].nValue != nValue)
                continue;

            char key[64];
            if (snprintf(key, sizeof(key), "n_%u", k) < 1)
            {
                printf("CreateStealthTransaction(): Error creating narration key.");
                break;
            };
            wtxNew.mapValue[key] = sNarr;
            break;
        };
    };

    return rv;
}

string CWallet::SendStealthMoney(CScript scriptPubKey, int64 nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    int64 nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }
    if (fWalletUnlockMintOnly)
    {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }
    if (!CreateStealthTransaction(scriptPubKey, nValue, P, narr, sNarr, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds  "), FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}

bool CWallet::SendStealthMoneyToDestination(CStealthAddress& sxAddress, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, std::string& sError, bool fAskFee)
{
    // -- Check amount
    if (nValue <= 0)
    {
        sError = "Invalid amount";
        return false;
    };
    if (nValue + nTransactionFee > GetBalance())
    {
        sError = "Insufficient funds";
        return false;
    };

    ec_secret ephem_secret;
    ec_secret secretShared;
    ec_point pkSendTo;
    ec_point ephem_pubkey;

    if (GenerateRandomSecret(ephem_secret) != 0)
    {
        sError = "GenerateRandomSecret failed.";
        return false;
    };

    if (StealthSecret(ephem_secret, sxAddress.scan_pubkey, sxAddress.spend_pubkey, secretShared, pkSendTo) != 0)
    {
        sError = "Could not generate receiving public key.";
        return false;
    };

    CPubKey cpkTo(pkSendTo);
    if (!cpkTo.IsValid())
    {
        sError = "Invalid public key generated.";
        return false;
    };

    CKeyID ckidTo = cpkTo.GetID();

    CBitcoinAddress addrTo(ckidTo);

    if (SecretToPublicKey(ephem_secret, ephem_pubkey) != 0)
    {
        sError = "Could not generate ephem public key.";
        return false;
    };

    if (fDebug)
    {
        printf("Stealth send to generated pubkey %" PRIszu ": %s\n", pkSendTo.size(), HexStr(pkSendTo).c_str());
        printf("hash %s\n", addrTo.ToString().c_str());
        printf("ephem_pubkey %" PRIszu ": %s\n", ephem_pubkey.size(), HexStr(ephem_pubkey).c_str());
    };

    std::vector<unsigned char> vchNarr;
    if (sNarr.length() > 0)
    {
        SecMsgCrypter crypter;
        crypter.SetKey(&secretShared.e[0], &ephem_pubkey[0]);

        if (!crypter.Encrypt((uint8_t*)&sNarr[0], sNarr.length(), vchNarr))
        {
            sError = "Narration encryption failed.";
            return false;
        };

        if (vchNarr.size() > 48)
        {
            sError = "Encrypted narration is too long.";
            return false;
        };
    };

    // -- Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(addrTo.Get());

    if ((sError = SendStealthMoney(scriptPubKey, nValue, ephem_pubkey, vchNarr, sNarr, wtxNew, fAskFee)) != "")
        return false;

    return true;
}

uint64 CWallet::GetEnigmaOutputsAmounts(const CTransaction& tx, std::vector<uint8_t>& vchEphemPK)
{
    printf("GetEnigmaOutputsAmounts...\n");
    uint64 amount = 0;
    mapValue_t addressLookup;

    // last output contains stealth root nonce
    CTxOut txoutStealthNonce = tx.vout[tx.vout.size()-1];

    if (txoutStealthNonce.nValue != MIN_TXOUT_AMOUNT)
        return 0;

    CTxDestination stealthPk;
    if (ExtractDestination(txoutStealthNonce.scriptPubKey,  stealthPk))
        return 0;

    std::set<CStealthAddress>::iterator it;
    for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it){
        CStealthAddress sa = *it;

        if (!sa.IsOwned())
            continue;

        vector<CScript> script;
        vector<ec_secret> secrets;
        GetEnigmaChangeAddresses(sa, vchEphemPK, sa.GetEnigmaAddressPrivateHash(), 1, script, secrets, 0);

        CTxDestination onetimeAddress;
        if (!ExtractDestination(script[0], onetimeAddress))
            continue;

        string sxAdd = sa.Encoded();
        string txAdd = CBitcoinAddress(onetimeAddress).ToString();
        printf("%s = %s\n", sxAdd.c_str(), txAdd.c_str());
        addressLookup[txAdd] = sxAdd;
    }

    BOOST_FOREACH(const CTxOut& txout, tx.vout){
        if (txout == txoutStealthNonce){
            continue;
        }

        CTxDestination txAddress;
        if (!ExtractDestination(txout.scriptPubKey,  txAddress))
            continue;

        if (txAddress.type() != typeid(CKeyID))
            continue;

        string txAdd = CBitcoinAddress(txAddress).ToString();
        CStealthAddress stealthAddress;
        const string stealthEncoded = addressLookup[txAdd];
        if (stealthEncoded.length() > 0 && GetStealthAddress(stealthAddress, stealthEncoded)){
            // we've matched our index zero, this is our target stealth address
            printf("1.MATCH %s for %s = %d\n", txAdd.c_str(), stealthAddress.Encoded().c_str(), txout.nValue);
            amount+=txout.nValue;

            vector<CScript> scripts;
            vector<ec_secret> secrets;
            GetEnigmaChangeAddresses(stealthAddress, vchEphemPK, stealthAddress.GetEnigmaAddressPrivateHash(), tx.vout.size(), scripts, secrets, 1);

            set<CTxDestination> generatedKeyIds;

            for (uint i=0; i<scripts.size(); i++){
                CTxDestination add;
                if (!ExtractDestination(scripts[i], add))
                    continue;

                if (add.type() != typeid(CKeyID))
                    continue;

                generatedKeyIds.insert(add);
            }

            BOOST_FOREACH(const CTxOut& txoutInner, tx.vout){
                if (txout==txoutInner)
                    continue;

                CTxDestination txAddressInner;
                if (!ExtractDestination(txoutInner.scriptPubKey,  txAddressInner))
                    continue;

                set<CTxDestination>::iterator dit = generatedKeyIds.find(txAddressInner);
                if (dit != generatedKeyIds.end()){
                    amount+=txoutInner.nValue;
                    printf("2.MATCH %s for %s = %d\n", CBitcoinAddress(txAddressInner).ToString().c_str(), stealthAddress.Encoded().c_str(), txoutInner.nValue);
                }else{
                    printf("2.NO MATCH %s for %s = %d\n", CBitcoinAddress(txAddressInner).ToString().c_str(), stealthAddress.Encoded().c_str(), txoutInner.nValue);
                }
            }
        }
    }

    return amount;
}

bool CWallet::FindStealthTransactionForOutput(const CStealthAddress& stealthAddress, const CTxOut& txoutStealthKey, const CTxOut& txoutB, vector<uint8_t> vchEphemPK, mapValue_t& mapNarr, CScript::const_iterator itTxA, int32_t nOutputId, bool isEnigmaAddress)
{
    ec_secret sSpendR;
    ec_secret sSpend;
    ec_secret sScan;
    ec_secret sShared;

    ec_point pkExtracted;
    unsigned int txFoundCount = 0;
    opcodetype opCode;

    char cbuf[256];
    std::vector<uint8_t> vchENarr;

    CTxDestination address;
    if (!ExtractDestination(txoutB.scriptPubKey, address))
        return false;

    if (address.type() != typeid(CKeyID))
        return false;

    CKeyID ckidMatch = boost::get<CKeyID>(address);

    if (HaveKey(ckidMatch)) // no point checking if already have key
        false;

    //printf("it->Encodeded() %s\n",  it->Encoded().c_str());
    memcpy(&sScan.e[0], &stealthAddress.scan_secret[0], ec_secret_size);

    if (StealthSecret(sScan, vchEphemPK, stealthAddress.spend_pubkey, sShared, pkExtracted) != 0)
    {
        printf("StealthSecret failed.\n");
        return false;
    };
    //printf("pkExtracted %"PRIszu": %s\n", pkExtracted.size(), HexStr(pkExtracted).c_str());

    CPubKey cpkE(pkExtracted);

    if (!cpkE.IsValid())
        return false;

    CKeyID ckidE = cpkE.GetID();

    if (ckidMatch != ckidE) // todo: mismatch
        return false;

    if (fDebug)
        printf("Found stealth txn to address %s\n", stealthAddress.Encoded().c_str());

    if (IsLocked())
    {
        if (fDebug)
            printf("Wallet is locked, adding key without secret.\n");

        // -- add key without secret
        std::vector<uint8_t> vchEmpty;
        AddCryptedKey(cpkE, vchEmpty);
        CKeyID keyId = cpkE.GetID();
        CBitcoinAddress coinAddress(keyId);
        std::string sLabel = stealthAddress.Encoded();
        SetAddressBookName(keyId, sLabel);

        CPubKey cpkEphem(vchEphemPK);
        CPubKey cpkScan(stealthAddress.scan_pubkey);
        CStealthKeyMetadata lockedSkMeta(cpkEphem, cpkScan);

        if (!CWalletDB(strWalletFile).WriteStealthKeyMeta(keyId, lockedSkMeta))
            printf("WriteStealthKeyMeta failed for %s\n", coinAddress.ToString().c_str());

        mapStealthKeyMeta[keyId] = lockedSkMeta;
        if (isEnigmaAddress)
            nFoundEnigma++;
        else
            nFoundStealth++;
        txFoundCount++;
    } else
    {
        if (stealthAddress.spend_secret.size() != ec_secret_size)
            return false;

        memcpy(&sSpend.e[0], &stealthAddress.spend_secret[0], ec_secret_size);

        if (StealthSharedToSecretSpend(sShared, sSpend, sSpendR) != 0)
        {
            printf("StealthSharedToSecretSpend() failed.\n");
            return false;
        };

        ec_point pkTestSpendR;
        if (SecretToPublicKey(sSpendR, pkTestSpendR) != 0)
        {
            printf("SecretToPublicKey() failed.\n");
            return false;
        };

        CSecret vchSecret;
        vchSecret.resize(ec_secret_size);

        memcpy(&vchSecret[0], &sSpendR.e[0], ec_secret_size);
        CKey ckey;

        try {
            ckey.SetSecret(vchSecret, true);
        } catch (std::exception& e) {
            printf("ckey.SetSecret() threw: %s.\n", e.what());
            return false;
        };

        CPubKey cpkT = ckey.GetPubKey();
        if (!cpkT.IsValid())
        {
            printf("cpkT is invalid.\n");
            return false;
        };

        if (!ckey.IsValid())
        {
            printf("Reconstructed key is invalid.\n");
            return false;
        };

        CKeyID keyID = cpkT.GetID();
        if (fDebug)
        {
            CBitcoinAddress coinAddress(keyID);
            printf("Adding key %s.\n", coinAddress.ToString().c_str());
        };

        if (!AddKey(ckey))
        {
            printf("AddKey failed.\n");
            return false;
        };

        std::string sLabel = stealthAddress.Encoded();
        SetAddressBookName(keyID, sLabel);
        if (isEnigmaAddress)
            nFoundEnigma++;
        else
            nFoundStealth++;
        txFoundCount++;
    };

    if (txoutStealthKey.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
            && opCode == OP_RETURN
            && txoutStealthKey.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
            && vchENarr.size() > 0)
    {
        SecMsgCrypter crypter;
        crypter.SetKey(&sShared.e[0], &vchEphemPK[0]);
        std::vector<uint8_t> vchNarr;
        if (crypter.Decrypt(&vchENarr[0], vchENarr.size(), vchNarr))
        {
            std::string sNarr = std::string(vchNarr.begin(), vchNarr.end());
            snprintf(cbuf, sizeof(cbuf), "n_%d", nOutputId);
            mapNarr[cbuf] = sNarr;
        }else if (!isEnigmaAddress){
            // we only fail for standard stealth addresses as enigma narration should only be read by recipient - not cloakers!
            printf("Decrypt narration failed.\n");
            return false;
        }
    };

    return txFoundCount;
}

int64 CWallet::FindEnigmaTransactions(const CTransaction& tx, const CTxOut& txoutStealthKey, std::vector<uint8_t> vchEphemPK, mapValue_t& mapNarr)
{
    int64 amountFound = 0;

    printf("** FIND ENIGMA TRANSACTIONS **\n");
    mapValue_t addressLookupCloaker; // lookups for zero-index cloaker change returns
    mapValue_t addressLookupTarget; // lookups for zero-index recipient outputs

    std::set<CStealthAddress>::iterator it;
    for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it){
        CStealthAddress sa = *it;

        if (!sa.IsOwned())
            continue;

        vector<CScript> script;
        vector<ec_secret> secrets;
        GetEnigmaChangeAddresses(sa, vchEphemPK, sa.GetEnigmaAddressPrivateHash(), 1, script, secrets, 0);

        CTxDestination onetimeAddress;
        if (!ExtractDestination(script[0], onetimeAddress))
            continue;

        string sxAdd = sa.Encoded();
        string txAdd = CBitcoinAddress(onetimeAddress).ToString();
        addressLookupCloaker[txAdd] = sxAdd;
    }

    for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it){
        CStealthAddress sa = *it;

        if (!sa.IsOwned())
            continue;

        vector<CScript> script;
        vector<ec_secret> secrets;
        GetEnigmaChangeAddresses(sa, vchEphemPK, uint256(HexStr(vchEphemPK)), 1, script, secrets, 0);

        CTxDestination onetimeAddress;
        if (!ExtractDestination(script[0], onetimeAddress))
            continue;

        string sxAdd = sa.Encoded();
        string txAdd = CBitcoinAddress(onetimeAddress).ToString();
        addressLookupTarget[txAdd] = sxAdd;
    }

    unsigned int keysFound = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout){

        CTxDestination txAddress;

        if (txout == txoutStealthKey){
            continue;
        }

        if (!ExtractDestination(txout.scriptPubKey,  txAddress))
            continue;

        if (txAddress.type() != typeid(CKeyID))
            continue;

        string outputAdd = CBitcoinAddress(txAddress).ToString();
        CStealthAddress stealthAddress;
        const string stealthEncodedCloaker = addressLookupCloaker[outputAdd];
        const string stealthEncodedTarget = addressLookupTarget[outputAdd];

        uint256 ourHash;
        if (stealthEncodedCloaker.length() > 0 && GetStealthAddress(stealthAddress, stealthEncodedCloaker)){
            //printf("** matched cloaker **\n");
            ourHash = stealthAddress.GetEnigmaAddressPrivateHash();
        }else if (stealthEncodedTarget.length() > 0 && GetStealthAddress(stealthAddress, stealthEncodedTarget)){
            //printf("** matched target **\n");
            ourHash = uint256(HexStr(vchEphemPK));
        }else{
            continue;
        }

        vector<CScript> scripts;
        vector<ec_secret> secrets;
        GetEnigmaChangeAddresses(stealthAddress, vchEphemPK, ourHash, tx.vout.size(), scripts, secrets, 0);

        map<CTxDestination, pair<ec_secret, int> > stealthSecrets;
        for (uint i=0; i<scripts.size(); i++){
            CTxDestination add;
            if (!ExtractDestination(scripts[i], add))
                continue;

            if (add.type() != typeid(CKeyID))
                continue;

            stealthSecrets[add] = make_pair(secrets[i], static_cast<int>(i));
        }

        BOOST_FOREACH(const CTxOut& txoutInner, tx.vout){
            if (txoutInner==txoutStealthKey || txoutInner.nValue == 0)
                continue;

            CTxDestination txAddressInner;
            if (!ExtractDestination(txoutInner.scriptPubKey,  txAddressInner))
                continue;

            if (txAddressInner.type() != typeid(CKeyID))
                continue;

            if (stealthSecrets.count(txAddressInner)){
                pair<ec_secret, int> secretAndIndex = stealthSecrets[txAddressInner];
                ec_point pubKey;
                if (SecretToPublicKey(secretAndIndex.first, pubKey) != 0)
                    continue;

               if (FindStealthTransactionForOutput(stealthAddress, txoutStealthKey, txoutInner, pubKey, mapNarr, txoutInner.scriptPubKey.begin(), secretAndIndex.second, true)){
                   //printf("$$ found enigma for %s = %ld $$\n", innerAdd.c_str(), txoutInner.nValue);
                   amountFound+=txoutInner.nValue;
               }else{
                   // no key index match for this address, skip to next address
                   continue;
               }
            }
        }
    }
    return amountFound;
}

bool CWallet::GetStealthEphemPK(const CTransaction& tx, mapValue_t& mapNarr, std::vector<uint8_t>& vchEphemPK, std::vector<uint8_t>& vchENarr, CScript::const_iterator& itTxA, CTxOut& stealthReturn)
{
    opcodetype opCode;
    char cbuf[256];

    int32_t nOutputIdOuter = -1;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nOutputIdOuter++;
        // -- for each OP_RETURN need to check all other valid outputs

        //printf("txout scriptPubKey %s\n",  txout.scriptPubKey.ToString().c_str());
        itTxA = txout.scriptPubKey.begin();

        if (!txout.scriptPubKey.GetOp(itTxA, opCode, vchEphemPK) || opCode != OP_RETURN){
            continue;
        }else{
            if (!txout.scriptPubKey.GetOp(itTxA, opCode, vchEphemPK) || vchEphemPK.size() != 33)
            {
                // -- look for plaintext narrations
                if (vchEphemPK.size() > 1
                    && vchEphemPK[0] == 'n'
                    && vchEphemPK[1] == 'p')
                {
                    if (txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                        && opCode == OP_RETURN
                        && txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                        && vchENarr.size() > 0)
                    {
                        std::string sNarr = std::string(vchENarr.begin(), vchENarr.end());

                        // plaintext narration always matches preceding value output
                        // enigma transactions don't currently contain narrations, but certainly could using the same derived key technique used to
                        // generate one-time payment addresses from stealth addresses...
                        snprintf(cbuf, sizeof(cbuf), "n_%d", nOutputIdOuter-1);
                        mapNarr[cbuf] = sNarr;
                    } else
                    {
                        printf("Warning: GetStealthEphemPK() tx: %s, Could not extract plaintext narration.\n", tx.GetHash().GetHex().c_str());
                    };
                }
                continue;
            }
        }
        stealthReturn = txout;
        return true;
    }
    return false;
}

int64 CWallet::FindStealthTransactions(const CTransaction& tx, mapValue_t& mapNarr, int64& stealthAmountOut, int64& enigmaAmountOut)
{
    stealthAmountOut=0;
    enigmaAmountOut=0;

    if (fDebug)
        printf("FindStealthTransactions() tx: %s\n", tx.GetHash().GetHex().c_str());

    mapNarr.clear();

    LOCK(cs_wallet);
    {
        std::vector<uint8_t> vchEphemPK;
        std::vector<uint8_t> vchENarr;
        CScript::const_iterator itTxA;
        CTxOut stealthReturn;

        if (!GetStealthEphemPK(tx, mapNarr, vchEphemPK, vchENarr, itTxA, stealthReturn))
            return false;

        int32_t nOutputId = -1;
        BOOST_FOREACH(const CTxOut& txoutB, tx.vout)
        {
            nOutputId++;

            if (&txoutB == &stealthReturn)
                continue;

            bool txnMatch = false; // only 1 txn will match an ephem pk
            //printf("txoutB scriptPubKey %s\n",  txoutB.scriptPubKey.ToString().c_str());

            std::set<CStealthAddress>::iterator it;
            for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
            {
                if (!it->IsOwned())
                    continue;

                if (FindStealthTransactionForOutput(*it, stealthReturn, txoutB, vchEphemPK, mapNarr, itTxA, nOutputId, false)){
                    txnMatch = true;
                    stealthAmountOut+=txoutB.nValue;
                    break;
                }
            };

            if (txnMatch){
                nStealth++;
                break;
            }
        }

        // we have a valid stealth pubkey, let's see if we can derive any enigma one-time payment addresses from it
        enigmaAmountOut += FindEnigmaTransactions(tx, stealthReturn, vchEphemPK, mapNarr);
    };

    return stealthAmountOut + enigmaAmountOut;
};

// check if we have staked within the last [stakeLimitDays]
// this is used to warn the user if Enigma cloaking would lose them staking interest
int CWallet::SecondsSinceWalletLastStaked()
{
    LOCK(cs_wallet);
    int lastStakedSeconds = -1;
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake())
        {
            int txLastStakedSeconds = (GetAdjustedTime() - pcoin->GetTxTime()); // 86400 = secs per day
            if (lastStakedSeconds == -1 || txLastStakedSeconds < lastStakedSeconds)
                lastStakedSeconds = txLastStakedSeconds;
        }
    }
    return lastStakedSeconds;
}

bool CWallet::GetStakeWeight(const CKeyStore& keystore, uint64& nMinWeight, uint64& nMaxWeight, uint64& nWeight)
{
    // todo: should probably return false here if we're processing posa...
    // Choose coins to use
    int64 nBalance = GetBalance();
    int64 nReserveBalance = 0;

    // handle reserve balance from args
    if (mapArgs.count("-reservebalance") && !ParseMoney(mapArgs["-reservebalance"], nReserveBalance))
        return error("CreateCoinStake : invalid reserve balance amount");

    int64 enigmaReserveBalance = (nBalance * nEnigmaReservedBalancePercent)/100;
    if (enigmaReserveBalance > nReserveBalance)
        nReserveBalance = enigmaReserveBalance;

    if (nBalance <= nReserveBalance)
        return false;

    set<pair<const CWalletTx*,unsigned int> > setCoins;
    vector<const CWalletTx*> vwtxPrev;
    int64 nValueIn = 0;

    if (!SelectCoins(nBalance - nReserveBalance, GetTime(), setCoins, nValueIn))
        return false;

    if (setCoins.empty())
        return false;

    CTxDB txdb("r");
    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        CTxIndex txindex;
        {
            LOCK2(cs_main, cs_wallet);
            if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
                continue;
        }

        int64 nTimeWeight = GetWeight((int64)pcoin.first->nTime, (int64)GetTime());
        CBigNum bnCoinDayWeight = CBigNum(pcoin.first->vout[pcoin.second].nValue) * nTimeWeight / COIN / (24 * 60 * 60);

        // Weight is greater than zero
        if (nTimeWeight > 0)
        {
            nWeight += bnCoinDayWeight.getuint64();
        }

        // Weight is greater than zero, but the maximum value isn't reached yet
        if (nTimeWeight > 0 && nTimeWeight < nStakeMaxAge)
        {
            nMinWeight += bnCoinDayWeight.getuint64();
        }

        // Maximum weight was reached
        if (nTimeWeight == nStakeMaxAge)
        {
            nMaxWeight += bnCoinDayWeight.getuint64();
        }
    }

    return true;
}

// ppcoin: create coin stake transaction
bool CWallet::CreateCoinStake(const CKeyStore& keystore, unsigned int nBits, int64 nSearchInterval, CTransaction& txNew)
{
    // The following split & combine thresholds are important to security
    // Should not be adjusted if you don't understand the consequences
    static unsigned int nStakeSplitAge = (60 * 60 * 24 * 30);
    const CBlockIndex* pIndex0 = GetLastBlockIndex(pindexBest, false);
    int64 nCombineThreshold = 0;
    if(pIndex0->pprev)
        nCombineThreshold = GetProofOfWorkReward(pIndex0->nHeight, MIN_TX_FEE, pIndex0->pprev->GetBlockHash()) / 3;

    CBigNum bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);

    txNew.vin.clear();
    txNew.vout.clear();
    // Mark coin stake transaction
    CScript scriptEmpty;
    scriptEmpty.clear();
    txNew.vout.push_back(CTxOut(0, scriptEmpty));
    // Choose coins to use
    int64 nBalance = GetBalance();
    int64 nReserveBalance = 0;
    if (mapArgs.count("-reservebalance") && !ParseMoney(mapArgs["-reservebalance"], nReserveBalance))
        return error("CreateCoinStake : invalid reserve balance amount");
    if (nBalance <= nReserveBalance)
        return false;

    set<pair<const CWalletTx*,unsigned int> > setCoins;
    vector<const CWalletTx*> vwtxPrev;
    int64 nValueIn = 0;
    if (!SelectCoins(nBalance - nReserveBalance, txNew.nTime, setCoins, nValueIn))
        return false;

    if (setCoins.empty())
        return false;

    int64 nCredit = 0;
    CScript scriptPubKeyKernel;
    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        CTxDB txdb("r");
        CTxIndex txindex;
        {
            LOCK2(cs_main, cs_wallet);
            if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
                continue;
        }

        // Read block header
        CBlock block;
        {
            LOCK2(cs_main, cs_wallet);
            if (!block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                continue;
        }

        static int nMaxStakeSearchInterval = 60;

        // printf(">> block.GetBlockTime() = %"PRI64d", nStakeMinAge = %d, txNew.nTime = %d\n", block.GetBlockTime(), nStakeMinAge,txNew.nTime);
        if (block.GetBlockTime() + nStakeMinAge > txNew.nTime - nMaxStakeSearchInterval)
            continue; // only count coins meeting min age requirement

        bool fKernelFound = false;
        for (unsigned int n=0; n<min(nSearchInterval,(int64)nMaxStakeSearchInterval) && !fKernelFound && !fShutdown; n++)
        {
            // printf(">> In.....\n");
            // Search backward in time from the given txNew timestamp
            // Search nSearchInterval seconds back up to nMaxStakeSearchInterval
            uint256 hashProofOfStake = 0;
            COutPoint prevoutStake = COutPoint(pcoin.first->GetHash(), pcoin.second);
            if (CheckStakeKernelHash(nBits, block, txindex.pos.nTxPos - txindex.pos.nBlockPos, *pcoin.first, prevoutStake, txNew.nTime - n, hashProofOfStake))
            {
               // Found a kernel
                if (fDebug && GetBoolArg("-printcoinstake"))
                    printf("CreateCoinStake : kernel found\n");
                vector<valtype> vSolutions;
                txnouttype whichType;
                CScript scriptPubKeyOut;
                scriptPubKeyKernel = pcoin.first->vout[pcoin.second].scriptPubKey;
                if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
                {
                    if (fDebug && GetBoolArg("-printcoinstake"))
                        printf("CreateCoinStake : failed to parse kernel\n");
                    break;
                }
                if (fDebug && GetBoolArg("-printcoinstake"))
                    printf("CreateCoinStake : parsed kernel type=%d\n", whichType);
                if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
                {
                    if (fDebug && GetBoolArg("-printcoinstake"))
                        printf("CreateCoinStake : no support for kernel type=%d\n", whichType);
                    break;  // only support pay to public key and pay to address
                }
                if (whichType == TX_PUBKEYHASH) // pay to address type
                {
                    // convert to pay to public key type
                    CKey key;
                    if (!keystore.GetKey(uint160(vSolutions[0]), key))
                    {
                        if (fDebug && GetBoolArg("-printcoinstake"))
                            printf("CreateCoinStake : failed to get key for kernel type=%d\n", whichType);
                        break;  // unable to find corresponding public key
                    }
                    scriptPubKeyOut << key.GetPubKey() << OP_CHECKSIG;
                }
                else
                    scriptPubKeyOut = scriptPubKeyKernel;

                txNew.nTime -= n;
                txNew.vin.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
                nCredit += pcoin.first->vout[pcoin.second].nValue;

                // printf(">> Wallet: CreateCoinStake: nCredit = %"PRI64d"\n", nCredit);

                vwtxPrev.push_back(pcoin.first);
                txNew.vout.push_back(CTxOut(0, scriptPubKeyOut));
                if (block.GetBlockTime() + nStakeSplitAge > txNew.nTime)
                    txNew.vout.push_back(CTxOut(0, scriptPubKeyOut)); //split stake

                if (fDebug && GetBoolArg("-printcoinstake"))
                    printf("CreateCoinStake : added kernel type=%d\n", whichType);
                fKernelFound = true;
                break;
            }
        }
        if (fKernelFound || fShutdown)
            break; // if kernel is found stop searching
    }
    if (nCredit == 0 || nCredit > nBalance - nReserveBalance)
    {
        // printf(">> Wallet: CreateCoinStake: nCredit = %"PRI64d", nBalance = %"PRI64d", nReserveBalance = %"PRI64d"\n", nCredit, nBalance, nReserveBalance);
        return false;
    }

    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        // Attempt to add more inputs
        // Only add coins of the same key/address as kernel
        if (txNew.vout.size() == 2 && ((pcoin.first->vout[pcoin.second].scriptPubKey == scriptPubKeyKernel || pcoin.first->vout[pcoin.second].scriptPubKey == txNew.vout[1].scriptPubKey))
            && pcoin.first->GetHash() != txNew.vin[0].prevout.hash)
        {
            // Stop adding more inputs if already too many inputs
            if (txNew.vin.size() >= 100)
                break;
            // Stop adding more inputs if value is already pretty significant
            if (nCredit > nCombineThreshold)
                break;
            // Stop adding inputs if reached reserve limit
            if (nCredit + pcoin.first->vout[pcoin.second].nValue > nBalance - nReserveBalance)
                break;
            // Do not add additional significant input
            if (pcoin.first->vout[pcoin.second].nValue > nCombineThreshold)
                continue;
            // Do not add input that is still too young
            if (pcoin.first->nTime + nStakeMaxAge > txNew.nTime)
                continue;
            // Do not add coins that are reserved for Enigma
            if (pcoin.first->IsEnigmaReserved(pcoin.second))
                continue;

            txNew.vin.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
            nCredit += pcoin.first->vout[pcoin.second].nValue;
            vwtxPrev.push_back(pcoin.first);
        }
    }
    // Calculate coin age reward
    {
        uint64 nCoinAge;
        CTxDB txdb("r");
        const CBlockIndex* pIndex0 = GetLastBlockIndex(pindexBest, false);

        if (!txNew.GetCoinAge(txdb, nCoinAge))
            return error("CreateCoinStake : failed to calculate coin age");
        nCredit += GetProofOfStakeReward(nCoinAge, nBits, txNew.nTime, pIndex0->nHeight);
    }

    int64 nMinFee = 0;
    while (true)
    {
        // Set output amount
        if (txNew.vout.size() == 3)
        {
            txNew.vout[1].nValue = ((nCredit - nMinFee) / 2 / CENT) * CENT;
            txNew.vout[2].nValue = nCredit - nMinFee - txNew.vout[1].nValue;
        }
        else
            txNew.vout[1].nValue = nCredit - nMinFee;

        // Sign
        int nIn = 0;
        BOOST_FOREACH(const CWalletTx* pcoin, vwtxPrev)
        {
            if (!SignSignature(*this, *pcoin, txNew, nIn++))
                return error("CreateCoinStake : failed to sign coinstake");
        }

        // Limit size
        unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
        if (nBytes >= MAX_BLOCK_SIZE_GEN/5)
            return error("CreateCoinStake : exceeded coinstake size limit");

        // Check enough fee is paid
        if (nMinFee < txNew.GetMinFee() - MIN_TX_FEE)
        {
            nMinFee = txNew.GetMinFee() - MIN_TX_FEE;
            continue; // try signing again
        }
        else
        {
            if (fDebug && GetBoolArg("-printfee"))
                printf("CreateCoinStake : fee for coinstake %s\n", FormatMoney(nMinFee).c_str());
            break;
        }
    }

    // Successfully generated coinstake
    return true;
}


// Call after CreateTransaction unless you want to abort
bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey)
{
    mapValue_t mapNarr;
    int64 stealthAmountUs = 0;
    int64 enigmaAmountUs = 0;
    FindStealthTransactions(wtxNew, mapNarr, stealthAmountUs, enigmaAmountUs);

    if (!mapNarr.empty())
    {
        BOOST_FOREACH(const PAIRTYPE(string,string)& item, mapNarr)
            wtxNew.mapValue[item.first] = item.second;
    };

    {
        LOCK2(cs_main, cs_wallet);
        printf("CommitTransaction:\n%s", wtxNew.ToString().c_str());
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile,"r") : NULL;

            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Mark old coins as spent
            set<CWalletTx*> setCoins;
            BOOST_FOREACH(const CTxIn& txin, wtxNew.vin)
            {
                CWalletTx &coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                coin.MarkSpent(txin.prevout.n);
                coin.WriteToDisk();
                NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast
        if (!wtxNew.AcceptToMemoryPool())
        {
            // This must not fail. The transaction has already been signed and recorded.
            printf("CommitTransaction() : Error: Transaction not valid");
            return false;
        }
        wtxNew.RelayWalletTransaction();
    }
    return true;
}

string CWallet::SendMoney(CScript scriptPubKey, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, bool bAnonymize, bool fAskFee, const string& txData)
{
    CReserveKey reservekey(this);
    int64 nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }
    if (fWalletUnlockMintOnly)
    {
        string strError = _("Error: Wallet unlocked for block minting only, unable to create transaction.");
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }

    string strError;
    if (!CreateTransaction(scriptPubKey, nValue, sNarr, wtxNew, reservekey, nFeeRequired, strError, bAnonymize, NULL, txData))
    {

        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds  "), FormatMoney(nFeeRequired).c_str());
        else if (strError == "")
            strError = _("Error: Transaction creation failed  ");
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}

string CWallet::SendMoneyToDestination(const CTxDestination& address, int64 nValue, std::string& sNarr, CWalletTx& wtxNew, bool bAnonymize, bool fAskFee, const string& txData)
{
    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");

    if (nValue + nTransactionFee > GetBalance())
        return _("Insufficient funds");

    if (sNarr.length() > 24)
        return _("Narration must be 24 characters or less.");

    // Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address);

    return SendMoney(scriptPubKey, nValue, sNarr, wtxNew, bAnonymize, fAskFee, txData);
}


string CWallet::SendData(CWalletTx& wtxNew, bool fAskFee, const std::string& txData)
{
    // Check amount
    if (nTransactionFee > GetBalance())
        return _("Insufficient funds for fee");

    CReserveKey reservekey(this);
    int64 nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction!");
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }

    string strError;
    string sNarr;
    CScript scriptPubKey;

    if (!CreateTransaction(scriptPubKey, 0, sNarr, wtxNew, reservekey, nFeeRequired, strError, false, NULL, txData))
    {
        if (nFeeRequired > GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!"), FormatMoney(nFeeRequired).c_str());
        printf("SendData() : %s\n", strError.c_str());
        return strError;
    }

    printf("SendData(): nFeeRequired = %d\n", (int)nFeeRequired);

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}


DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    fFirstRunRet = false;
    vector<pair<string, uint256> > vWalletErase;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    NewThread(ThreadFlushWalletDB, &strWalletFile);
    return DB_LOAD_OK;
}


bool CWallet::SetAddressBookName(const CTxDestination& address, const string& strName)
{
    std::map<CTxDestination, std::string>::iterator mi = mapAddressBook.find(address);
    mapAddressBook[address] = strName;
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address), (mi == mapAddressBook.end()) ? CT_NEW : CT_UPDATED);
    if (!fFileBacked)
        return false;

    if (address.which() == CTxDestination_CStealthAddress){
        string addStr = boost::get<CStealthAddress>(address).Encoded();
        return CWalletDB(strWalletFile).WriteName(addStr, strName);
    }else{
        return CWalletDB(strWalletFile).WriteName(CBitcoinAddress(address).ToString(), strName);
    }
}

bool CWallet::DelAddressBookName(const CTxDestination& address)
{
    mapAddressBook.erase(address);
    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address), CT_DELETED);
    if (!fFileBacked)
        return false;

    if (address.which() == CTxDestination_CStealthAddress){
        string addStr = boost::get<CStealthAddress>(address).Encoded();
        return CWalletDB(strWalletFile).EraseName(addStr);
    }else{
        return CWalletDB(strWalletFile).EraseName(CBitcoinAddress(address).ToString());
    }
}

void CWallet::PrintWallet(const CBlock& block)
{
    {
        LOCK(cs_wallet);
        if (block.IsProofOfWork() && mapWallet.count(block.vtx[0].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[0].GetHash()];
            printf("    mine:  %d  %d  %" PRI64d "", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
        }
        if (block.IsProofOfStake() && mapWallet.count(block.vtx[1].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[1].GetHash()];
            printf("    stake: %d  %d  %" PRI64d "", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
         }

    }
    printf("\n");
}

bool CWallet::GetTransaction(const uint256 &hashTx, CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
        {
            wtx = (*mi).second;
            return true;
        }
    }
    return false;
}

bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (fFileBacked)
    {
        if (!CWalletDB(strWalletFile).WriteDefaultKey(vchPubKey))
            return false;
    }
    vchDefaultKey = vchPubKey;
    return true;
}

bool GetWalletFile(CWallet* pwallet, string &strWalletFileOut)
{
    if (!pwallet->fFileBacked)
        return false;
    strWalletFileOut = pwallet->strWalletFile;
    return true;
}

//
// Mark old keypool keys as used,
// and generate all new keys
//
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        BOOST_FOREACH(int64 nIndex, setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64 nKeys = max(GetArg("-keypool", 100), (int64)0);
        for (int i = 0; i < nKeys; i++)
        {
            int64 nIndex = i+1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey()));
            setKeyPool.insert(nIndex);
        }
        printf("CWallet::NewKeyPool wrote %" PRI64d " new keys\n", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool()
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize = max(GetArg("-keypool", 100), 0LL);
        while (setKeyPool.size() < (nTargetSize + 1))
        {
            int64 nEnd = 1;
            if (!setKeyPool.empty())
                nEnd = *(--setKeyPool.end()) + 1;
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey())))
                throw runtime_error("TopUpKeyPool() : writing generated key failed");
            setKeyPool.insert(nEnd);
            printf("keypool added key %" PRI64d ", size=%" PRIszu "\n", nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64& nIndex, CKeyPool& keypool)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool() : read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool() : unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
        if (fDebug && GetBoolArg("-printkeypool"))
            printf("keypool reserve %" PRI64d "\n", nIndex);
    }
}

int64 CWallet::AddReserveKey(const CKeyPool& keypool)
{
    {
        LOCK2(cs_main, cs_wallet);
        CWalletDB walletdb(strWalletFile);

        int64 nIndex = 1 + *(--setKeyPool.end());
        if (!walletdb.WritePool(nIndex, keypool))
            throw runtime_error("AddReserveKey() : writing added key failed");
        setKeyPool.insert(nIndex);
        return nIndex;
    }
    return -1;
}

void CWallet::KeepKey(int64 nIndex)
{
    // Remove from key pool
    if (fFileBacked)
    {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
    if(fDebug)
        printf("keypool keep %" PRI64d "\n", nIndex);
}

void CWallet::ReturnKey(int64 nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
    //if(fDebug)
        //printf("keypool return %"PRI64d"\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, bool fAllowReuse)
{
    int64 nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex == -1)
        {
            if (fAllowReuse && vchDefaultKey.IsValid())
            {
                result = vchDefaultKey;
                return true;
            }
            if (IsLocked()) return false;
            result = GenerateNewKey();
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64 CWallet::GetOldestKeyPoolTime()
{
    int64 nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return GetTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

std::map<CTxDestination, int64> CWallet::GetAddressBalances()
{
    map<CTxDestination, int64> balances;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
        {
            CWalletTx *pcoin = &walletEntry.second;

            if (!pcoin->IsFinal() || !pcoin->IsConfirmed())
                continue;

            if ((pcoin->IsCoinBase() || pcoin->IsCoinStake()) && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe() ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                int64 n = pcoin->IsSpent(i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set< set<CTxDestination> > CWallet::GetAddressGroupings()
{
    set< set<CTxDestination> > groupings;
    set<CTxDestination> grouping;

    BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
    {
        CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0 && IsMine(pcoin->vin[0]))
        {
            // group all input addresses with each other
            BOOST_FOREACH(CTxIn txin, pcoin->vin)
            {
                CTxDestination address;
                if (mapWallet.count(txin.prevout.hash)){
                    CWalletTx prevTx = mapWallet[txin.prevout.hash];

                    if(!ExtractDestination(prevTx.vout[txin.prevout.n].scriptPubKey, address))
                        continue;
                    grouping.insert(address);
                }
            }

            // group change with input addresses
            BOOST_FOREACH(CTxOut txout, pcoin->vout)
                if (IsChange(txout))
                {
                    CWalletTx tx = mapWallet[pcoin->vin[0].prevout.hash];
                    CTxDestination txoutAddr;
                    if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                        continue;
                    grouping.insert(txoutAddr);
                }
            groupings.insert(grouping);
            grouping.clear();
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i]))
            {
                CTxDestination address;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set< set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    map< CTxDestination, set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    BOOST_FOREACH(set<CTxDestination> grouping, groupings)
    {
        // make a set of all the groups hit by this new group
        set< set<CTxDestination>* > hits;
        map< CTxDestination, set<CTxDestination>* >::iterator it;
        BOOST_FOREACH(CTxDestination address, grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        set<CTxDestination>* merged = new set<CTxDestination>(grouping);
        BOOST_FOREACH(set<CTxDestination>* hit, hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        BOOST_FOREACH(CTxDestination element, *merged)
            setmap[element] = merged;
    }

    set< set<CTxDestination> > ret;
    BOOST_FOREACH(set<CTxDestination>* uniqueGrouping, uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

// ppcoin: check 'spent' consistency between wallet and txindex
// ppcoin: fix wallet spent state according to txindex
void CWallet::FixSpentCoins(int& nMismatchFound, int64& nBalanceInQuestion, bool fCheckOnly)
{
    nMismatchFound = 0;
    nBalanceInQuestion = 0;

    LOCK(cs_wallet);
    vector<CWalletTx*> vCoins;
    vCoins.reserve(mapWallet.size());
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        vCoins.push_back(&(*it).second);

    CTxDB txdb("r");
    BOOST_FOREACH(CWalletTx* pcoin, vCoins)
    {
        // Find the corresponding transaction index
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(pcoin->GetHash(), txindex))
            continue;
        for (unsigned int n=0; n < pcoin->vout.size(); n++)
        {
            if (IsMine(pcoin->vout[n]) && pcoin->IsSpent(n) && (txindex.vSpent.size() <= n || txindex.vSpent[n].IsNull()))
            {
                printf("FixSpentCoins found lost coin %sppc %s[%d], %s\n",
                    FormatMoney(pcoin->vout[n].nValue).c_str(), pcoin->GetHash().ToString().c_str(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkUnspent(n);
                    pcoin->WriteToDisk();
                }
            }
            else if (IsMine(pcoin->vout[n]) && !pcoin->IsSpent(n) && (txindex.vSpent.size() > n && !txindex.vSpent[n].IsNull()))
            {
                printf("FixSpentCoins found spent coin %sppc %s[%d], %s\n",
                    FormatMoney(pcoin->vout[n].nValue).c_str(), pcoin->GetHash().ToString().c_str(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkSpent(n);
                    pcoin->WriteToDisk();
                }
            }
        }
    }
}

// ppcoin: disable transaction (only for coinstake)
void CWallet::DisableTransaction(const CTransaction &tx)
{
    if (!tx.IsCoinStake() || !IsFromMe(tx))
        return; // only disconnecting coinstake requires marking input unspent

    LOCK(cs_wallet);
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size() && IsMine(prev.vout[txin.prevout.n]))
            {
                prev.MarkUnspent(txin.prevout.n);
                prev.WriteToDisk();
            }
        }
    }
}

// a enigma request that we created or are participating in has been updated.
void CWallet::EnigmaRequestUpdated(const CCloakingRequest* request)
{
    NotifyEnigmaChanged(this, request);
}

CPubKey CReserveKey::GetReservedKey()
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else
        {
            printf("CReserveKey::GetReservedKey(): Warning: Using default key instead of a new key, top up your keypool!");
            vchPubKey = pwallet->vchDefaultKey;
        }
    }
    assert(vchPubKey.IsValid());
    return vchPubKey;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress)
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    BOOST_FOREACH(const int64& id, setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes() : read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes() : unknown key in key pool");
        setAddress.insert(keyID);
    }
}

void CWallet::UpdatedTransaction(const uint256 &hashTx)
{
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
    {
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
            CWalletTx wtx;
            if (!GetTransaction(hashTx, wtx))
                return;
        }
    }
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            if (pwallet->vchDefaultKey.IsValid()) {
                printf("CReserveKey::GetReservedKey(): Warning: Using default key instead of a new key, top up your keypool!");
                vchPubKey = pwallet->vchDefaultKey;
            } else
                return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

bool CWallet::GetCloakingOutputs(CCloakingInputsOutputs& inOuts, int64 nValue, int64 nChange, uint256 requestHash)
{
    if (nChange > 0 && inOuts.requestIdentifier > uint256(0))
    {
        // create keys for change
        vector<int64> nChangeAmounts;

        // split into amounts that match the sent amount
        int64 changeToSplit = nChange;

        // more change than send amount remains or we've used our change address allocation?
        while(changeToSplit>=nValue && nChangeAmounts.size() <= ENIGMA_CHANGE_ADDRESS_NUM)
        {
            nChangeAmounts.push_back(nValue);
            changeToSplit-=nValue;
        }

        srand(time(NULL));

        // after splitting the change into amounts that match the sent amount, we still have some odd change
        // left to return so handle it...
        if (changeToSplit > 0)
        {
            // only double-split if we have at least twice MIN_TXOUT_AMOUNT left
            bool doDoubleSplit = GetRandBool() && changeToSplit >= MIN_TXOUT_AMOUNT * 2;

            if (doDoubleSplit)
            {
                // use 2 splits for remaining change
                // output must always be at least MIN_TXOUT_AMOUNT
                int64 split1 = max(rand() % changeToSplit, MIN_TXOUT_AMOUNT);
                int64 split2 = changeToSplit - split1;
                nChangeAmounts.push_back(split1);
                nChangeAmounts.push_back(split2);
            }else{
                // use single split
                nChangeAmounts.push_back(changeToSplit);
            }
        }

        int numChangeAddresses = nChangeAmounts.size();
        vector<CScript> scriptChange;
       // if (!pwalletMain->GetEnigmaChangeAddresses(&inOuts, numChangeAddresses, scriptChange)){
       //     return false;
      //  }

        for(int i=0; i<numChangeAddresses; i++)
        {
            inOuts.vout.push_back(CTxOut(nChangeAmounts.at(i), scriptChange.at(i)));
        }
    }

    return true;
}
