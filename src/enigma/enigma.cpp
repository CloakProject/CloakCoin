// Copyright (c) 2014 The CloakCoin Developers
//
// Enigma Cloaking System

/*
beta 2:
1. Split output (change) amounts into payments that match the target send amount whenever possible.
2. Split Enigma reward according to number of supplied cloaking outputs. This ensures that Enigma nodes that supply
a greater input amount (and more change addresses) receive a higher fee. This automatically penalizes any nodes
that deliberately supply small amounts of change addresses in an attempt to hamper cloaking.
3. Route all traffic via Cloak shield and add options to Options dialog to control/toggle this functionality.
4. Ban nodes that refuse to sign.
5. Ban nodes that supply bad inputs/outputs.
*/
#include "txdb.h"
#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <map>
#include "enigma.h"
//#include "enigmaann.h"
#include "main.h"
#include "key.h"
#include "net.h"
#include "init.h"
#include "util.h"
#include "wallet.h"
#include "ui_interface.h"

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <openssl/ecdh.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>
//#include <openssl/ecies.h>

using namespace std;

int nNextEnigmaHeight = -1; // block # before we can send another enigma transaction. only 1 per block allowed atm.

extern string nMyEnigmaAddress;
extern int64 nEnigmaProcessed;
extern std::map<std::string, int> peerDosCounts; // key = public key

// stats for status bar
int nCloakerCountAccepted=0;
int nCloakerCountSigned=0;
int nCloakerCountExpired=0;
int nCloakerCountRefused=0;
int nCloakerCountCompleted=0;
int64 nCloakFeesEarnt=0;

extern int nCloakShieldNumHops;
extern int nCloakShieldNumNodes;
extern int nCloakShieldNumRoutes;


void ThreadEnigma2(void* parg)
{
    // ... or if IRC is not enabled.
    if (!GetBoolArg("-enigma", true))
        return;

    printf("ThreadEnigma started\n");

    while (!fShutdown)
    {        
        try
        {
            // expire any enigma requests that have exceed their time limit
            pwalletMain->ExpireOldEnigma();
            CEnigmaAnnouncement::CullEnigmaAnns();
        }catch (std::exception& e){
            error("ExpireOldEnigma failed: %s.", e.what());
        }
        Sleep(1000); // sleep for 5 secs
    }
}

void ThreadEnigma(void* parg)
{
    // Make this thread recognisable as the IRC seeding thread
    RenameThread("enigma-monitor");

    try
    {
        ThreadEnigma2(parg);
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "ThreadEnigma()");
    } catch (...) {
        PrintExceptionContinue(NULL, "ThreadEnigma()");
    }
    printf("ThreadEnigma exited\n");
}

// decode CCloakingAcceptResponse from CCloakingData object
void CCloakingMessage::SetNull()
{
    hash = uint256(0);
    messageCode = 0;
    messageData = "";
}

// decode CCloakingAcceptResponse from CCloakingData object
void CCloakingSignResponse::SetNull()
{
    signingAddress = "";
    identifier = uint256(0);
    signResponseId = uint256(0);
    signatures.clear();
    hash = uint256(0);
}

uint256 CCloakingSignResponse::GetHash() const
{
    if (hash == 0){
        //hash = SerializeHash(*this);
    }
    return SerializeHash(*this);
}


// process a signature response from a cloaker who has signed our Enigma transaction.
bool CCloakingSignResponse::ProcessSignatureResponse(CCloakingEncryptionKey* myKey)
{
    try
    {
        CCloakingRequest* r;
        {
            LOCK(pwalletMain->cs_mapOurCloakingRequests);
            if(pwalletMain->mapOurCloakingRequests.count(identifier) == 0)
            {
                printf("ENIGMA: ProcessSignatureResponse - not our request!\n");
                return false; // we don't have this request, it's not one of ours
            }

            r = &pwalletMain->mapOurCloakingRequests[identifier];
        }

        // if the request has been aborted at our end we should abort the signing process
        if (r->aborted)
        {
            printf("ENIGMA: ProcessSignatureResponse - aborted!\n");
            return false;
        }

        // already signed by this address?
        if(r->signedByAddresses.count(signingAddress) > 0)
        {
            printf("ENIGMA: ProcessSignatureResponse - already signed!\n");
            return false;
        }

        // merge the signed inputs from this cloaker into our local version of the final Enigma Tx
        r->AddSignedInputsToTx(this);

        if ((int)r->signedByAddresses.size() >= r->nParticipantsRequired && !r->processed)
        {
            // all participants have signed, self sign
            // this will handle submitting the Tx via cloak shield
            if (r->SignAndRespond(myKey, false))
                r->processed = true;

            return true;
        }
    }
    catch (std::exception& e) {
#ifdef ENIGMA_ASSERTS
        assert(false);
#endif
        error("ProcessSignatureResponse: %s", e.what());
    }
    return false;
}

