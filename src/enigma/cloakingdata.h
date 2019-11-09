#ifndef CLOAKINGDATA_H
#define CLOAKINGDATA_H

#include <set>
#include <map>
#include <vector>
#include <string>
#include <boost/foreach.hpp>

#include "main.h"
#include "uint256.h"
#include "util.h"
#include "sync.h"
#include "lrucache.h"
#include <openssl/ec.h> // for EC_KEY definition

#define ENCRYPT_PAYLOAD_YES true
#define ENCRYPT_PAYLOAD_NO false
#define TRANSMIT_YES true
#define TRANSMIT_NO false
#define ONION_ROUTING_YES true
#define ONION_ROUTING_NO false

// data types for cloaking data
enum CloakingDataType
{
    CLOAK_DATA_UNKNOWN             = 0,
    CLOAK_DATA_REQ_ACCEPT          = 1, // cloaking request accept msg (part->req
    CLOAK_DATA_SIGN_REQUEST        = 2, // request from cloaker to participant to sign the shared transaction
    CLOAK_DATA_SIGN_RESPONSE       = 3, // signing result from a participant node to cloaker
    CLOAK_DATA_RELAY_DATA          = 4, // onion routing relay packet
    CLOAK_DATA_ENIGMA_ANNOUNCEMENT = 5, // enigma announcement
    CLOAK_DATA_ENIGMA_DONE         = 6, // finalized request which is routed back to participants when final enigma tx is created
    CLOAK_DATA_TX                  = 7, // Enigma or standard TX to be onion routed before submission to network
    CLOAK_DATA_ENIGMAREQ           = 8, // Enigma request to be onion routed out to network
    CLOAK_DATA_POS_BLOCK           = 9, // PoS block (we should onion route these for security!)
    CLOAK_DATA_ENIGMA_MSG          = 10, // Enigma message (cloaker rejected etc.)
};

class CCloakingEncryptionKey;
class CCloakingSecret;

// receipt for successfully received cloakdata packet
class CCloakingDataReceipt
{
public:
    uint256 cloakDataId;

    CCloakingDataReceipt()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->cloakDataId);
    )
    void SetNull();
};

// encrypted payload for a cloakdata packet
class CCloakingDataPayload
{
public:
    bool isCompressed;
    unsigned char dataType;
    std::vector<unsigned char> data; // encrypted & encoded data
    std::vector<unsigned char> dataRaw; // unencrypted data (memory only. we never serialize this)

    CCloakingDataPayload()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        // we only serialize the encrypted data and signature.
        // datatype is a local only object that gets packed into the payload [data] for sending.
        READWRITE(this->isCompressed);
        READWRITE(this->dataType);
        READWRITE(this->data);
    )

    void SetNull();
};

// ecrypted cloak data
class CCloakingData
{
public:
    int nVersion;
    uint256 identifier;
    std::string senderPubKeyHex;
    // todo: compress pubkeys!
    std::vector<std::pair<std::string, std::string> > recipientKeys; // pub key hex and corresponding encrypted secret hex
    CCloakingDataPayload payload;
    int64 timestamp;
    std::vector<unsigned char> vchSig;
    unsigned short maxHops;
    unsigned short hops;

    // mem only
    bool shouldForward; // should we forward the packet to peers? this is a MEMORY ONLY setting and should not be serialized.

    std::string _dataTypeName; // memory only - for debugging purposes

    CCloakingData()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->identifier);
        READWRITE(this->senderPubKeyHex);
        READWRITE(this->recipientKeys);
        READWRITE(this->payload);
        READWRITE(this->timestamp);
        READWRITE(this->vchSig);
        READWRITE(this->maxHops);
        READWRITE(this->hops);
    )

    void SetNull();
    bool Transmit(bool shuffle, bool onionRoute = false);
    bool Transmit(CNode* sender, bool shuffle = true, bool onionRoute = false);
    bool MarkProcessed(uint256 hash, bool processed = true);
    bool AlreadyProcessed();
    bool CanDecrypt(const CCloakingEncryptionKey* myKey);
    uint256 GetSigningHash();

    // encoding/decoding calls for cloaking object serialization/encryption
    bool DoPow();
    bool CheckPow();
    bool Sign(CCloakingEncryptionKey* myKey);
    bool Decode(void* objOut); // decode our object (be sure Decrypt() was successfully called first)
    bool Encode(const void* data, enum CloakingDataType cloakDataType, CCloakingEncryptionKey* myKey);
    bool Encode(const void* data, enum CloakingDataType cloakDataType);
    bool EncodeEncryptTransmit(const void* data, enum CloakingDataType cloakDataType, std::vector<CCloakingSecret> recipientSecrets, CCloakingEncryptionKey* myKey, const bool encrypt, const bool transmit, const bool onionRoute);
    bool Authenticate(); // check signature against sender pubkey and raw data

    bool Encrypt(std::vector<CCloakingSecret> recipients, const std::string myEnigmaPubkeyHex); // encrypt for one or more recipients
    bool Decrypt(CCloakingEncryptionKey* myKey); // decrypt (if intended for us, we will have a secret key in pwalletMain->mapCloakSecrets)

    void SetDataTypeName()
    {
        switch(payload.dataType)
        {
        case CLOAK_DATA_UNKNOWN:
            _dataTypeName = "CLOAK_DATA_UNKNOWN";
            break;

        case CLOAK_DATA_REQ_ACCEPT:
            _dataTypeName = "CLOAK_DATA_REQ_ACCEPT";
            break;

        case CLOAK_DATA_SIGN_REQUEST:
            _dataTypeName = "CLOAK_DATA_SIGN_REQUEST";
            break;

        case CLOAK_DATA_SIGN_RESPONSE:
            _dataTypeName = "CLOAK_DATA_SIGN_RESPONSE";
            break;

        case CLOAK_DATA_RELAY_DATA:
            _dataTypeName = "CLOAK_DATA_RELAY_DATA";
            break;

        case CLOAK_DATA_ENIGMA_ANNOUNCEMENT:
            _dataTypeName = "CLOAK_DATA_ENIGMA_ANNOUNCEMENT";
            break;

        case CLOAK_DATA_ENIGMA_DONE:
            _dataTypeName = "CLOAK_DATA_ENIGMA_DONE";
            break;

        case CLOAK_DATA_TX:
            _dataTypeName = "CLOAK_DATA_TX";
            break;

        case CLOAK_DATA_ENIGMAREQ:
            _dataTypeName = "CLOAK_DATA_ENIGMAREQ";
            break;

        case CLOAK_DATA_POS_BLOCK:
            _dataTypeName = "CLOAK_DATA_POS_BLOCK";
            break;

        case CLOAK_DATA_ENIGMA_MSG:
            _dataTypeName = "CLOAK_DATA_ENIGMA_MSG";
            break;
        }
    }

    //bool EncryptAndEncode();
    //bool DecryptAndDecode();
protected:
    bool RelayTo(CNode* pnode, bool onionRoute = ONION_ROUTING_ENABLED);
};


#endif // CLOAKINGDATA_H
