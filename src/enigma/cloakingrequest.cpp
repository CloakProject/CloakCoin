#include "txdb.h"
#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
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

#include "enigma.h"
#include "cloakshield.h"
#include "cloakingrequest.h"

#include "cloakexception.h"

/*

  IMPORTANT:
  cloakshield improvement: currently the same shared secret is used continually. this should be avoided and
  some mechanism should be introduced to repeatedly change the encryption key more than once per session...

*/

extern string nMyEnigmaAddress;
extern int nCloakerCountRefused;
extern int nCloakerCountSigned;
extern int64 nEnigmaProcessed;
extern map<string, int> peerDosCounts; // key = public key

// process a request to help cloak from a Enigma peer.
// returns TRUE if we agree to assist with cloaking, FALSE otherewise.
bool CCloakingRequest::ProcessRequest(CCloakingEncryptionKey* myKey)
{
    try
    {
        if (nVersion != ENIGMA_REQUEST_CURRENTVERSION) // check version matches (more important during testing)
            return false;

        CCloakShield* cs = CCloakShield::GetShield();
        uint256 idhash = GetIdHash();

        if (pwalletMain->sRequestHashes.count(idhash)>0){
            return false; // already handled
        }

        if ((ONION_ROUTING_ENABLED && fEnableOnionRouting) && !cs->OnionRoutingAvailable(false))
            return true; // we can't process it (not enough onion nodes), but others may

        bool willProcess = !fSyncing;

        // make sure it isn't already ours, and isn't already contained
        {
            LOCK2(pwalletMain->cs_mapCloakingRequests, pwalletMain->cs_mapOurCloakingRequests);
            willProcess = (pwalletMain->mapOurCloakingRequests.find(idhash) == pwalletMain->mapOurCloakingRequests.end() &&
                                    pwalletMain->mapCloakingRequests.find(idhash) == pwalletMain->mapCloakingRequests.end());
        }

        willProcess &= !pwalletMain->IsLocked() || fWalletUnlockMintOnly;

        if(willProcess)
        {
            if(GetBoolArg("-printenigma"))
                printf("ENIGMA: processing enigma request %s.\n", idhash.ToString().c_str());

            bool hasSpace = (int)this->sParticipants.size() < this->nParticipantsRequired;
            bool notExpired = !this->IsExpired();
            bool meetsSecurity = this->nParticipantsRequired >= GetBoolArg("-mincloakers", MIN_CLOAKERS); // does this request require more than our min cloaker count for participation?

            // check there's space for another participant and that request is still valid (less than [timeoutSecs] old)
            if (hasSpace && notExpired && meetsSecurity)
            {
                // accept the request?
                // must have at least [balance * splits] cloak available
                // wallet must be unlocked
                if (pwalletMain->GetBalance() > this->numSplitsRequired * this->nSendAmount)
                {
                    // generate shared secret for communicating with cloak requester (via ECDH)
                    // this is done early to allow us to bail out if it fails
                    CCloakingSecret secret;
                    myKey->GetSecret(this->identifier.pubKeyHex, secret, true);

                    // reserve the funds needed to participate

                    // ** generate change addresses and split the change/return randomly between them
                    // ** also allocate the fee to one of the addresses. we need to code in support for handling
                    // ** these transactions within the wallet GUI and ideally list the fee/reward separately.
                    // ** true output from requester should have the VOUTs split randonly to further muddy the waters.

                    CCloakingAcceptResponse car;
                    car.requestIdentifier = this->GetIdHash(); // set response id to request hash
                    car.participantInfo.pubKey = myKey->pubkeyHex; // we are participating, set our address in response (enigma??)

                    if (!pwalletMain->GetEnigmaAddress(car.participantInfo.stealthAddressObj)) // use one of our active enigma cloaking stealth addresses
                    {
                        printf("CCloakingRequest::ProcessRequest - Failed to get enigma address address.\n");
                        return false;
                    }
                    car.participantInfo.stealthAddress = car.participantInfo.stealthAddressObj.Encoded();

                    bool gotInOuts = this->GetCloakingInputsMine(car.participantInfo); // add our inputs and change address ouputs

                    if (!gotInOuts){
                        if(GetBoolArg("-printenigma", false)){
                            printf("CCloakingRequest::ProcessRequest - GetCloakingInputsOutputsMine failed - need to stake??\n");
                        }
                        // we couldn't participate due to funding issues, but lets others by relaying
                        return true;
                    }                   

                    if(GetBoolArg("-printenigma")){
                        printf("ENIGMA: accepting request.\n");
                    }

                    {
                        LOCK2(pwalletMain->cs_CloakingAccepts, pwalletMain->cs_wallet);
                        // since we've accepted this request, we should save it for use when constructing transaction view lists
                        uint256 acceptHash = car.GetHash();
                        pwalletMain->sCloakingAccepts.insert(make_pair(acceptHash, car));
                    }

                    // wrap accept-response in an encrypted cloak-data object and broadcast
                    CCloakingData cloakData;
                    vector<CCloakingSecret> recipientSecrets;
                    recipientSecrets.push_back(secret);

                    if (cloakData.EncodeEncryptTransmit((void*)&car, CLOAK_DATA_REQ_ACCEPT, recipientSecrets, myKey, ENCRYPT_PAYLOAD_YES, TRANSMIT_YES, ONION_ROUTING_ENABLED)){
                        // store ref and inc counter
                        pwalletMain->mapCloakingRequests.insert(make_pair(idhash, *this));
                    }
                }
            }
        }

        pwalletMain->sRequestHashes.insert(idhash);

        if(GetBoolArg("-printenigma"))
            printf("ENIGMA: enigmarequest relaying to nodes.\n");

        return willProcess;
    }catch (std::exception& e){
        error("CCloakingRequest::ProcessRequest: %s.", e.what());
    }
    return false;
}