void CCloakingParticipant::SetNull()
{
    pubKey = "";
    inOuts.SetNull();
    //destinations.clear();
}

uint256 CCloakingParticipant::GetHash() const
{
    return SerializeHash(*this);
}

void CCloakingIdentifier::SetNull()
{
    pubKeyHex = "";
    nInitiatedTime = 0;
    hash = uint256(0);
}

uint256 CCloakingIdentifier::GetHash() const
{
    if (hash == 0){
        //hash = SerializeHash(*this);
    }
    return SerializeHash(*this);
}

void CCloakingAcceptResponse::SetNull()
{
    requestIdentifier = uint256(0);
    hash = uint256(0);
    participantInfo.SetNull();
}

// check inputs/outputs are valid for use
// will log DoS score against Enigma node that matches [ownerPubKey].
bool CCloakingInputsOutputs::CheckValid(int64 minInputAmount, bool printToLog, std::string ownerPubKey)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        int64 nValueIn = 0;
        //int64 nValueOut = 0;
        //int64 nFees = 0;

        CTransaction txTemp;
        txTemp.vin = this->vin;
        txTemp.vout = this->vout;

        CTxDB txdb("r");

        MapPrevTx mapInputs;
        map<uint256, CTxIndex> mapUnused;
        bool fInvalid = false;

        if (!txTemp.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid))
        {
            if (printToLog)
                error("ERROR CCloakingInputsOutputs::CheckValid() : Failed to fetch inputs (%d)", fInvalid);
            return false;
        }

        for (uint i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;

            if (mapInputs.count(prevout.hash) == 0)
                return false;

            CTxIndex& txindex = mapInputs[prevout.hash].first;
            CTransaction& txPrev = mapInputs[prevout.hash].second;

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
            {
                if (printToLog)
                    error("CCloakingInputsOutputs::CheckValid() : prevout.n out of range.");

                string msg = strprintf("prevout.n out of range %d %" PRIszu " %" PRIszu " prev tx %s\n%s",
                                            prevout.n,
                                            txPrev.vout.size(),
                                            txindex.vSpent.size(),
                                            prevout.hash.ToString().substr(0,10).c_str(),
                                            txPrev.ToString().c_str());
                if (ownerPubKey.length())
                    cs->BanPeer(ownerPubKey, ENIGMA_BANSECS_MISSING_INPUTS, msg);
                return false;
            }

            // If prev is coinbase or coinstake, check that it's matured
            if (txPrev.IsCoinBase() || txPrev.IsCoinStake())
                for (const CBlockIndex* pindex = pindexBest; pindex && pindexBest->nHeight - pindex->nHeight < nCoinbaseMaturity; pindex = pindex->pprev)
                    if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
                    {
                        char msg[255];
                        sprintf(msg, "tried to spend immature %s", txPrev.IsCoinBase() ? "coinbase" : "coinstake");

                        if (printToLog)
                        {
                            std::string s(msg);
                            error("CCloakingInputsOutputs::CheckValid() : %s", msg);
                        }

                        if (ownerPubKey.length())
                            cs->BanPeer(ownerPubKey, ENIGMA_BANSECS_MISSING_INPUTS, string(msg));
                        return false;
                    }

            // ppcoin: check transaction timestamp
            if (txPrev.nTime > GetAdjustedTime())
            {
                if (ownerPubKey.length())
                    cs->BanPeer(ownerPubKey, ENIGMA_BANSECS_MISSING_INPUTS, "transaction timestamp earlier than input transaction");
                return false;
            }

            // Check for negative or overflow input values
            nValueIn += txPrev.vout[prevout.n].nValue;
            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
            {
                if (ownerPubKey.length())
                    cs->BanPeer(ownerPubKey, ENIGMA_BANSECS_MISSING_INPUTS, "txin values out of range");
                return false;
            }
        }

        if (nValueIn < minInputAmount){
            if (ownerPubKey.length())
                cs->BanPeer(ownerPubKey, ENIGMA_BANSECS_MISSING_INPUTS, "txin values too small");
        }

        // store the sent amount and check it's valid
        nInputAmount = nValueIn;
        return true;
    }
    catch (std::exception &e)
    {
        error("CheckValid: %s", e.what());
    }
    return false;
}

// get a list of bitcoin addresses that are paying into this collection of inputs/outputs
vector<CBitcoinAddress> CCloakingInputsOutputs::addressesIn()
{
    vector<CBitcoinAddress> addresses;
    try
    {

        for(uint i=0; i<vin.size(); i++)
        {
            CTxIn in = vin[i];
            if (pwalletMain->IsMine(in))
            {
                COutput cout(&pwalletMain->mapWallet[in.prevout.hash], in.prevout.n, 0);
                CTxDestination address;
                if(!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address))
                    continue;
                addresses.push_back(CBitcoinAddress(address));
            }
        }
    }
    catch (std::exception &e)
    {
        error("addressesIn: %s", e.what());
    }
    return addresses;
}

