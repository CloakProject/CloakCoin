// response contains destination addresses
// tx created


// Copyright (c) 2014 The CloakCoin Developers
// Copyright (c) 2013-2014 The FedoraCoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
todo:
- when wallet is unlocked for enigma participation, check enigma request map for any enigma transactions we still have time to participate in.
- requester and participants should check their outputs are still valid before signing the transaction.
- GUI should display enigma processing info.
- investigate alternate means of flagging an Enigma transaction. perhaps using our Enigma address as one of the change addresses so we can detect the marker
within transactions to identify them as Enigma transactions. current method may be too liberal and flag other transaction types.
- any usused keys should be returned if a Enigma transaction does not complete or is aborted.
- participants should check fees before signing to circumvent


- when decrypting a cloak shield package, the signing address should be checked to ensure it matches the content (if applicable). this will ensure that
- a cloaking acceptance has the correct signing address for a participant and will help to catch nefarious monkeys.

- detecting incoming transations - if enigma scanning finds a match, we are a cloaker or the sender
- if there's more money output to us than we put in, we're a cloaker
- if there's less money output to us than we put in, we're the sender
- if we get just a standard stealth match rather than an enigma one, we're the recipient
- this covers all bases and negates the need to store inputs/outputs in the wallet for tx detection

*/

/*

  remove the participant info and other unneeded items for cloaking requests when sent to other nodes

*/

#ifndef _ENIGMA_H_
#define _ENIGMA_H_ 1

#include <set>
#include <string>
#include <boost/foreach.hpp>

#include "main.h"
#include "uint256.h"
#include "util.h"
#include "sync.h"
#include "lrucache.h"

#include <openssl/ec.h> // for EC_KEY definition

#include "cloakshield.h"
#include "cloakingrequest.h"

#include "stealth.h"

using namespace std;

void ThreadEnigma(void* parg);
void StartProcessingEnigma();

class CNode;
class CTxIn;
class CTxOut;
class CWalletTx;
class CCloakingAcceptResponse;

class CCloakingMessage
{
    public:
        short messageCode;
        string messageData;

    CCloakingMessage()
    {
        SetNull();
    }

    CCloakingMessage(short code, string message)
    {
        messageCode = code;
        messageData = message;
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->messageCode);
        READWRITE(this->messageData);
    )

    void SetNull();
    uint256 GetHash() const;

    private:
    uint256 hash;
};

// response sent to cloaking requester once we've signed the joint/shared transaction
class CCloakingSignResponse
{
public:
    string signingAddress;
    uint256 identifier; // hash of request we have signed
    uint256 signResponseId; // unique id for this sign response
    vector<CScript> signatures;

    CCloakingSignResponse()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->signingAddress);
        READWRITE(this->identifier);
        READWRITE(this->signResponseId);
        READWRITE(this->signatures);
    )

    void SetNull();
    uint256 GetHash() const;    
    bool ProcessSignatureResponse(CCloakingEncryptionKey* myKey);

private:
        uint256 hash;
};

inline bool operator<(const CCloakingSignResponse& a, const CCloakingSignResponse& b)
{
    return a.GetHash() < b.GetHash();
}

inline bool operator>(const CCloakingSignResponse& a, const CCloakingSignResponse& b)
{
    return a.GetHash() > b.GetHash();
}

// inputs/outputs for a shared/joint transaction. this can represent
// 'cloaking' inputs/outputs for a participant, or genuine inputs/outputs
// for the cloaking requester.
class CCloakingInputsOutputs
{
public:
    ushort nVersion;
    std::vector<CTxIn> vin; // tx inputs
    std::vector<CTxOut> vout; // tx outputs
    uint256 requestIdentifier; // hash of the request these in/outs are reserved for    
    int64 nSendAmount; // payment amount for enigma tx (amount being sent by sender)
    int64 nInputAmount; // our input amount for enigma tx
    uint256 stealthHash; // hash of private stealth data

    // memory only
    bool requestSigned; // was the assoicated enigma request signed by us?
    int64 lockedUntilBlock;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        READWRITE(this->vin);
        READWRITE(this->vout);
        READWRITE(this->requestIdentifier);
        READWRITE(this->nInputAmount);
        READWRITE(this->nSendAmount);
        READWRITE(this->stealthHash);        
    )

    CCloakingInputsOutputs()
    {
        SetNull();
    }

    void SetNull()
    {
        nVersion = ENIGMA_REQUEST_CURRENTVERSION;
        requestIdentifier = uint256(0);
        stealthHash = uint256(0);
        nInputAmount = 0;
        nSendAmount = 0;
        requestSigned = false;
        lockedUntilBlock = 0;
    }

    inline uint64 OutputAmount() const
    {
        uint64 res = 0;
        for(uint64 i=0; i<this->vout.size(); i++){
            res+=this->vout.at(i).nValue;
        }
        return res;
    }

    bool addVinCreateStealthVouts(const CCloakingParticipant &cloaker, const uint256 &randomHash);
    bool addVinVouts(CCloakingInputsOutputs inouts, bool addInputs = true, bool addOuputs = true);
    bool addVinVouts(vector<CCloakingInputsOutputs> inouts, bool addInputs = true, bool addOuputs = true);
    bool addVinVoutsToTx(CTransaction* tx, bool addInputs = true, bool addOuputs = true);
    bool shuffle(int numSwaps);
    bool assignReward(int64 rewardTotal, int64 sendAmount);
    //void markSpent();
    bool SenderCalcFee();
    bool CheckValid(int64 amountOut, bool printToLog = false, string ownerPubKey = "");
    vector<CBitcoinAddress> addressesIn();
    vector<CBitcoinAddress> addressesOut();
};