// called when a Enigma request has changed. This signals to update balance displays etc.
bool CCloakingRequest::WasUpdated()
{
    try
    {
        // mark the wallet as dirty to update the balance
        pwalletMain->MarkDirty();
        pwalletMain->EnigmaRequestUpdated(this);
    }catch (std::exception& e){
        error("CCloakingRequest::WasUpdated: %s.", e.what());
        return false;
    }
    return true;
}

// ensure the correct network fee is added to the joint tx by subtracting the fee from our change return
bool CCloakingRequest::AddCorrectFee(CCloakingInputsOutputs* inOutsParticipants, CCloakingInputsOutputs* inOutsOurs)
{
    try
    {
        int64 intFee = 0;
        CTransaction tx;
        inOutsParticipants->addVinVoutsToTx(&tx);
        inOutsOurs->addVinVoutsToTx(&tx);

        uint nBytes = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
        nBytes+=tx.vin.size() * 120; // bump up to account for script signatures on the signed inputs
        intFee = tx.GetMinFee(1, false, GMF_SEND, nBytes);

        CTxOut* txoutMatch = 0;
        int64 amt = 0;
        bool success = false;

        for(int i=0; i<(int)inOutsOurs->vout.size(); i++){
            CTxOut* txout = &(inOutsOurs->vout[i]);
            if (txout->nValue - MIN_TXOUT_AMOUNT > intFee && txout->nValue  > amt){
                // this is one of our change addresses, subtract fee to reserve fee
                success = true;
                txoutMatch = txout;
                amt = txout->nValue;
                break;
            }
        }

        if (success){
            txoutMatch->nValue -= intFee;
        }else{
            printf("CCloakingRequest::AddCorrectFee failed to find target output.");
        }
        return success;
    }catch (std::exception& e){
        error("CCloakingRequest::AddCorrectFee: %s.", e.what());
    }
    return false;
}

bool CCloakingRequest::ShouldRetry()
{
    return this->retryCount < ENIGMA_MAX_RETRIES;
}

bool CCloakingRequest::IsExpired()
{
    int64 timeTaken = (int)(GetAdjustedTime() - this->nBroadcastedTime);
    int timeLimit = this->timeoutSecs;
    bool expired = timeTaken > timeLimit;
    return expired;
}

int CCloakingRequest::TimeLeftMs()
{
    return this->identifier.nInitiatedTime + (this->timeoutSecs) - GetTime();
}

// handle the outputs for cloakers and assign the outputs to their corresponding collection of inputs/outputs
bool CCloakingRequest::CreateCloakerTxOutputs(vector<CTxOut>& outputs)
{
    // todo: this creates a zero / empty script output at end of outputs - fix!
    int64 totalReward = this->nSendAmount * ENIGMA_TOTAL_FEE_PERCENT * 0.01;
    int64 reward = totalReward / this->nParticipantsRequired;

    BOOST_FOREACH(const CCloakingParticipant& cloaker, sParticipants){
        int64 change = cloaker.inOuts.nInputAmount + reward;
        // create 2-3 matching outputs that equal send amount before splitting up the rest of the change
        std::vector<int64> amounts = SplitAmount(this->nSendAmount, MIN_TXOUT_AMOUNT, GetRandRange(2, 3));
        change-=this->nSendAmount;

        if (change > MIN_TXOUT_AMOUNT){
            // now split up the remaining change between 1-3 outputs. any reward/change will be allocated to these
            // outputs before the tx inputs and outputs are shuffled prior to tx creation (for signing)
            std::vector<int64> amounts2 = SplitAmount(change, MIN_TXOUT_AMOUNT, GetRandRange(1, 3));
            amounts.insert(amounts.end(), amounts2.begin(), amounts2.end());
        }else{
            int idx = GetRandInt(amounts.size()-1);
            amounts.at(idx)+=change;
        }

        vector<ec_secret> secrets;
        vector<CScript> scriptChange;        
        if (!pwalletMain->GetEnigmaChangeAddresses(cloaker.stealthAddressObj, this->stealthRootKey, cloaker.inOuts.stealthHash, amounts.size(), scriptChange, secrets)){
            printf("CCloakingRequest:HandleCloakerOutputs GetEnigmaChangeAddresses failed\n");
            return false;
        }

        int64 total = 0;

        // create the sent outputs for our inouts
        for(uint64 i=0; i<(uint64)amounts.size(); i++){
            outputs.push_back(CTxOut(amounts.at(i), scriptChange.at(i)));
            CTxDestination onetimeAddress;
            if (!ExtractDestination(scriptChange.at(i), onetimeAddress)){
                printf("CCloakingRequest:HandleCloakerOutputs ExtractDestination failed\n");
                return false;
            }
            if (onetimeAddress.type() != typeid(CKeyID)){
                printf("CCloakingRequest:CreateSenderOutputs typeid(CKeyID) failed\n");
                return false;
            }
            CBitcoinAddress address(onetimeAddress);
            string s = address.ToString();

            total+=amounts.at(i);
            if(GetBoolArg("-printenigma", false))
                printf("OUTPUT %s = %ld\n", s.c_str(), amounts.at(i));
        }
        if(GetBoolArg("-printenigma", false)){
            printf("TOTAL INPUT  %d\n", cloaker.inOuts.nInputAmount);
            printf("TOTAL OUTPUT %d\n", total);
        }
    }
    return true;
}