// get a list of bitcoin addresses that are paid out from this collection of inputs/outputs
vector<CBitcoinAddress> CCloakingInputsOutputs::addressesOut()
{
    vector<CBitcoinAddress> addresses;
    try
    {
        for(uint i=0; i<vout.size(); i++)
        {
            CTxOut out = vout[i];
            if (pwalletMain->IsMine(out))
            {
                CTxDestination address;
                if(!ExtractDestination(out.scriptPubKey, address))
                    continue;
                addresses.push_back(CBitcoinAddress(address));
            }
        }
    }
    catch (std::exception &e)
    {
        error("addressesOut: %s", e.what());
    }
    return addresses;
}

// split the cloaking participation reward amongst the participants
bool CCloakingInputsOutputs::assignReward(int64 rewardTotal, int64 sendAmount)
{
    try
    {
        // handle reward.
        // split the fee amongst the recipients
        int64 nRewardToSplit = rewardTotal;
        //int64 nRewardMin = nRewardToSplit / this->vout.size() / 2;

        bool assignToAny = false;
        while (nRewardToSplit > 0)
        {
            bool foundMatch = false;
            for(uint i=0; i<this->vout.size(); i++)
            {
                CTxOut* txout = &(this->vout[i]);
                // this output matches the true send amount?
                // then leave intact (no added fee) for cloaking purposes
                if (txout->nValue == sendAmount && !assignToAny)
                    continue;

                if (nRewardToSplit == 0)
                    break;

                int64 amt = GetRandInt(nRewardToSplit/2)+1;
                if (amt > nRewardToSplit)
                    amt = nRewardToSplit;

                txout->nValue += amt;
                nRewardToSplit -= amt;

                foundMatch = true;
            }

            // if we have no outputs that DON'T match the amount of the send
            // then we just assign the rewards to any outputs
            // note: we may want to bail out in future instead...
            if (!foundMatch)
                assignToAny = true;
        }
        return true;
    }
    catch (std::exception &e)
    {
        error("assignReward: %s", e.what());
    }
    return false;
}

bool CCloakingInputsOutputs::addVinCreateStealthVouts(const CCloakingParticipant &cloaker, const uint256 &randomHash)
{
    try
    {
        /*
        int64 inputAmount = cloaker.inOuts.nSendAmount;

        // add the inputs
        BOOST_FOREACH(CTxIn txin, cloaker.inOuts.vin){
            vin.push_back(txin);
        }

        // create and store the stealth outputs



*/
        return true;
    }catch (std::exception& e){
        error("CCloakingInputsOutputs::addVinVouts failed: %s.", e.what());
    }
    return false;
}

bool CCloakingInputsOutputs::addVinVouts(vector<CCloakingInputsOutputs> inouts, bool addInputs, bool addOuputs)
{
    BOOST_FOREACH(CCloakingInputsOutputs item, inouts){
        addVinVouts(inouts, addInputs, addOuputs);
    }
}

