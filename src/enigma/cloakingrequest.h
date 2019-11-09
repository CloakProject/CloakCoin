#ifndef CLOAKINGREQUEST_H
#define CLOAKINGREQUEST_H

#include <set>
#include <map>
#include <vector>
#include <string>
#include <boost/foreach.hpp>
#include <openssl/ec.h> // for EC_KEY definition

#include "main.h"
#include "uint256.h"
#include "util.h"
#include "sync.h"
#include "lrucache.h"
#include "cloakingdata.h"

using namespace std;

class CCloakingParticipant;
class CCloakingInputsOutputs;
class CCloakingAcceptResponse;
class CCloakingSignResponse;
class CWalletTx;

// data types for cloaking data
typedef enum EnigmaTransactionType
{
    ENIGMA_TXTYPE_NOTSET   = 0,
    ENIGMA_TXTYPE_SENDER   = 1,
    ENIGMA_TXTYPE_CLOAKER  = 2,
    ENIGMA_TXTYPE_RECEIVER = 3
}EnigmaTransactionType;

// result from calling an enigma function
typedef enum EnigmaFunctionResult
{
    ENIGMA_FUNC_RES_OK   = 0,
    ENIGMA_FUNC_RES_FAIL = 1,
    ENIGMA_FUNC_RES_DOS  = 2
}EnigmaFunctionResult;

typedef enum EnigmaMessageCode
{
    ENIGMA_MSG_OK                          = 0,
    ENIGMA_MSG_CLOAKER_ACCEPTED            = 1,
    ENIGMA_MSG_CLOAKER_REJECTED            = 2,
    ENIGMA_MSG_SIGN_REQUEST_NOT_FOUND      = 3,
    ENIGMA_MSG_SIGN_INPUTS_OUTPUTS_MISSING = 4,
    ENIGMA_MSG_SIGN_INPUT_NOT_FOUND        = 5,
    ENIGMA_MSG_SIGN_CHANGE_MISSING         = 6,
    ENIGMA_MSG_NODE_GONE_OFFLINE           = 7
}EnigmaMessageCode;

// ID for a cloaking operation.
class CCloakingIdentifier
{
public:
    string pubKeyHex; // initiator address
    int64 nInitiatedTime;
    uint256 hash;

    CCloakingIdentifier()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->pubKeyHex);
        READWRITE(this->nInitiatedTime);
    )

    void SetNull();
    uint256 GetHash() const;
};

// request to other nodes in network to participate in the Enigma transaction to 'cloak' coins.
class CCloakingRequest
{
public:

    int nVersion;
    CCloakingIdentifier identifier;
    uint64 nSendAmount; // amount the all participants must fund
    int nParticipantsRequired; // 3, 6, or some number of total participants needed including initiator
    string rawtx; // the original transaction
    string signedrawtx; // the multi-signed version of the rawtx. this is sequentially signed by participants (we should have already signed it).
    set<string> signedByAddresses; // to keep track of addresses that have already signed rawtx (signed version stored in signedrawtx)
    string recipientAddress; // keep track of recipient to prevent him from being enigma assistant.
    bool bBroadcasted;
    int64 nBroadcastedTime;
    bool processed; // if this is one of our requests, have
    bool aborted; // cancelled/abandoned
    int numSplitsRequired;

    bool isMine;
    bool completed;
    bool txCreated;
    bool autoRetry;
    int retryCount;
    int timeoutSecs;

    // LOCAL MEMORY ONLY - never serialized
    string txid;  // txid of finalized transaction
    set<CCloakingParticipant> sParticipants;
    ec_point stealthRootKey; // hash for deriving output addresses from stealth addresses
    CStealthAddress senderAddress; // private ref to sender stealth address for change
    // comments for the finalized wallet tx - NOT YET USED
    string txComment;
    string txCommentTo;

    CCloakingRequest()
    {
        SetNull();
    }

    bool AddCorrectFee(CCloakingInputsOutputs* inOutsParticipants, CCloakingInputsOutputs* inOutsOurs, int senderChangeStartIndex);
    bool CreateTransactionAndRequestSign(CCloakingEncryptionKey* myKey);
    bool CreateEnigmaOutputs(CCloakingInputsOutputs &inOutsCloakers, CCloakingInputsOutputs &inOutsOurs, CCloakingInputsOutputs &inOutsFinal);
    bool CreateCloakerTxOutputs(vector<CTxOut>& outputs);
    bool GetCloakingInputsMine(CCloakingParticipant& part);
    bool IsExpired();
    int TimeLeftMs();
    bool ShouldRetry();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        READWRITE(this->identifier);
        READWRITE(this->nSendAmount);
        READWRITE(this->nParticipantsRequired);
        READWRITE(this->rawtx);
        READWRITE(this->signedrawtx);
        READWRITE(this->signedByAddresses);
        READWRITE(this->bBroadcasted);
        READWRITE(this->nBroadcastedTime);
        READWRITE(this->processed);
        READWRITE(this->aborted);
        READWRITE(this->numSplitsRequired);
        READWRITE(this->isMine);
        READWRITE(this->completed);
        READWRITE(this->autoRetry);
        READWRITE(this->retryCount);
        READWRITE(this->timeoutSecs);
    )

    string ToString() const;
    void print() const;
    void SetNull();
    uint256 GetIdHash() const;
    bool RelayToAll(bool shuffle = true) const;
    bool RelayTo(CNode* pnode) const;
    bool EncryptSendSignRequest(CCloakingEncryptionKey* myKey) const;
    bool ProcessRequest(CCloakingEncryptionKey* myKey);
    bool GetCloakingInputsOutputsMine(CCloakingParticipant& part);
    bool SignAndRespond(CCloakingEncryptionKey* myKey, bool participantSigning = true);

    bool DeserializeTx(string tx, CTransaction& txOut);
    bool AddSignedInputsToTx(CCloakingSignResponse* signResponse);
    bool WasUpdated();
    bool ShouldAbort();
    bool Abort();
    bool Retry();

    static CCloakingRequest* CreateForBroadcast(int participantsRequired, int numSplits, int timeoutSecs, const CCloakingEncryptionKey* myKey, int64 sendAmount);
};

#endif // CLOAKINGREQUEST_H