bool CCloakingRequest::CreateEnigmaOutputs(CCloakingInputsOutputs &inOutsCloakers, CCloakingInputsOutputs &inOutsOurs, CCloakingInputsOutputs &inOutsFinal)
{
    if(GetBoolArg("-printenigma", false))
        printf("* create cloaker outputs *\n");

    // add cloaker outputs to cloaker inouts (change and reward)
    if (!CreateCloakerTxOutputs(inOutsCloakers.vout)){
        return false;
    }

    int64 amountIn = inOutsOurs.nInputAmount;
    BOOST_FOREACH(const CCloakingParticipant& cloaker, sParticipants){
        amountIn+=cloaker.inOuts.nInputAmount;
    }

    uint64 amountOut = inOutsOurs.OutputAmount() + inOutsCloakers.OutputAmount();

    if(GetBoolArg("-printenigma", false)){
        printf("SENDER INPUT  %ld\n", inOutsOurs.nInputAmount);
        printf("SENDER OUTPUT %ld\n", inOutsOurs.OutputAmount());
    }

    // calc our change due (we still need to remove the tx fee from this)
    int64 amountRemain = amountIn - amountOut;
    // account for op return payment
    amountRemain-=MIN_TXOUT_AMOUNT;

    std::vector<int64> amounts = SplitAmount(amountRemain, MIN_TXOUT_AMOUNT, GetRandRange(2, 3));

    printf("* create sender outputs *\n");

    vector<CScript> scriptChange;
    vector<ec_secret> secrets;
    if (!pwalletMain->GetEnigmaChangeAddresses(this->senderAddress, this->stealthRootKey, this->senderAddress.GetEnigmaAddressPrivateHash(), amounts.size(), scriptChange, secrets)){
        printf("CCloakingRequest:CreateSenderOutputs failed");
        return false;
    }

    // create the sent outputs for our inouts
    for(uint64 i=0; i<(uint64)scriptChange.size(); i++){
        amountOut+=amounts.at(i);
        inOutsOurs.vout.push_back(CTxOut(amounts.at(i), scriptChange.at(i)));
    }

    if(GetBoolArg("-printenigma", false)){
        printf("TX TOTAL INPUT  %ld\n", amountIn);
        printf("TX TOTAL OUTPUT %ld\n", amountOut);
    }

    // remove network fee from our change
    if (!AddCorrectFee(&inOutsCloakers, &inOutsOurs)){
        return false;
    }

    inOutsFinal.addVinVouts(inOutsOurs);
    inOutsFinal.addVinVouts(inOutsCloakers);

    return true;
}

// populate the transaction object for the request before the participants sign it.
bool CCloakingRequest::CreateTransactionAndRequestSign(CCloakingEncryptionKey* myKey)
{
    try
    {
        if((int)sParticipants.size() >= nParticipantsRequired)
        {
            // enough participants - create the enigma transaction and send to cloakers to check and sign
            // add our inputs
            CCloakingInputsOutputs inOutsMine;
            LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
            map<uint256, CCloakingInputsOutputs>::iterator myInputsOutputs = pwalletMain->mapCloakingInputsOutputs.find(GetIdHash());
            if(myInputsOutputs != pwalletMain->mapCloakingInputsOutputs.end()){
                inOutsMine = myInputsOutputs->second;
            }else{
                printf("WARNING: Enigma CreateTransactionAndRequestSign: Failed to find own inputs/outputs.");
                return false;
            }

            CCloakingInputsOutputs inOutsCloakers;

            // collate cloaker inputs
            BOOST_FOREACH(const CCloakingParticipant& cloaker, sParticipants){
                inOutsCloakers.addVinVouts(cloaker.inOuts, true, false);
            }

            // add our inouts to the collated collection
            CCloakingInputsOutputs inOuts;
            if (!CreateEnigmaOutputs(inOutsCloakers, inOutsMine, inOuts)){
                return false;
            }

            // update the original copy of our inouts so that it reflects the redistributed fee
            pwalletMain->mapCloakingInputsOutputs[GetIdHash()] = inOutsMine;

            // shuffle inputs and outputs
            random_shuffle(inOuts.vin.begin(), inOuts.vin.end());
            random_shuffle(inOuts.vout.begin(), inOuts.vout.end());

            // create zero output with op return and nonce for stealth change collection
            CScript scriptP = CScript() << OP_RETURN << stealthRootKey;
            inOuts.vout.push_back(CTxOut(MIN_TXOUT_AMOUNT, scriptP));

            // create transaction
            CTransaction jointTrans;

            // add in/outs
            inOuts.addVinVoutsToTx(&jointTrans);

            // add transaction to request

            CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
            ssTx << jointTrans;
            this->rawtx = this->signedrawtx = HexStr(ssTx.begin(), ssTx.end());

            CTxDB txdb("r");
            MapPrevTx mapInputs;
            map<uint256, CTxIndex> mapUnused;
            bool fInvalid = false;
            if (!jointTrans.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid)){
                return false;
            }
            if (fInvalid){
                return false;
            }

            int64 valIn = jointTrans.GetValueIn(mapInputs);
            int64 valOut = jointTrans.GetValueOut();
            int64 nFees = valIn-valOut;

            if (nFees < MIN_TX_FEE){
                printf("WARNING: Enigma CreateTransactionAndRequestSign: TxFee too small. in %d, out %d, fee %d\n", valIn, valOut, nFees);
                return false;
            }

            // check that all inputs are still valid before requesting signing.
            // if they are not, we abort and send a message to cloakers so they
            // can release their reserved/held funds locally for cloaking another tx.
            this->WasUpdated();

            // request signing from participants
            this->EncryptSendSignRequest(myKey);

        }
        return true;
    }catch (std::exception& e){
        error("CCloakingRequest::CreateTransactionAndRequestSign: %s.", e.what());
    }
    return false;
}