bool CCloakingInputsOutputs::addVinVouts(CCloakingInputsOutputs inouts, bool addInputs, bool addOuputs)
{
    try
    {
        if (addInputs){
            BOOST_FOREACH(CTxIn txin, inouts.vin)
                vin.push_back(txin);
        }

        if (addOuputs){
            BOOST_FOREACH(CTxOut txout, inouts.vout)
                vout.push_back(txout);
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingInputsOutputs::addVinVouts failed: %s.", e.what());
    }
    return false;
}

// shuffle the inputs and outputs
bool CCloakingInputsOutputs::shuffle(int numSwaps)
{
    try
    {
        for(int i=0; i<numSwaps; i++)
        {
            int inPos1 = GetRandInt(vin.size());
            int inPos2 = GetRandInt(vin.size());
            int outPos1 = GetRandInt(vout.size());
            int outPos2 = GetRandInt(vout.size());

            CTxIn vi = vin[inPos1];
            vin[inPos1] = vin[inPos2];
            vin[inPos2] = vi;

            CTxOut vo = vout[outPos1];
            vout[outPos1] = vout[outPos2];
            vout[outPos2] = vo;
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingInputsOutputs::shuffle failed: %s.", e.what());
    }
    return false;
}

// add this set of inputs/outputs to a transaction (vin/vout)
bool CCloakingInputsOutputs::addVinVoutsToTx(CTransaction* tx, bool addInputs, bool addOuputs)
{
    try
    {
        if (addInputs){
            BOOST_FOREACH(const CTxIn inv, vin)
                    tx->vin.push_back(inv);
        }

        if (addOuputs){
            BOOST_FOREACH(const CTxOut outv, vout)
                    tx->vout.push_back(outv);
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingInputsOutputs::addVinVoutsToTx failed: %s.", e.what());
    }
    return false;
}

uint256 CCloakingAcceptResponse::GetHash() const
{
    if (hash == 0){
        //hash = SerializeHash(*this);
    }
    return SerializeHash(*this);
}

// process response from cloaker that wants to participate in cloaking an enigma tx
bool CCloakingAcceptResponse::ProcessAcceptResponse(CCloakingEncryptionKey* myKey)
{
    try
    {
        // 1. find request in request list
        // 2. add the participant to the cloaking request
        // 3. relay update

        // check the inputs and outputs are valid and that at least one output matches the amount being cloaked
        CCloakingRequest* request = NULL;
        {
            LOCK(pwalletMain->cs_mapOurCloakingRequests);
            request = &pwalletMain->mapOurCloakingRequests[requestIdentifier];
        }

        // check stealth address provided by user is valid
        if (!IsStealthAddress(this->participantInfo.stealthAddress)){
            printf("ProcessAcceptResponse - stealth address invalid!\n");
            return false;
        }
        /* else if (this->participantInfo.stealthAddress == request->senderAddress) {
            printf("ProcessAcceptResponse - receiver address can't help, rejected.\n");
            return false;
        } */

        this->participantInfo.stealthAddressObj.SetEncoded(this->participantInfo.stealthAddress);

        // generate shared secret for communicating with cloak requester (ECDH)
        CCloakingSecret secret;
        if (!myKey->GetSecret(this->participantInfo.pubKey, secret, true)){
            return false;
        }

        // add recipient?
        if ((int)request->sParticipants.size() < request->nParticipantsRequired)
        {
            // check the inputs supplied by the cloaker are valid (matured and unspent)
            if (!this->participantInfo.inOuts.CheckValid(request->nSendAmount, true, this->participantInfo.pubKey))
            {
                printf("ProcessAcceptResponse - inOuts invalid!\n");
                return false; // bail
            }

            request->sParticipants.insert(this->participantInfo);
            Enigma::SendEnigmaMessage(myKey, ENIGMA_MSG_CLOAKER_ACCEPTED, request->GetIdHash().GetHex(), secret);

            // if we have enough cloakers, create the shared transaction and request cloakers sign it
            // once all cloakers have signed and responded, we sign ourself and submit the transaction
            if ((int)request->sParticipants.size() >= request->nParticipantsRequired && !request->txCreated)
            {
                request->txCreated = true;
                request->CreateTransactionAndRequestSign(myKey);
            }

        }else{
            // respond to node to say request is full, so that they may attempt to join another request or choose to create their own
            Enigma::SendEnigmaMessage(myKey, ENIGMA_MSG_CLOAKER_REJECTED, request->GetIdHash().GetHex(), secret);
            return false;
        }
        return true;
    }catch (std::exception& e){
        error("ENIGMA ERROR CCloakingAcceptResponse::ProcessAcceptResponse failed: %s.\n", e.what());
    }
    return false;
}

void CCloakingDoneInfo::SetNull()
{
    nVersion = ENIGMA_REQUEST_CURRENTVERSION;
    requestId.SetNull();
    hashTx = 0;
}


// handle a cloaking accept message
bool Enigma::HandleCloakingAccept(CCloakingData cloakData)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        bool handled = false;

        if (!cloakData.AlreadyProcessed() && !cs->IsEnigmaPeerBanned(cloakData.senderPubKeyHex))
        {
            // check sig
            if (!cloakData.Authenticate()){
                cs->BanPeer(cloakData.senderPubKeyHex, ENIGMA_BANSECS_JUNK_DATA);
                return false;
            }

            // decrypt and de-serialize the response
            if (cloakData.Decrypt(cs->GetRoutingKey()))
            {
                CCloakingAcceptResponse ccar;
                if (cloakData.Decode(&ccar))
                {
                    // If this is in response to one of *our* requests, process it
                    uint256 acceptHash = ccar.GetHash();                    
                    {
                        LOCK2(pwalletMain->cs_mapCloakingRequests, pwalletMain->cs_CloakingAccepts);
                        if (pwalletMain->sCloakingAccepts.find(acceptHash) == pwalletMain->sCloakingAccepts.end())
                        {
                            if(pwalletMain->mapOurCloakingRequests.find(ccar.requestIdentifier) != pwalletMain->mapOurCloakingRequests.end())
                            {
                                // wfd if (ccar.requestIdentifier != cloakData.identifier)
                                {
                                    // it's one of ours, add the participant (from response) to the cloaking request and relay updated request
                                    ccar.ProcessAcceptResponse(cs->GetRoutingKey());
                                    handled = true;
                                }
                                /* else
                                {
                                    printf("ENIGMA: HandleCloakingAccept - don't allow receiver to assist.");
                                } */
                            }else{
                                // couldn't find our associated request
                                printf("ENIGMA: HandleCloakingAccept - request not found\n");
                            }
                            pwalletMain->sCloakingAccepts.insert(make_pair(acceptHash, ccar));
                        }
                    }
                }
            }
        }
        return handled;
    }catch (std::exception& e){
        error("Enigma::HandleCloakingAccept: %s.", e.what());
    }
    return false;
}

// handle an onion routed tx from a peer. We decode and relay the contained tx.
bool Enigma::HandleOnionTx(CCloakingData cloakData)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        bool handled = cloakData.AlreadyProcessed() && !cs->IsEnigmaPeerBanned(cloakData.senderPubKeyHex);
        if (!handled)
        {
            // decrypt and de-serialize the response
            if (cloakData.Decrypt(cs->GetRoutingKey()))
            {
                CTransaction tx;
                cloakData.Decode(&tx);
                {
                    uint256 hash = tx.GetHash();
                    RelayMessage(CInv(MSG_TX, hash), tx);
                }
            }
        }
        return handled;
    }catch (std::exception& e){
        error("ENIGMA::HandleOnionTx: %s.", e.what());
    }
    return false;
}

