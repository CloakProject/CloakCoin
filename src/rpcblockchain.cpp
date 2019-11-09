// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "init.h"
#include "net.h"
#include "wallet.h"
#include "walletdb.h"
#include "bitcoinrpc.h"
#include "alert.h"
#include "enigma/enigmaann.h"
#include "base58.h"
#include <boost/algorithm/string.hpp>

using namespace json_spirit;
using namespace std;

extern CCloakingEncryptionKey* CurrentPosaKey;
extern int64 nNodeStartTime;
extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);
//extern bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv);

extern CBitcoinAddress GetAccountAddress(string strAccount, bool bForceNew);

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (pindexBest == NULL)
            return 1.0;
        else
            blockindex = GetLastBlockIndex(pindexBest, false);
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GetPoSKernelPS()
{
    int nPoSInterval = 72;
    double dStakeKernelsTriedAvg = 0;
    int nStakesHandled = 0, nStakesTime = 0;

    CBlockIndex* pindex = pindexBest;;
    CBlockIndex* pindexPrevStake = NULL;

    while (pindex && nStakesHandled < nPoSInterval)
    {
        if (pindex->IsProofOfStake())
        {
            dStakeKernelsTriedAvg += GetDifficulty(pindex) * 4294967296.0;
            nStakesTime += pindexPrevStake ? (pindexPrevStake->nTime - pindex->nTime) : 0;
            pindexPrevStake = pindex;
            nStakesHandled++;
        }

        pindex = pindex->pprev;
    }

    return nStakesTime ? dStakeKernelsTriedAvg / nStakesTime : 0;
}

Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);
    result.push_back(Pair("confirmations", (int)txGen.GetDepthInMainChain()));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    result.push_back(Pair("mint", ValueFromAmount(blockindex->nMint)));
    result.push_back(Pair("time", (boost::int64_t)block.GetBlockTime()));
    result.push_back(Pair("nonce", (boost::uint64_t)block.nNonce));
    result.push_back(Pair("bits", HexBits(block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    if (blockindex->pnext)
        result.push_back(Pair("nextblockhash", blockindex->pnext->GetBlockHash().GetHex()));

    result.push_back(Pair("flags", strprintf("%s%s", blockindex->IsProofOfStake()? "proof-of-stake" : "proof-of-work", blockindex->GeneratedStakeModifier()? " stake-modifier": "")));
    result.push_back(Pair("proofhash", blockindex->IsProofOfStake()? blockindex->hashProofOfStake.GetHex() : blockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016" PRI64x, blockindex->nStakeModifier)));
    result.push_back(Pair("modifierchecksum", strprintf("%08x", blockindex->nStakeModifierChecksum)));
    Array txinfo;
    BOOST_FOREACH (const CTransaction& tx, block.vtx)
    {
        if (fPrintTransactionDetail)
        {
            Object entry;

            entry.push_back(Pair("txid", tx.GetHash().GetHex()));
            TxToJSON(tx, 0, entry);

            txinfo.push_back(entry);
        }
        else
            txinfo.push_back(tx.GetHash().GetHex());
    }

    result.push_back(Pair("tx", txinfo));
    result.push_back(Pair("signature", HexStr(block.vchBlockSig.begin(), block.vchBlockSig.end())));

    return result;
}


Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "Returns the number of blocks in the longest block chain.");

    return nBestHeight;
}


Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "Returns the difficulty as a multiple of the minimum difficulty.");

    Object obj;
    obj.push_back(Pair("proof-of-work",        GetDifficulty()));
    obj.push_back(Pair("proof-of-stake",       GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("search-interval",      (int)nLastCoinStakeSearchInterval));
    return obj;
}


Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1 || AmountFromValue(params[0]) < MIN_TX_FEE)
        throw runtime_error(
            "settxfee <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.01");

    nTransactionFee = AmountFromValue(params[0]);
    nTransactionFee = (nTransactionFee / CENT) * CENT;  // round to cent

    return true;
}

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getrawmempool\n"
            "Returns all transaction ids in memory pool.");

    vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    Array a;
    BOOST_FOREACH(const uint256& hash, vtxid)
        a.push_back(hash.ToString());

    return a;
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    return pblockindex->phashBlock->GetHex();
}

Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock <hash> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-hash.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

Value getblockbynumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock <number> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-number.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;

    uint256 hash = *pblockindex->phashBlock;

    pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

// ppcoin: get information of sync-checkpoint
Value getcheckpoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getcheckpoint\n"
            "Show info of synchronized checkpoint.\n");

    Object result;
    CBlockIndex* pindexCheckpoint;

    result.push_back(Pair("synccheckpoint", Checkpoints::hashSyncCheckpoint.ToString().c_str()));
    pindexCheckpoint = mapBlockIndex[Checkpoints::hashSyncCheckpoint];        
    result.push_back(Pair("height", pindexCheckpoint->nHeight));
    result.push_back(Pair("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str()));
    if (mapArgs.count("-checkpointkey"))
        result.push_back(Pair("checkpointmaster", true));

    return result;
}

Value dumpenigmap(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "dumpenigmap\n");

    Object result;

    BOOST_FOREACH(const uint256 hash, pwalletMain->setEnigmaProcessed)
    {
	result.push_back(Pair("hash", hash.GetHex()));
    }

    return result;
}