// add our inputs and outputs to a request that we are participating in
// this is done after the request is received
bool CCloakingRequest::GetCloakingInputsMine(CCloakingParticipant& part)
{
    try
    {
        part.inOuts.requestSigned = false;
        part.inOuts.vin.clear();
        part.inOuts.vout.clear();        

        LOCK2(pwalletMain->cs_CloakingRequest, pwalletMain->cs_mapCloakingInputsOutputs);
        {
            // Choose coins to use
            set<pair<const CWalletTx*,uint> > setCoins;
            int64 nChange = 0;

            vector<COutput> vCoins;
            pwalletMain->AvailableCoins(vCoins, true, NULL, ENIGMA_MAX_COINAGE_DAYS);

            // select coins to use. we ensure we add MIN_TXOUT_AMOUNT so there's always enough funds
            // to populate the change address with a valid amount, regardless of fee paid to us.
            int64 amt = part.inOuts.nSendAmount = (this->nSendAmount)+MIN_TXOUT_AMOUNT;
            if (!pwalletMain->SelectCoinsMinConf(amt, GetAdjustedTime(), 1, 6, vCoins, setCoins, nChange))
            {
                error("WARNING: CCloakingRequest::GetInputsOutputs(): failed to select coins (%s) for input.", FormatMoney(amt).c_str());
                return false;
            }

            // add inputs / fill vins
            // get previous tx and index of input we want to spend
            BOOST_FOREACH(const PAIRTYPE(const CWalletTx*, uint)& coin, setCoins)
            {
                // mark the coins as enigma reserved
                CWalletTx& wtxIn = pwalletMain->mapWallet[coin.first->GetHash()];
                wtxIn.MarkEnigmaReserved(coin.second);
                part.inOuts.vin.push_back(CTxIn(coin.first->GetHash(), coin.second));
            }

            // store the stealth address unique hash so the sender can derive one-time payment addresses for our stealth address
            // and we can then claim those funds using the hash and unique output index
            part.inOuts.stealthHash = part.stealthAddressObj.GetEnigmaAddressPrivateHash();
            part.inOuts.nSendAmount = this->nSendAmount;
            part.inOuts.nInputAmount = nChange;

            // add the CCloakingInputsOutputs object to our local mem queue
            // note: we use request id hash as map index
            // note: we should consider storing these in the db so we can easily restore state when wallet reloaded.
            {
                LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
                pwalletMain->mapCloakingInputsOutputs[this->GetIdHash()] = part.inOuts;
            }

            return true;
        }
    }catch (std::exception& e){
        error("CCloakingRequest::GetInputsOutputsMine: %s.", e.what());
    }
    return false;
}