// send an untargetted message (much like a network-wide alert)
bool Enigma::SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, bool doNotOnionRoute)
{
    return SendEnigmaMessage(myKey, code, data, vector<CCloakingSecret>(), doNotOnionRoute);
}

bool Enigma::SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, string recipientPubKey, bool doNotOnionRoute)
{
    CCloakingSecret recipient;
    if (CCloakShield::GetShield()->GetRoutingKey()->GetSecret(recipientPubKey, recipient, true)){
        return SendEnigmaMessage(myKey, code, data, recipient, doNotOnionRoute);
    }
    return false;
}

bool Enigma::SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, CCloakingSecret recipient, bool doNotOnionRoute)
{
    vector<CCloakingSecret> recipients;
    recipients.push_back(recipient);
    return SendEnigmaMessage(myKey, code, data, recipients, doNotOnionRoute);
}

bool Enigma::SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, vector<CCloakingSecret> recipients, bool doNotOnionRoute)
{
    try
    {
        // wrap accept-respose in encrypted cloak-data object and broadcast
        CCloakingData cloakData;

        CCloakingMessage msg;
        msg.messageCode = code;
        msg.messageData = data;

        // encode accept response
        return cloakData.EncodeEncryptTransmit((void*)&msg, CLOAK_DATA_ENIGMA_MSG, recipients, myKey, ENCRYPT_PAYLOAD_YES && recipients.size() > 0, TRANSMIT_YES, !doNotOnionRoute && ONION_ROUTING_ENABLED);
    }catch (std::exception& e){
        error("Enigma::SendEnigmaMessage: %s.", e.what());
    }
    return false;
}

// handle a response from a cloaker that contains a version of the Enigma Tx that they have signed.
bool Enigma::HandleSignResponse(CCloakingData cloakData)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        bool handled = cloakData.AlreadyProcessed() && !cs->IsEnigmaPeerBanned(cloakData.senderPubKeyHex);
        if (!handled)
        {
            // check sig
            if (!cloakData.Authenticate()){
                cs->BanPeer(cloakData.senderPubKeyHex, ENIGMA_BANSECS_JUNK_DATA);
                return false;
            }

            // decrypt and de-serialize the response
            if (cloakData.Decrypt(cs->GetRoutingKey()))
            {
                CCloakingSignResponse ccr;
                cloakData.Decode(&ccr);
                {
                    {
                        LOCK(pwalletMain->cs_sSignResponses);
                        // check we haven't already accepted the request
                        BOOST_FOREACH(CCloakingSignResponse sr, pwalletMain->sSignResponses)
                        {
                            if(sr.identifier == ccr.identifier && sr.signingAddress == ccr.signingAddress)
                            {
                                handled = true;
                                break;
                            }
                        }
                    }

                    if (!handled)
                    {
                        handled = true;
                        // if this is a response to us, process it
                        if(ccr.ProcessSignatureResponse(cs->GetRoutingKey()))
                        {
                            if(GetBoolArg("-printenigma"))
                                printf("ENIGMA: posasignresp processed signature response.\n");
                        }

                        // mark as handled
                        if (!pwalletMain->sSignResponses.insert(ccr).second)
                        {
                            printf("ENIGMA: posasignresp failed to insert signresponse.\n");
                        }
                    }
                }
            }
        }
        return handled;
    }catch (std::exception& e){
        error("ENIGMA::HandleSignResponse: %s.", e.what());
    }
    return false;
}

