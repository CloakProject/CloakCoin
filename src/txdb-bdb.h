// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef TXDB_BDB_H
#define TXDB_BDB_H


/** Access to the transaction database (blkindex.dat) */
class CTxDB : public CDB
{
public:
    CTxDB(const char* pszMode="r+", const char* filename = "blkindex.dat") : CDB(filename, pszMode) { }
private:
    CTxDB(const CTxDB&);
    void operator=(const CTxDB&);
public:

    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust);
    bool WriteBestInvalidTrust(CBigNum bnBestInvalidTrust);
    bool ReadSyncCheckpoint(uint256& hashCheckpoint);
    bool WriteSyncCheckpoint(uint256 hashCheckpoint);
    bool ReadCheckpointPubKey(std::string& strPubKey);
    bool WriteCheckpointPubKey(const std::string& strPubKey);
    bool ReadModifierUpgradeTime(unsigned int& nUpgradeTime);
    bool WriteModifierUpgradeTime(const unsigned int& nUpgradeTime);
    bool LoadBlockIndex();
private:
    bool LoadBlockIndexGuts();
};

// Called from the initialization code. Checks to see if there is an old
// blkindex.dat file. If so, deletes it and begins re-importing the block
// chain, which will create the new database.
enum BerkeleyDBUpgradeResult {
  NONE_NEEDED,
  INSUFFICIENT_DISK_SPACE,
  COMPLETED,
  OTHER_ERROR,
};

/*
BerkeleyDBUpgradeResult UpgradeBerkeleyDB(BerkerleyDBUpgradeProgress &progress);
*/
#endif