// retry a failed Enigma requst
bool CCloakingRequest::Retry()
{
    try
    {
        CCloakingRequest req;
        {
            LOCK2(pwalletMain->cs_mapCloakingInputsOutputs, pwalletMain->cs_mapOurCloakingRequests);

            // find the inputs and outputs
            uint256 oldHash = this->identifier.GetHash();
            if (!pwalletMain->mapCloakingInputsOutputs.count(oldHash))
            {
                if(GetBoolArg("-printenigma", false))
                    OutputDebugStringF("CCloaSendEnigmaMessage(myKey, ENIGMA_MSG_SIGN_INPUTS_OUTPUTS_MISSING);kingRequest::Retry - cant find req %s\n", oldHash.ToString().c_str());
                return false;
            }

            // get the inouts and request
            CCloakingInputsOutputs inOuts = pwalletMain->mapCloakingInputsOutputs[oldHash];
            req = pwalletMain->mapOurCloakingRequests[oldHash];

            // update request properties - this will change the underlying hash
            req.identifier.nInitiatedTime = GetAdjustedTime();
            req.aborted = false;
            req.signedByAddresses.clear();
            req.sParticipants.clear();
            req.bBroadcasted = true;
            req.nBroadcastedTime = GetAdjustedTime();
            req.retryCount++;

            // store the request
            uint256 newHash = req.identifier.GetHash();

            if(GetBoolArg("-printenigma", false))
                OutputDebugStringF("CCloakingRequest::Retry - new hash %s\n", newHash.ToString().c_str());

            pwalletMain->mapOurCloakingRequests[newHash] = req;

            // store CCloakingInputsOutputs and tag with request hash
            pwalletMain->mapCloakingInputsOutputs[newHash] = inOuts;

            // remove old request and inouts
            pwalletMain->mapCloakingInputsOutputs.erase(oldHash);
            pwalletMain->mapOurCloakingRequests.erase(oldHash);

            // mark the request as updated to trigger display update in GUI
            req.WasUpdated();

            // relay request message to peers
            return CCloakShield::GetShield()->SendEnigmaRequest(&req);
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingRequest::Retry: %s.", e.what());
    }
    return false;
}

CCloakingRequest* CCloakingRequest::CreateForBroadcast(int participantsRequired, int numSplits, int timeoutSecs, const CCloakingEncryptionKey* myKey, int64 sendAmount)
{
    CCloakingRequest* cr = new CCloakingRequest();
    cr->nParticipantsRequired = participantsRequired; // requester plus [ENIGMA_MIN_PARTICIPANTS-1] other participants
    cr->numSplitsRequired = numSplits;
    cr->timeoutSecs = timeoutSecs;
    cr->identifier.pubKeyHex = myKey->pubkeyHex;
    cr->identifier.nInitiatedTime = GetAdjustedTime();
    cr->nSendAmount = sendAmount;
    cr->isMine = true; // mark as owner

    // generate base stealth secret for deriving payment returns to cloakers and sender
    ec_secret secret;
    GenerateRandomSecret(secret);
    SecretToPublicKey(secret, cr->stealthRootKey);

    // mark the request as sent and send it
    cr->bBroadcasted = true;
    cr->nBroadcastedTime = GetAdjustedTime();
    return cr;
}

bool CCloakingRequest::ShouldAbort()
{
    try
    {
        if (!this->completed && !this->aborted && this->IsExpired())
        {
            if (!pwalletMain->mapCloakingInputsOutputs.count(this->GetIdHash()))
            {
                CCloakingInputsOutputs inOuts = pwalletMain->mapCloakingInputsOutputs[this->GetIdHash()];
                return nBestHeight >= inOuts.lockedUntilBlock;
            }
        }
    }catch (std::exception& e){
        error("CCloakingRequest::Abort: %s.", e.what());
    }
    return true;
}

// cancel the cloaking request as either the cloaker or participant
// this will restore any funds held for participation
bool CCloakingRequest::Abort()
{
    try
    {
        // mark the request as aborted and save it to disk
        this->aborted = true;

        if (!pwalletMain->mapCloakingInputsOutputs.count(this->GetIdHash()))
        {
            printf("WARNING: Enigma: Abort - Failed to find locked inputs to abort request #%s\n", this->GetIdHash().GetHex().c_str());
        }else{
            {
                LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
                // free the reserved funds (they will be cleared on exit too)
                CCloakingInputsOutputs inOuts = pwalletMain->mapCloakingInputsOutputs[this->GetIdHash()];
                BOOST_FOREACH(CTxIn& input, inOuts.vin)
                {
                    if (!pwalletMain->mapWallet.count(input.prevout.hash))
                    {
                        printf("WARNING: Enigma: Abort - Failed to unlock unused input for CTxOut #%s\n", input.prevout.hash.GetHex().c_str());
                    }else{
                        // remove fEnigmaReserved flag so coins can be treated as 'available' once more.
                        CWalletTx& wtxIn = pwalletMain->mapWallet[input.prevout.hash];
                        wtxIn.MarkNotEnigmaReserved(input.prevout.n);
                    }
                }
            }
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingRequest::Abort: %s.", e.what());
    }
    return false;
}

// ***** split this up into 2 additional methods, one for cloaker and one for sender
// sign a enigma transaction (as cloaker or sender)
bool CCloakingRequest::SignAndRespond(CCloakingEncryptionKey* myKey, bool participantSigning)
{
    try
    {
        LOCK(pwalletMain->cs_CloakingRequest);
        {
            // If we are a participant in this cloaking request,
            // then sign it and send the response
            CCloakShield* cs = CCloakShield::GetShield();
            uint256 idhash = this->GetIdHash();
            CCloakingRequest* reqOurCopy = NULL;

            // get our cached copy of the request so we can check we've not already signed
            if (participantSigning){
                LOCK(pwalletMain->cs_mapCloakingRequests);
                if (pwalletMain->mapCloakingRequests.find(idhash) != pwalletMain->mapCloakingRequests.end()){
                    reqOurCopy = &pwalletMain->mapCloakingRequests[idhash];
                }else{
                    Enigma::SendEnigmaMessage(myKey, ENIGMA_MSG_SIGN_REQUEST_NOT_FOUND, this->identifier.pubKeyHex, this->GetIdHash().GetHex());
                    if(GetBoolArg("-printenigma"))
                        printf("CCloakingRequest::SignAndRespond: cached request not found.\n");

                    // increment rejected counter to update GUI display
                    nCloakerCountRefused++;
                    return false;
                }
            }else{
                LOCK(pwalletMain->cs_mapOurCloakingRequests);
                if (pwalletMain->mapOurCloakingRequests.find(idhash) != pwalletMain->mapOurCloakingRequests.end()){
                    reqOurCopy = &pwalletMain->mapOurCloakingRequests[idhash];
                }else{
                    if(GetBoolArg("-printenigma"))
                        printf("CCloakingRequest::SignAndRespond: cached request not found (mine).\n");
                    return false;
                }
            }

            // check our copy to see if we've signed it already
            BOOST_FOREACH(string add, reqOurCopy->signedByAddresses){
                if (add == myKey->pubkeyHex){
                    if(GetBoolArg("-printenigma"))
                        printf("CCloakingRequest::SignAndRespond: already signed by me.\n");
                    return false; // already signed by me
                }
            }

            vector<unsigned char> txData(ParseHex(signedrawtx));
            CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
            vector<CTransaction> txVariants;
            while (!ssData.empty()){
                try{
                    CTransaction tx;
                    ssData >> tx;
                    txVariants.push_back(tx);
                }catch (std::exception &e){
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
                }
            }

            if (txVariants.empty())
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transaction");

            // mergedTx will end up with all the signatures; it
            // starts as a clone of the rawtx:
            CTransaction jointTrans(txVariants[0]);

            // get the inputs and outputs we supplied to the shared transaction request
            bool found = false;
            CCloakingInputsOutputs inoutsMine;
            LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
            if (pwalletMain->mapCloakingInputsOutputs.find(GetIdHash()) != pwalletMain->mapCloakingInputsOutputs.end()){
                inoutsMine = pwalletMain->mapCloakingInputsOutputs[GetIdHash()];
                found = true;
            }

            if(found){
                // check our inputs and outputs are included in the joint transaction
                int idx = 0;                
                BOOST_FOREACH(const CTxIn invMine, inoutsMine.vin){ // inputs from us
                    // check our input exists (unaltered) in the tx
                    if (find(jointTrans.vin.begin(), jointTrans.vin.end(), invMine) == jointTrans.vin.end()){
                        // ban the Enigma sender
                        cs->BanPeer(this->identifier.pubKeyHex, ENIGMA_BANSECS_MISSING_INPUTS);
                        nCloakerCountRefused++;

                        if(GetBoolArg("-printenigma"))
                            printf("***  CCloakingRequest::SignAndRespond: signed.\n");

                        Enigma::SendEnigmaMessage(myKey, ENIGMA_MSG_SIGN_INPUT_NOT_FOUND, this->identifier.pubKeyHex, this->GetIdHash().GetHex());
                        return error("*** CCloakingRequest::SignAndRespond() could not find input #%d in transaction.", idx);
                    }
                    idx++;
                }
                idx = 0;
                std::vector<uint8_t> vchEphemPK;
                std::vector<uint8_t> vchENarr;
                CScript::const_iterator itTxA;
                mapValue_t mapNarr;
                CTxOut stealthReturn;
                if (!pwalletMain->GetStealthEphemPK(jointTrans, mapNarr, vchEphemPK, vchENarr, itTxA, stealthReturn)){
                    return false;
                }
                uint64 amountOut = pwalletMain->GetEnigmaOutputsAmounts(jointTrans, vchEphemPK);
                if (participantSigning && amountOut < inoutsMine.nInputAmount){
                    cs->BanPeer(this->identifier.pubKeyHex, ENIGMA_BANSECS_MISSING_INPUTS);
                    nCloakerCountRefused++;
                    Enigma::SendEnigmaMessage(myKey, ENIGMA_MSG_SIGN_CHANGE_MISSING, this->GetIdHash().GetHex(), this->identifier.pubKeyHex);
                    return error("*** CCloakingRequest::SignAndRespond() change too small. Paying %s, getting %s.", FormatMoney(inoutsMine.nInputAmount).c_str(), FormatMoney(amountOut).c_str());
                }
            }else if (participantSigning){
                cs->BanPeer(this->identifier.pubKeyHex, ENIGMA_BANSECS_MISSING_INPUTS);
                nCloakerCountRefused++;
                Enigma::SendEnigmaMessage(myKey, ENIGMA_MSG_SIGN_INPUTS_OUTPUTS_MISSING, this->GetIdHash().GetHex(), this->identifier.pubKeyHex);
                return error("*** CCloakingRequest::SignAndRespond() could not our find inputs/outputs.");
            }

            // Sign
            int nHashType = SIGHASH_ALL | SIGHASH_ANYONECANPAY;
            const CKeyStore& keystore = *pwalletMain;

            // Fetch previous transactions (inputs):
            map<COutPoint, CScript> mapPrevOut;
            for (uint i = 0; i < jointTrans.vin.size(); i++){
                CTransaction tempTx;
                MapPrevTx mapPrevTx;
                CTxDB txdb("r");
                map<uint256, CTxIndex> unused;
                bool fInvalid;

                // FetchInputs aborts on failure, so we go one at a time.
                tempTx.vin.push_back(jointTrans.vin[i]);
                if (!tempTx.FetchInputs(txdb, unused, false, false, mapPrevTx, fInvalid)){
                    if (fInvalid){
                        cs->BanPeer(this->identifier.pubKeyHex, ENIGMA_BANSECS_MISSING_INPUTS);
                        nCloakerCountRefused++;
                        return error("*** CCloakingRequest::SignAndRespond() : FetchInputs found invalid tx %s", tempTx.GetHash().ToString().substr(0,10).c_str());
                    }
                    return false;
                }

                // Copy results into mapPrevOut:
                BOOST_FOREACH(const CTxIn& txin, tempTx.vin){
                    const uint256& prevHash = txin.prevout.hash;
                    if (mapPrevTx.count(prevHash) && mapPrevTx[prevHash].second.vout.size()>txin.prevout.n){
                        mapPrevOut[txin.prevout] = mapPrevTx[prevHash].second.vout[txin.prevout.n].scriptPubKey;
                    }else{
                        cs->BanPeer(this->identifier.pubKeyHex, ENIGMA_BANSECS_MISSING_INPUTS);
                        nCloakerCountRefused++;
                        return error("*** CCloakingRequest::SignAndRespond() : mapPrevOut failed");
                    }
                }
            }

            bool fComplete = true;
            string msg= "";
            msg.append(strprintf("Enigma Request: %s\n", this->GetIdHash().GetHex().c_str()));
            msg.append(strprintf("My Enigma Address: %s\n", nMyEnigmaAddress.c_str()));

            {
                LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
                found = pwalletMain->mapCloakingInputsOutputs.find(this->GetIdHash()) != pwalletMain->mapCloakingInputsOutputs.end();

                if(found){
                    // found our inputs
                    vector<CTxIn> ourInputs;
                    {
                        LOCK(pwalletMain->cs_mapCloakingInputsOutputs);
                        ourInputs = pwalletMain->mapCloakingInputsOutputs[this->GetIdHash()].vin;
                    }

                    // Sign what we can:
                    for (uint i = 0; i < jointTrans.vin.size(); i++){
                        CTxIn& txInput = jointTrans.vin[i];
                        if (mapPrevOut.count(txInput.prevout) == 0){
                            fComplete = false;
                            continue;
                        }

                        bool isOurs = find(ourInputs.begin(), ourInputs.end(), txInput) != ourInputs.end();
                        if (isOurs){
                            txInput.scriptSig.clear();

                            // sign our inputs
                            const CScript& prevPubKey = mapPrevOut[txInput.prevout];
                            if (!SignSignature(keystore, prevPubKey, jointTrans, i, nHashType)){
                                printf("CCloakingRequest::SignAndRespond - failed to sign %s\n", participantSigning ? "[part]" : "[sender]");
                                return false;
                            }

                            // ... and merge in other signatures. we randomize the order of combining here to mitigate
                            // decloaking attacks that analyze the signature order to ascertain last signer as sender.
                            if (GetRandBool()){
                                txInput.scriptSig = CombineSignatures(prevPubKey, jointTrans, i, txInput.scriptSig, jointTrans.vin[i].scriptSig);
                            }else{
                                txInput.scriptSig = CombineSignatures(prevPubKey, jointTrans, i, jointTrans.vin[i].scriptSig, txInput.scriptSig);
                            }
                            if (!VerifyScript(txInput.scriptSig, prevPubKey, jointTrans, i, true, 0))
                                fComplete = false;
                        }
                    }

                }
            }

            CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
            ssTx << jointTrans;

            if (participantSigning){
                CCloakingSignResponse ccsr;
                ccsr.signingAddress = myKey->pubkeyHex;
                BOOST_FOREACH(CTxIn& input, jointTrans.vin){
                    ccsr.signatures.push_back(input.scriptSig);
                }
                ccsr.identifier = this->GetIdHash();

                // Relay to nodes
                CCloakingSecret secret;
                myKey->GetSecret(this->identifier.pubKeyHex, secret, true);
                CCloakingData cloakData;
                vector<CCloakingSecret> recipPubKeys;
                recipPubKeys.push_back(secret);
                cloakData.EncodeEncryptTransmit((void*)&ccsr, CLOAK_DATA_SIGN_RESPONSE, recipPubKeys, myKey, ENCRYPT_PAYLOAD_YES, TRANSMIT_YES, ONION_ROUTING_ENABLED);
                nEnigmaProcessed++;
                nCloakerCountSigned++;

                uint256 hash = GetIdHash();
                pwalletMain->mapCloakingInputsOutputs[hash].requestSigned = true;
                // keep the inputs/outputs locked until the next block to avoid freeing too early
                pwalletMain->mapCloakingInputsOutputs[hash].lockedUntilBlock = nBestHeight+1;
            }else{
                // we are signing (last) as the sender
                // add our address to signers so we don't reprocess relayed messages
                this->signedByAddresses.insert(myKey->pubkeyHex);
                this->signedrawtx = HexStr(ssTx.begin(), ssTx.end());

                if (fComplete){
                    // tx created and signed, submit :)
                    if (!jointTrans.CheckTransaction()){
                        return error("*** CTxMemPool::accept() : Enigma CheckTransaction failed");
                    }

                    // mark the request as sent and send it
                    bBroadcasted = true;
                    nBroadcastedTime = GetAdjustedTime();

                    CTxDB txdb("r");

                    vector<unsigned char> txData(ParseHex(signedrawtx));
                    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
                    vector<CTransaction> txVariants;
                    while (!ssData.empty()){
                        try{
                            CTransaction tx;
                            ssData >> tx;
                            txVariants.push_back(tx);
                        }catch (std::exception &e){
                            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Enigma TX decode failed");
                        }
                    }

                    if (txVariants.empty())
                        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing Enigma transaction");

                    // mergedTx will end up with all the signatures; it
                    // starts as a clone of the rawtx:
                    //CTransaction jointTrans(txVariants[0]);
                    bool accepted = jointTrans.AcceptToMemoryPool(txdb, true);

                    if (accepted){
                        uint256 hashTx = jointTrans.GetHash();
                        this->txid = hashTx.GetHex();

                        // mark complete (only really important for participants [counts] atm, but we do it for good measure)
                        this->completed = true;

                        // See if the transaction is already in a block
                        // or in the memory pool:
                        CTransaction existingTx;
                        uint256 hashBlock = 0;
                        if (GetTransaction(hashTx, existingTx, hashBlock)){
                            if (hashBlock != 0)
                                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Enigma transaction already in block ")+hashBlock.GetHex());
                            // Not in block, but already in the memory pool; will drop
                            // through to re-relay it.
                        }else{
                            // push to local node
                            CTxDB txdb("r");
                            bool accepted = jointTrans.AcceptToMemoryPool(txdb, true);

                            if (!accepted)
                                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Enigma TX rejected");

                            SyncWithWallets(jointTrans, NULL, true);
                        }

                        // relay (via onion if needed)
                        if(fOnionRouteAll && ONION_ROUTING_ENABLED)
                            cs->OnionRouteTx(jointTrans);
                        else
                            RelayMessage(CInv(MSG_TX, hashTx), jointTrans);

                        // add to wallet
                        if (pwalletMain->AddToWalletIfInvolvingMe(jointTrans, NULL, false, false)){
                            // all good. we should reduce the banscore for cloakers who participated in this cloaking
                            // check our copy to see if we've signed it already
                            BOOST_FOREACH(string add, this->signedByAddresses){
                                if (add != myKey->pubkeyHex)
                                    cs->ClearBan(add);
                            }
                        }
                    }
                }
            }
            return true;
        }
    }catch (std::exception& e){
        error("*** CCloakingRequest::SignAndRespond: %s.", e.what());
    }
    return false;
}

string CCloakingRequest::ToString() const
{
    try
    {
        string strSignedBy;
        BOOST_FOREACH(string strSb, signedByAddresses)
        strSignedBy += strSb + "\n";

        string strParticipants;
        BOOST_FOREACH(CCloakingParticipant p, sParticipants){
            strParticipants += "        Participant signing address: " + p.pubKey + "\n";
            //BOOST_FOREACH(string sDest, p.destinations)
            //strParticipants += "         Destination: " + sDest + "\n";
        }

        return strprintf(
            "CCloakingRequest(\n"
            "    nVersion                = %d\n"
            "    initiatorAddress	     = %s\n"
            "    nInitiatedTime           = %" PRI64d "\n"
            "    nTotalAmount            = %" PRI64d "\n"
            "    nParticipantsRequired   = %d\n"
            "    txid                    = %s\n"
            "    rawtx                   = %s\n"
            "    signedrawtx             = %s\n"
            "    nBroadcastedTime        = %" PRI64d "\n"
            "    Participants	         = %s\n"
            ")\n",
            nVersion,
            identifier.pubKeyHex.c_str(),
            identifier.nInitiatedTime,
            nSendAmount,
            nParticipantsRequired,
            txid.c_str(),
            rawtx.c_str(),
            signedrawtx.c_str(),
            nBroadcastedTime,
            strParticipants.c_str());
    }catch (std::exception& e){
        error("CCloakingRequest::ToString: %s.", e.what());
    }
    return "ERROR";
}

void CCloakingRequest::print() const
{
    printf("%s", ToString().c_str());
}

void CCloakingRequest::SetNull()
{
    nVersion = ENIGMA_REQUEST_CURRENTVERSION;
    identifier.SetNull();
    nSendAmount = 0;
    nParticipantsRequired = 3;
    sParticipants.clear();
    txid = "0";  // the "address" that participants will fund for the multisig tx
    rawtx = "";
    txCreated = false;    
    timeoutSecs = 60;
    signedrawtx = "";
    stealthRootKey.clear();
    signedByAddresses.clear(); // to keep track
    bBroadcasted = false;
    processed = false;
    aborted = false;
    nBroadcastedTime = 0;
    isMine = false;
    completed = false;
    autoRetry = (ENIGMA_FORCE_AUTO_RETRY || fEnigmaAutoRetry) && !ENIGMA_BLOCK_AUTO_RETRY;
    retryCount = 0;
}

// encrypt the cloak-request containing the shared transaction and request participants sign it
// we encrypt the data using a one-time key and then send the key for each user (encrypted using
// the derived ECDH key). This avoids sending extra messages but could also be revisited later
// using group ECDH...
bool CCloakingRequest::EncryptSendSignRequest(CCloakingEncryptionKey* myKey) const
{
    try
    {
        vector<CCloakingSecret> recipPubKeys;
        BOOST_FOREACH(CCloakingParticipant p, sParticipants)
        {
            CCloakingSecret secret;
            myKey->GetSecret(p.pubKey, secret, true);
            recipPubKeys.push_back(secret);
        }

        // wrap accept-respose in encrypted cloak-data object and broadcast
        CCloakingData cloakData;

        // encode accept response
        return cloakData.EncodeEncryptTransmit((void*)this, CLOAK_DATA_SIGN_REQUEST, recipPubKeys, myKey, ENCRYPT_PAYLOAD_YES, TRANSMIT_YES, ONION_ROUTING_ENABLED);
    }catch (std::exception& e){
        error("CCloakingRequest::EncryptSendSignRequest: %s.", e.what());
    }
    return false;
}

uint256 CCloakingRequest::GetIdHash() const
{
    return this->identifier.GetHash();
}

bool CCloakingRequest::DeserializeTx(string tx, CTransaction& txOut)
{
    try
    {
        // update our local copy of the tx with the newly signed inputs
        vector<unsigned char> txData(ParseHex(tx));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        vector<CTransaction> txVariants;
        while (!ssData.empty())
        {
            try
            {
                CTransaction txT;
                ssData >> txT;
                txVariants.push_back(txT);
            }
            catch (std::exception &e)
            {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
            }
        }

        if (txVariants.empty())
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transaction");

        txOut = txVariants[0];
        return true;
    }
    catch (std::exception& e) {
        error("DeserializeTx: %s", e.what());
    }

    return false;
}

bool CCloakingRequest::AddSignedInputsToTx(CCloakingSignResponse* signResponse)
{
    try
    {
        // get the original transaction for the cloak request
        CTransaction txRequest;
        DeserializeTx(this->signedrawtx, txRequest);

        // copy the signed inputs from the copy signed by cloaker to our original tx
        bool updated = false;
        if (signResponse->signatures.size() != txRequest.vin.size()){
            return false;
        }
        for(uint i=0; i<signResponse->signatures.size(); i++)
        {
            if (signResponse->signatures[i].size() > txRequest.vin[i].scriptSig.size())
            {
                txRequest.vin[i].scriptSig = signResponse->signatures[i];
                updated = true;
            }
        }

        if (updated)
        {
            CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
            ssTx << txRequest;
            this->signedrawtx = HexStr(ssTx.begin(), ssTx.end());
        }
        this->signedByAddresses.insert(signResponse->signingAddress);
        return true;
    }
    catch (std::exception& e) {
        error("AddSignedInputsToTx: %s", e.what());
    }
    return false;
}