// handle a cloaking accept message
bool Enigma::HandleSignRequest(CCloakingData cloakData)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        bool handled = cloakData.AlreadyProcessed() && !cs->IsEnigmaPeerBanned(cloakData.senderPubKeyHex);
        if (!handled)
        {
            // check sig
            if (!cloakData.Authenticate()){
                cs->BanPeer(cloakData.senderPubKeyHex, ENIGMA_BANSECS_JUNK_DATA);
                return false;
            }

            // decrypt and de-serialize the response
            if (cloakData.Decrypt(cs->GetRoutingKey()))
            {
                CCloakingRequest csr;
                cloakData.Decode(&csr);

                bool bFound = false;
                uint256 hashReq = csr.identifier.GetHash();
                {
                    LOCK(pwalletMain->cs_sSignHashes);
                    BOOST_FOREACH(uint256 hashCacheReq, pwalletMain->sSignRequestHashes) // check we haven't already accepted the request
                    {
                        if(hashCacheReq == hashReq)
                        {
                            bFound = true;
                            break;
                        }
                    }

                    // Relay to nodes
                    if (!bFound)
                    {
                        // if we are a participant, sign the request and send it back
                        if(!csr.aborted && csr.SignAndRespond(cs->GetRoutingKey()))
                        {
                            if(GetBoolArg("-printenigma"))
                                printf("ENIGMA: posasignreq processed and responded.\n");
                        }

                        {
                            LOCK(pwalletMain->cs_sSignHashes);
                            if (!pwalletMain->sSignRequestHashes.insert(hashReq).second)
                                printf("ENIGMA: posasignreq failed to insert hash.\n");
                        }
                    }
                }
            }
        }
        return handled;
    }catch (std::exception& e){
        error("Enigma::HandleSignRequest: %s.", e.what());
    }
    return false;
}

void Enigma::HandleNodeOffline(string nodePubkeyHash)
{
    TRY_LOCK(cs_mapEnigmaAnns, lockAnns);
    if (lockAnns)
    {
        uint160 keyHash = uint160(nodePubkeyHash);
        map<uint160, CEnigmaAnnouncement>::iterator iter = mapEnigmaAnns.find(keyHash);
        if( iter != mapEnigmaAnns.end() ){
            mapEnigmaAnns.erase( iter );
        }
    }
}

bool Enigma::HandleAcceptResponseFromSender(EnigmaMessageCode msgType, string requestIdHashHex)
{
    try
    {
        uint256 requestHash = uint256(requestIdHashHex);

        switch (msgType)
        {
            case (ENIGMA_MSG_CLOAKER_ACCEPTED):
                // our offer of cloaking assistance has been accepted
                nCloakerCountAccepted++;
                break;

            case (ENIGMA_MSG_CLOAKER_REJECTED):
                // our offer of cloaking assistance has been rejected, cancel the request
                {
                    LOCK(pwalletMain->cs_mapCloakingRequests);
                    if (pwalletMain->mapCloakingRequests.find(requestHash) != pwalletMain->mapCloakingRequests.end())
                    {
                        CCloakingRequest* req = &pwalletMain->mapCloakingRequests[requestHash];
                        req->Abort();
                    }
                    else
                    {
                        printf("CCloakingRequest::SignAndRespond: cached request not found.\n");
                        return false;
                    }
                }
                break;

            default:
                break;
        }

    }catch (std::exception& e){
        error("Enigma::HandleAcceptResponseFromSender: %s.", e.what());
    }
    return false;
}

void Enigma::LogCloakMessage(CCloakingMessage msg)
{
    char codeStr[255];
    switch(msg.messageCode)
    {
        case ENIGMA_MSG_CLOAKER_REJECTED:
            sprintf(codeStr, "ENIGMA_MSG_CLOAKER_REJECTED");
            break;
        case ENIGMA_MSG_SIGN_INPUTS_OUTPUTS_MISSING:
            sprintf(codeStr, "ENIGMA_MSG_SIGN_INPUTS_OUTPUTS_MISSING");
            break;
        case ENIGMA_MSG_SIGN_INPUT_NOT_FOUND:
            sprintf(codeStr, "ENIGMA_MSG_SIGN_INPUT_NOT_FOUND");
            break;
        case ENIGMA_MSG_SIGN_CHANGE_MISSING:
            sprintf(codeStr, "ENIGMA_MSG_SIGN_CHANGE_MISSING");
            break;
        case ENIGMA_MSG_SIGN_REQUEST_NOT_FOUND:
            sprintf(codeStr, "ENIGMA_MSG_SIGN_REQUEST_NOT_FOUND");
            break;
        case ENIGMA_MSG_CLOAKER_ACCEPTED:
            sprintf(codeStr, "ENIGMA_MSG_CLOAKER_ACCEPTED");
            break;
        case ENIGMA_MSG_OK:
            sprintf(codeStr, "ENIGMA_MSG_OK");
            break;
        case ENIGMA_MSG_NODE_GONE_OFFLINE:
            sprintf(codeStr, "ENIGMA_MSG_NODE_GONE_OFFLINE");
            break;
        default:
            sprintf(codeStr, "ENIGMA_MSG_???");
            break;
    }

    switch(msg.messageCode)
    {
        case ENIGMA_MSG_CLOAKER_REJECTED:
        case ENIGMA_MSG_SIGN_INPUTS_OUTPUTS_MISSING:
        case ENIGMA_MSG_SIGN_INPUT_NOT_FOUND:
        case ENIGMA_MSG_SIGN_CHANGE_MISSING:
        case ENIGMA_MSG_SIGN_REQUEST_NOT_FOUND:
        case ENIGMA_MSG_NODE_GONE_OFFLINE:
        case ENIGMA_MSG_CLOAKER_ACCEPTED:
            printf("%s : %s\n", codeStr, msg.messageData.c_str());
            break;
    }
}