Value enstr(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "enstr <loot>\n");

    Object result;

    string strTxtIn = params[0].get_str();
    string strMarker = "70352205";
    std::stringstream strConcat;
    strConcat << strTxtIn << strMarker;
    
    string strTxt = strConcat.str();
    std::vector<CScript> hashes;
    
    std::stringstream strDecode;
    for(unsigned int i=0; i<=strTxt.length()/40; i++)
    {
        if((unsigned int)i*40 == strTxt.length())
	    break;

	string strPiece = strTxt.substr(i*40,40);
        while(strPiece.length() < 40)
        {
            strPiece = strPiece + "0";
        }
	result.push_back(Pair("piece", strPiece));

        uint160 data(strPiece);
	CScript dataScript;
        dataScript << OP_DUP << OP_HASH160 << data << OP_EQUALVERIFY << OP_CHECKSIG;
	result.push_back(Pair("hash", data.GetHex()));
        result.push_back(Pair("CScript", dataScript.ToString()));
	result.push_back(Pair("CScript datastring", dataScript.ToDataString()));
        uint160 datahash(dataScript.ToDataString());
        result.push_back(Pair("datastring hash", datahash.GetHex()));
	
	std::stringstream strExtract;
        strExtract << dataScript.ToDataString();
        string strExtracted = strExtract.str();
        std::stringstream strReversed;

        for(int i=strExtracted.length()-2;i>=0;i=i-2)
        {
	    strReversed << strExtracted.substr(i,2);
        }

	strDecode << strReversed.str();
        result.push_back(Pair("Extracted", strReversed.str()));
    }

    string strDecoded = strDecode.str();
    string strOutput = strDecoded.substr(0, strDecoded.find(strMarker));
    result.push_back(Pair("Decoded", strOutput));

    CEnigmaTransaction enigmaTx;
    CDataStream ssTx(ParseHex(strOutput), SER_NETWORK, PROTOCOL_VERSION); // ssTx(SER_NETWORK, PROTOCOL_VERSION);
    //ssTx << ParseHex(strOutput); //ParseHex(strOutput);
    ssTx >> enigmaTx;

    result.push_back(Pair("ENIGMA TX", enigmaTx.ToString()));
    return result;
}

Value destr(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "destr <loot>\n");

    Object result;

    return result;
}

Value hexposa(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "hexenigma <amount> <realdestinationaddress> <phase>\n");

    // TODO: this looks b0rked/iffy - investigate!
    Object result;
    int hop = 0; //params[2].get_int();
    string strAddress = "mkgwGsCLCWDpFKXWWTiDn4xcvzDicHapDt";// params[1].get_str();

    // Amount
    int64 nAmount = AmountFromValue(2.12345); //AmountFromValue(params[0]);

    if (nAmount < MIN_TXOUT_AMOUNT)
        throw JSONRPCError(-101, "Send amount too small");

    CBitcoinAddress address(strAddress);
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid CloakCoin address");

    result.push_back(Pair("addr", strAddress));
    //result.push_back(Pair("amount", nAmount.ToString()));
    result.push_back(Pair("hop", hop));

    CScript posaScriptPubKey;
    posaScriptPubKey.SetDestination(address.Get());

    // Create enigma transaction object
    CEnigmaTransaction enigmaTx;
    CEnigmaTxOut enigmaTxOut;

    enigmaTxOut.nHop = hop;
    enigmaTxOut.nValue = nAmount;
    enigmaTxOut.scriptPubKey = posaScriptPubKey;

    enigmaTx.vout.push_back(enigmaTxOut);
    // Serialize the enigma transaction object to a string
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << enigmaTx;
    
    // Get the hex formatted string
    string enigmatxstring = HexStr(ssTx.begin(), ssTx.end());

    // populate result
    result.push_back(Pair("enigma", enigmatxstring));
    result.push_back(Pair("ENIGMA TX", enigmaTx.ToString()));
    return result;	
}

// send our Enigma details to the Enigma network so other nodes are aware of us.
void broadcastenigma()
{
    if(!fEnigma)
        return;

    if(GetBoolArg("-printenigma"))
        printf("ENIGMA: broadcasting Enigma announcement\n");

    if (!CCloakShield::GetShield()){
        // cloakshield not initialized yet
        return;
    }

    CCloakingEncryptionKey* key = CCloakShield::GetShield()->GetRoutingKey();

    if(GetBoolArg("-printenigma"))
        printf("ENIGMA: Enigma pubkey: %s\n", key->pubkeyHex.c_str());

    // create the ann
    CEnigmaAnnouncement ann(key);

    if(GetBoolArg("-printenigma"))
        printf("ENIGMA: Constructing ann\n");

    nMyEnigmaAddress = ann.pEnigmaSessionKeyHex;

    if(GetBoolArg("-printenigma")){
        printf("ENIGMA: ann: %s\n", ann.GetSigningHash().ToString().c_str());
        printf("ENIGMA: ann process message\n");
    }

    if(GetBoolArg("-printenigma")){
        printf("ENIGMA: relaying to peers.\n");
    }

    mapEnigmaAnns[ann.GetKeyHash()] = ann;

    // Relay
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        ann.RelayTo(pnode);
    }
}