// represents a node participating in a cloaking request
class CCloakingParticipant
{
public:
    string pubKey; // public signer address
    std::string stealthAddress;
    mutable CCloakingInputsOutputs inOuts;

    // local memory only
    CStealthAddress stealthAddressObj;

    CCloakingParticipant()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->pubKey);
        READWRITE(this->stealthAddress);
        READWRITE(this->inOuts);
    )

    void SetNull();
    uint256 GetHash() const;    
};

inline bool operator<(const CCloakingParticipant& a, const CCloakingParticipant& b) 
{
    return a.GetHash() < b.GetHash();
}

inline bool operator>(const CCloakingParticipant& a, const CCloakingParticipant& b) 
{
    return a.GetHash() > b.GetHash();
}

// response enigma request
class CCloakingAcceptResponse
{
public:
    uint256 requestIdentifier; // hash of request this accept applies to
    CCloakingParticipant participantInfo;
    uint256 hash;

    CCloakingAcceptResponse()
    {
        SetNull();
    }  

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->requestIdentifier);
        READWRITE(this->participantInfo);
    )

    void SetNull();
    bool ProcessAcceptResponse(CCloakingEncryptionKey* myKey);
    uint256 GetHash() const;

};

inline bool operator<(const CCloakingAcceptResponse& a, const CCloakingAcceptResponse& b)
{
    return a.GetHash() < b.GetHash();
}

inline bool operator>(const CCloakingAcceptResponse& a, const CCloakingAcceptResponse& b)
{
    return a.GetHash() > b.GetHash();
}

/*
// object sent back to recipient when request is full to allow participant to free funds and participate in a different enigma operation
class CCloakingRejectResponse
{
public:
    uint256 requestIdentifier; // cloaking request hash
    std::vector<unsigned char> rejectionSignature; //

    CCloakingAcceptResponse()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->requestIdentifier);
        READWRITE(this->rejectionSignature);
    )

    void SetNull();
    void RelayTo(CNode* pnode) const;
    void ProcessResponse();
    uint256 GetHash() const;
};

inline bool operator<(const CCloakingRejectResponse& a, const CCloakingRejectResponse& b)
{
    return a.GetHash() < b.GetHash();
}

inline bool operator>(const CCloakingRejectResponse& a, const CCloakingRejectResponse& b)
{
    return a.GetHash() > b.GetHash();
}
*/

class CCloakingDoneInfo
{
public:
    ushort nVersion;
    CCloakingIdentifier requestId;
    uint256 hashTx; // hash of the enigma tx

    CCloakingDoneInfo()
    {
        SetNull();
    }

    CCloakingDoneInfo(CCloakingIdentifier requestIdentifier, uint256 txHash)
    {
        SetNull();
        requestId = requestIdentifier;
        hashTx = txHash;
    }

    void SetNull();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(this->requestId);
        READWRITE(this->hashTx);
    )

};



class Enigma
{
public:
    static CCloakingRequest* SendEnigma(CCloakingInputsOutputs inputsOutputs, CCloakingRequest* cr);
    static CCloakingRequest* SendEnigma(CCloakingInputsOutputs inputsOutputs, int numParticipants, int numSplits, int timeoutSecs, const CCloakingEncryptionKey* myKey);    
    static bool HandleCloakMessage(CCloakingData cloakData);
    static bool HandleOnionTx(CCloakingData cloakData);
    static bool HandleSignResponse(CCloakingData cloakData);
    static bool HandleSignRequest(CCloakingData cloakData);
    static bool HandleCloakingAccept(CCloakingData cloakData);
    static bool HandleCloakOnionData(CDataStream& vRecv, CNode* node);
    static bool HandleCloakOnionData(CCloakingData cloakDataIn, CNode* node, int level=0);
    static void LogCloakMessage(CCloakingMessage msg);
    static void HandleNodeOffline(string nodePubkeyHash);
    static bool HandleAcceptResponseFromSender(EnigmaMessageCode code, string requestIdHashHex);
    static bool SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, bool doNotOnionRoute = false);
    static bool SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, string recipientPubKey, bool doNotOnionRoute = false);
    static bool SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, CCloakingSecret recipient, bool doNotOnionRoute = false);
    static bool SendEnigmaMessage(CCloakingEncryptionKey* myKey, EnigmaMessageCode code, std::string data, vector<CCloakingSecret> recipients, bool doNotOnionRoute = false);
    static bool CloakShieldShutdown();

};

#endif