bool Enigma::HandleCloakMessage(CCloakingData cloakData)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        bool handled = cloakData.AlreadyProcessed() && !cs->IsEnigmaPeerBanned(cloakData.senderPubKeyHex);
        if (!handled)
        {
            // note: no need to check signature as this was done by the onion data handler
            // decrypt and de-serialize the response
            if (cloakData.Decrypt(cs->GetRoutingKey()))
            {
                CCloakingMessage msg;
                cloakData.Decode(&msg);

                switch(msg.messageCode)
                {
                    case ENIGMA_MSG_NODE_GONE_OFFLINE:
                        Enigma::HandleNodeOffline(msg.messageData);
                    case ENIGMA_MSG_CLOAKER_REJECTED:
                    case ENIGMA_MSG_CLOAKER_ACCEPTED:
                        Enigma::HandleAcceptResponseFromSender((EnigmaMessageCode)msg.messageCode, msg.messageData);
                        break;
                    case ENIGMA_MSG_SIGN_INPUTS_OUTPUTS_MISSING:
                    case ENIGMA_MSG_SIGN_INPUT_NOT_FOUND:
                    case ENIGMA_MSG_SIGN_CHANGE_MISSING:
                    case ENIGMA_MSG_SIGN_REQUEST_NOT_FOUND:
                    case ENIGMA_MSG_OK:
                    default:
                        break;
                }

                if(GetBoolArg("-printenigma", false)){
                    // log if not OK or ACCEPTED
                    Enigma::LogCloakMessage(msg);
                }

            }
        }
        return handled;
    }catch (std::exception& e){
        error("ENIGMA::HandleCloakMessage: %s.", e.what());
    }
    return false;
}

// handle a Enigma message wrapped in a CCloakingData object.
bool Enigma::HandleCloakOnionData(CDataStream& vRecv, CNode* node)
{
    try
    {
        CCloakingData cloakData;
        vRecv >> cloakData;

        return HandleCloakOnionData(cloakData, node, 0);
    }catch (std::exception& e){
        error("Enigma::HandleEnigmaMessage: %s.", e.what());
    }
    return false;
}


// handle a cloak-data onion
// we peel back the layers and handle the content per layer, relaying the data if needed
bool Enigma::HandleCloakOnionData(CCloakingData cloakDataIn, CNode* node, int level)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();

        // too many layers? this is probably a DoS attempt from a remote peer
        if (level >= MAX_ONION_SKINS)
            return false;

        // set data type name for debugging
#ifdef QT_DEBUG
        cloakDataIn.SetDataTypeName();
#endif

        uint256 hash = cloakDataIn.GetSigningHash();
        bool alreadyProcessed = cloakDataIn.AlreadyProcessed();

        if (!alreadyProcessed)
        {                        
            cloakDataIn.hops++;

            if (level > 0){
                cloakDataIn.Transmit(NULL, true, false); // forward to all as we've unpacked a layer
            }else{
                cloakDataIn.Transmit(node, true, false); // forward to all but sender, sending as is
            }

            // decrypt and de-serialize the next encrypted onion packet and forward it to peers
            if (cloakDataIn.Decrypt(cs->GetRoutingKey()))
            {
                switch(cloakDataIn.payload.dataType)
                {
                    case(CLOAK_DATA_ENIGMAREQ): // unencrypted & encoded request
                    {
                        CCloakingRequest req;
                        cloakDataIn.Decode((void*)&req);
                        // mark the request as sent and process/relay it
                        bool relayRequest = req.ProcessRequest(cs->GetRoutingKey());
                        if (relayRequest){
                            req.bBroadcasted = true;
                            req.nBroadcastedTime = GetAdjustedTime();                            
                        }
                        break;
                    }
                    case(CLOAK_DATA_REQ_ACCEPT):
                    {
                        HandleCloakingAccept(cloakDataIn);
                        break;
                    }
                    case(CLOAK_DATA_SIGN_REQUEST):
                    {
                        HandleSignRequest(cloakDataIn);
                        break;
                    }
                    case(CLOAK_DATA_SIGN_RESPONSE):
                    {
                        HandleSignResponse(cloakDataIn);
                        break;
                    }
                    case(CLOAK_DATA_TX):
                    {
                        HandleOnionTx(cloakDataIn);
                        break;
                    }
                    case(CLOAK_DATA_ENIGMA_MSG):
                    {
                        // a cloak message to us from an enigma peer
                        HandleCloakMessage(cloakDataIn);
                        break;
                    }
                    case(CLOAK_DATA_RELAY_DATA):
                    {
                        CCloakingData cloakChildData;
                        // check encrypted, otherwise handle payload *****
                        // payload is cloakshield onion layer, decode and relay
                        // onion routed requests and transactions are not encrypted. we just decode those...
                        if (cloakDataIn.Decode(&cloakChildData))
                        {
                            // process the child data
                            HandleCloakOnionData(cloakChildData, node, level+1);
                        }
                        break;
                    }
                    default:
                    {
                        printf("ENIGMA: Unknown CLOAKSHIELD sub data type %d. Corrupt message?!?!\n", cloakDataIn.payload.dataType);
                        break;
                    }
                }
            }

            // add to processed list so we ignore any pingbacks, and relay randomly to peers
            cloakDataIn.MarkProcessed(hash, true);
        }
        return alreadyProcessed;
    }catch (std::exception& e){
        error("Enigma::HandleCloakOnionData: %s.", e.what());
    }
    return false;
}

CCloakingRequest* Enigma::SendEnigma(CCloakingInputsOutputs inputsOutputs, CCloakingRequest* cr)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        // store the request
        {
            LOCK(pwalletMain->cs_mapOurCloakingRequests);
            pwalletMain->mapOurCloakingRequests.insert(make_pair(cr->GetIdHash(), *cr));
        }

        {
            LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
            // add the CCloakingInputsOutputs object to our local mem queue
            // note: we use request id hash as map index
            pwalletMain->mapCloakingInputsOutputs.insert(make_pair(cr->GetIdHash(), inputsOutputs));
        }

        // mark the request as updated to trigger display update in GUI
        cr->WasUpdated();

        // relay request message to peers
        if (cs->SendEnigmaRequest(cr))
        {
            return cr;
        }
    }catch (std::exception& e){
        error("ENIGMA::SendEnigma: %s.", e.what());
    }
    return NULL;
}

// Send funds using joint/shuffled single cloak transaction.
// Will return a new request object
CCloakingRequest* Enigma::SendEnigma(CCloakingInputsOutputs inputsOutputs, int numParticipants, int numSplits, int timeoutSecs, const CCloakingEncryptionKey* myKey)
{
    try
    {
        CCloakShield* cs = CCloakShield::GetShield();
        if(GetBoolArg("-printenigma", false))
        {
            OutputDebugStringF("Sending Enigma**\n");
            OutputDebugStringF("NumHops:%d\n", cs->NumHopsRequired());
            OutputDebugStringF("NumNodes:%d\n", cs->NumNodesRequired());
            OutputDebugStringF("NumRoutes:%d\n", cs->NumRoutesRequired());
            OutputDebugStringF("AutoRetry:%d\n", fEnigmaAutoRetry);
            OutputDebugStringF("Timout:%d mins\n", timeoutSecs / 60);
            OutputDebugStringF("EnableOnionRouting:%d\n", fEnableOnionRouting);
            OutputDebugStringF("Staking:%d\n\n", fStaking);
        }

        // no request for us to join, create our own and let others join it
        CCloakingRequest* cr = CCloakingRequest::CreateForBroadcast(numParticipants-1, numSplits, timeoutSecs, myKey, inputsOutputs.nSendAmount);
        inputsOutputs.requestIdentifier = cr->identifier.GetHash();

        // store the request
        {
            LOCK(pwalletMain->cs_mapOurCloakingRequests);
            pwalletMain->mapOurCloakingRequests.insert(make_pair(cr->GetIdHash(), *cr));
        }

        {
            LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
            // add the CCloakingInputsOutputs object to our local mem queue
            // note: we use request id hash as map index
            pwalletMain->mapCloakingInputsOutputs.insert(make_pair(cr->GetIdHash(), inputsOutputs));
        }

        // mark the request as updated to trigger display update in GUI
        cr->WasUpdated();

        // relay request message to peers
        if (cs->SendEnigmaRequest(cr))
        {
            return cr;
        }
    }catch (std::exception& e){
        error("ENIGMA::SendEnigma: %s.", e.what());
    }
    return NULL;
}

bool Enigma::CloakShieldShutdown()
{
    CCloakingEncryptionKey* key = CCloakShield::GetShield()->GetShield()->GetRoutingKey();
    return SendEnigmaMessage(key, ENIGMA_MSG_NODE_GONE_OFFLINE, key->pubkeyHexHash, true);
}

void StartProcessingEnigma()
{
    // start processing the poa queue
    if (!NewThread(ThreadEnigma, NULL))
        printf("Error: NewThread(ThreadEnigma) failed\n");
}


