#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <map>

#include <openssl/ec.h> // for EC_KEY definition
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <openssl/ecdh.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>

#include "enigma.h"
#include "main.h"
#include "key.h"
#include "net.h"
#include "init.h"
#include "util.h"
#include "wallet.h"
#include "ui_interface.h"

#include "cloakshield.h"
#include "cloakingdata.h"
#include "encryption.h"
#include "pow.h"


#if defined(QT_GUI) && ENIGMA_DEBUGMSG_ENCRYPTION
    #include "bitcoingui.h"
    #include <QMessageBox>

    extern BitcoinGUI* GuiRef;
#endif

void CCloakingDataPayload::SetNull()
{
    isCompressed = false;
    dataType = CLOAK_DATA_UNKNOWN;
}

uint256 CCloakingData::GetSigningHash()
{
    // serialize everything except sig and hop count (as those are mutable)
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << this->identifier;
    ss << this->senderPubKeyHex;
    ss << this->recipientKeys;
    ss << this->payload;
    ss << this->timestamp;
    ss << this->maxHops;
    return ss.GetHash();
}

bool CCloakingData::Authenticate()
{
    CKey key;
    if (!key.SetPubKey(ParseHex(senderPubKeyHex)))
        return error("CCloakingData::Authenticate() : SetPubKey failed");
    if (!key.Verify(GetSigningHash(), vchSig))
        return error("CCloakingData::Authenticate() : verify signature failed");

    // check the data isn't too old
     if (GetAdjustedTime() >= (timestamp + CLOAKSHIELD_DATA_TIMEOUT_SECS + nMaxObservedTimeOffset))
        printf("Current data time (%" PRI64d ") should be within 60s after data was sent (%" PRI64d "),\n", GetAdjustedTime(), timestamp + CLOAKSHIELD_DATA_TIMEOUT_SECS);
    return true;
}

void CCloakingData::SetNull()
{
    try
    {
        nVersion = ENIGMA_REQUEST_CURRENTVERSION;
        senderPubKeyHex = "";
        maxHops = CLOAKSHIELD_MAX_HOPS;
        hops = 0;

        // generate unique hash
        CDataStream ss1(SER_GETHASH, 0);
        CCloakShield* cs = CCloakShield::GetShield();
        ss1 << GetTime() << cs->GetRoutingKey()->pubkeyHexHash;
        identifier = Hash(ss1.begin(), ss1.end());
        timestamp = GetAdjustedTime();
    }catch (std::exception& e){
        error("CCloakingData::SetNull: %s.", e.what());
    }
}

// relay a cloakdata packet. this handles onion layering for onion routing.
bool CCloakingData::Transmit(bool shuffle, bool onionRoute)
{
    return Transmit(NULL, shuffle, onionRoute);
}

// relay a cloakdata packet. this handles onion layering for onion routing.
bool CCloakingData::Transmit(CNode* sender, bool shuffle, bool onionRoute)
{
    try
    {
        if (!Authenticate()){
            printf("auth failed");
            if (sender){
                // small dos penalty for (directly connected) sending node
                sender->Misbehaving(5);
            }
        }
#ifdef QT_DEBUG

#endif

        CCloakShield* cs = CCloakShield::GetShield();
        vector<CCloakingData> packetsToSend;

        if ((ONION_ROUTING_ENABLED && fEnableOnionRouting) && onionRoute)
        {
            // data should be onion-routed to destination.
            // we use [nCloakShieldNumRoutes] different routes to the destination.
            // each route has a pool of [nCloakShieldNumNodes] nodes to construct the route from.
            // each route contains [nCloakShieldNumHops] hops from source to destination. Destination is always the last hop.
            // the cloakdata is packed into a series of cloakdata objects which can be decrypted and forwarded by any of the recipient
            // nodes chosen for the current hop. using a set of nodes per hop mitigates (to a degree) the risk of packets getting
            // lost if a hop-node goes offline during network packet transmission.

            // get sufficient active nodes
            vector<CEnigmaAnnouncement> relayNodes = CEnigmaAnnouncement::electAnonymizers(0);

            if (relayNodes.size() < (uint)cs->NumNodesRequired())
            {
                // not enough active Enigma nodes in our list.
                // TODO we should handle this automatically and gracefully somehow but for now we just abort
                printf("CCloakingData::RelayToAll - Not enough Enigma nodes to onion route");
                return false;
            }

            for (int route=0; route<cs->NumRoutesRequired(); route++)
            {
                // shuffle available nodes so we end up with a different subset per route
                std::random_shuffle(relayNodes.begin(), relayNodes.end(), GetRandInt);

                vector<CEnigmaAnnouncement> routeNodes;
                routeNodes.insert(routeNodes.end(), relayNodes.begin(), relayNodes.begin()+cs->NumNodesRequired());

                // get original data
                CCloakingData onionpacket = *this;

                for(int hop=0; hop<cs->NumHopsRequired(); hop++)
                {
                    // shuffle available nodes so we end up with a different subset per hop
                    std::random_shuffle(routeNodes.begin(), routeNodes.end(), GetRandInt);

                    // encode this data in wrapper data for target node
                    CCloakingData cloakData;
                    vector<CCloakingSecret> recipPubKeys;

                    // create secrets for each node on this hop
                    for (int node=0; node<cs->NumNodesRequired(); node++)
                    {
                        CEnigmaAnnouncement ann = routeNodes[node % routeNodes.size()];
                        CCloakingSecret secretThem;
                        cs->GetRoutingKey()->GetSecret(ann.pEnigmaSessionKeyHex, secretThem, true);
                        recipPubKeys.push_back(secretThem);
                    }

                    // encode the data packet and store it in onion packet. onion packet contains the previous onion layer.
                    // we've encoded for several nodes per hop which should help in cases where nodes drop connections as another node from the recipient
                    // list can unwrap the onion layer and forward the onion packet on.
                    // ENCRYPT_YES, TRANSMIT_YES, ONION_ROUTING_ENABLED
                    cloakData.EncodeEncryptTransmit(&onionpacket, CLOAK_DATA_RELAY_DATA, recipPubKeys, cs->GetRoutingKey(), ENCRYPT_PAYLOAD_YES, TRANSMIT_NO, ONION_ROUTING_NO);
                    onionpacket = cloakData; // overwrite original data with onion-layered data
                }

                // we now have a route (complete onion), store it for sending
                packetsToSend.push_back(onionpacket);
            }
        }else{
            // standard packet without onion routing
            packetsToSend.push_back(*this);
        }

        // Relay to nodes
        for(uint i=0; i<packetsToSend.size(); i++)
        {
            std::vector<CNode*> shuffledNodes;
            {
                LOCK(cs_vNodes);
                shuffledNodes = vNodes;
            }

            if (shuffle)
                std::random_shuffle(shuffledNodes.begin(), shuffledNodes.end(), GetRandInt);

            CCloakingData datapacket = packetsToSend[i];

            // mark as processed so we don't process the packet ourself
            //pwalletMain->sCloakDataHashes.insert(datapacket.GetHash());

            BOOST_FOREACH(CNode* pnode, shuffledNodes){
                if (sender == NULL || pnode != sender){
                    datapacket.RelayTo(pnode);
                }
            }
            datapacket.hops++;
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingData::RelayToAll: %s.", e.what());
    }
    return false;
}

bool CCloakingData::RelayTo(CNode* pnode, bool onionRoute)
{
    try
    {
        if(GetBoolArg("-printenigma", false))
            OutputDebugStringF("ENIGMA: CCloakingData::RelayTo node %s\n", pnode->addrName.c_str());

        switch(payload.dataType)
        {
            case CLOAK_DATA_REQ_ACCEPT:
                pnode->PushMessage(ENIMGA_STR_REQ_ACCEPT, *this);
                break;
            case CLOAK_DATA_SIGN_REQUEST:
                pnode->PushMessage(ENIMGA_STR_REQ_SIGN, *this);
                break;
            case CLOAK_DATA_SIGN_RESPONSE:
                pnode->PushMessage(ENIMGA_STR_REQ_SIGNED, *this);
                break;
            case CLOAK_DATA_RELAY_DATA:
                pnode->PushMessage(ENIMGA_STR_DATA, *this);
                break;
            case CLOAK_DATA_TX:
                pnode->PushMessage(ENIMGA_STR_ONION_TX, *this);
                break;
            case CLOAK_DATA_ENIGMAREQ:
                pnode->PushMessage(ENIMGA_STR_REQ, *this);
                break;
            case CLOAK_DATA_ENIGMA_MSG:
                pnode->PushMessage(ENIMGA_STR_MSG, *this);
                break;
            default:
                // unknown type - ignore
                printf("CCloakingData:RelayTo - unknown type %d.\n", payload.dataType);
                break;
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingData::RelayTo: %s.", e.what());
    }
    return false;
}

// already processed? if not, mark as processed before returning
bool CCloakingData::AlreadyProcessed()
{
    try
    {
        LOCK(pwalletMain->cs_CloakDataHashes);
        uint256 thisHash = this->GetSigningHash();
        bool exists = pwalletMain->sCloakDataHashes.find(thisHash) != pwalletMain->sCloakDataHashes.end();
        return exists;
    }catch (std::exception& e){
        error("CCloakingData::AlreadyProcessed: %s.", e.what());
    }
    return false;
}

bool CCloakingData::MarkProcessed(uint256 hash, bool processed)
{
    try
    {
        LOCK(pwalletMain->cs_CloakDataHashes);
        bool exists = pwalletMain->sCloakDataHashes.find(hash) != pwalletMain->sCloakDataHashes.end();

        if (processed && !exists){
            pwalletMain->sCloakDataHashes.insert(hash);
        }

        if (!processed && exists){
            pwalletMain->sCloakDataHashes.erase(hash);
        }
        return true;
    }catch (std::exception& e){
        error("CCloakingData::MarkProcessed: %s.", e.what());
    }
    return false;
}


// check if one of the public keys assigned to the data is ours
bool CCloakingData::CanDecrypt(const CCloakingEncryptionKey* myKey)
{
    try
    {
        LOCK(pwalletMain->cs_CloakingSecrets);
        return CCloakShield::GetShield()->mapCloakSecrets.count(myKey->pubkeyHex) && nVersion == ENIGMA_REQUEST_CURRENTVERSION;
    }catch (std::exception& e){
        error("CCloakingData::CanDecrypt: %s.", e.what());
    }
    return false;
}

// decrypt the cloaking data
bool CCloakingData::Decrypt(CCloakingEncryptionKey* myKey)
{
    try
    {
        if (nVersion != ENIGMA_REQUEST_CURRENTVERSION) {
            printf("ENIGMA: CCloakingData::Decrypt - wrong version.\n");
            return false; // cloaking data version too old or new
        }

        CCloakShield* cs = CCloakShield::GetShield();
        CCloakingSecret secret;

        if (this->recipientKeys.size() == 0) {
            // unencrypted
            if (payload.dataRaw.size() == 0 && payload.data.size() > 0){
                this->payload.dataRaw = this->payload.data;
                this->payload.data.clear();
            }
            return true;

        } else if (this->recipientKeys.size() == 1) {

            // single recipient. data was encrypted using key/iv derived from ECDH key exchange.
            if (this->recipientKeys[0].first.compare(myKey->pubkeyHexHash) == 0)
            {
                // encrypted just for us, use ECDH derived key/iv
                CCloakingSecret secret;
                myKey->GetSecret(this->senderPubKeyHex, secret, true);

                #if defined(QT_GUI) && ENIGMA_DEBUGMSG_ENCRYPTION

                        QString message = QString("---> Decrypting:\nkey them: %1\nkey us: %2\nkey: %3\niv: %4\nsecret: %4\n").arg(
                                    QString::fromStdString(secret.userPubKey),
                                    QString::fromStdString(cs->GetKey()->pubkeyHex),
                                    QString::fromStdString(secret.keyHex),
                                    QString::fromStdString(secret.ivHex),
                                    QString::fromStdString(secret.secretHex)
                                    );

                        printf(message.toStdString().c_str());
                #endif

                bool unpackedOk = cs->DecryptAES256(payload.data, payload.dataRaw, &secret.chKey[0], &secret.chIV[0]);

                // get data type from head of unpacked data buffer
                payload.dataType = payload.dataRaw[0];

                uint256 hash = Hash(&this->senderPubKeyHex[0], &this->senderPubKeyHex[this->senderPubKeyHex.size()],
                        &payload.dataRaw[0], (&payload.dataRaw[0])+payload.dataRaw.size());

                // strip the data type
                payload.dataRaw.erase(payload.dataRaw.begin(), payload.dataRaw.begin()+1);

                // check the rawdata/sender-pubkey hash matches the data id
                return unpackedOk && hash == this->identifier;
            }
        }else{
            // multiple recipients. data was encrypted with one-time key/iv and key was encrypted for
            // user using the key/iv  derived from ECDH key exchange.
            for(uint i=0; i<recipientKeys.size(); i++)
            {
                string pubkeyhash = recipientKeys[i].first;

                if (pubkeyhash.compare(myKey->pubkeyHexHash) == 0)
                {
                    CCloakingSecret secret;
                    myKey->GetSecret(this->senderPubKeyHex, secret, true);
                    // found our pubkey, decrypt the associated one-time symmetric key
                    vector<unsigned char> cryptedKey = ParseHex(recipientKeys[i].second);
                    vector<unsigned char> key;
                    bool unpackedOk = cs->DecryptAES256(cryptedKey, key, &secret.chKey[0], &secret.chIV[0]);

                    //printf("hex key for user %s = %s, %s\n", recipientKeys[i].first.c_str(), recipientKeys[i].second.c_str(), HexStr(stringFromVch(key)).c_str());

                    // now use the key to decrypt the data
                    unsigned char* chKey = &key[0];
                    unsigned char* chIv = &key[32];

                    unpackedOk = cs->DecryptAES256(payload.data, payload.dataRaw,  chKey, chIv);

                    // get data type from head of unpacked data buffer
                    payload.dataType = payload.dataRaw[0];

                    // strip the data type
                    payload.dataRaw.erase(payload.dataRaw.begin(), payload.dataRaw.begin()+1);

                    return unpackedOk;
                }
            }
        }
    }catch (std::exception &e)
    {
        printf("ENIGMA: CCloakingData::Decrypt failed: %s\n", e.what());
        return false;
    }
    return false;
}

/*
encrypt the cloaking data for one or more recipients.
If a single recipient is supplied, the shared key (generated using ECDH) is
used to encrypt the data so that only the recipient can read it.
If more than one recipient is supplied, the data is encrypted using a one-time
key and the key is encrypted using the ECDH key per recipient. This encrypted
key is then sent with the encrypted data.

todo:
add HMAC to avoid man-in-middle attacks.
*/
bool CCloakingData::Encrypt(vector<CCloakingSecret> recipientSecrets, const std::string myEnigmaPubkeyHex)
{
    try
    {
        //data = dataRaw;
        //return true;
        // clear previous keys
        this->recipientKeys.clear();

        CCloakingSecret secret;
        CCloakShield* cs = CCloakShield::GetShield();

        if (recipientSecrets.size() == 1){

            // encrypt for single recipient using ECDH derived key
            secret = recipientSecrets[0];
            this->recipientKeys.push_back(make_pair(secret.GetKeyHash(), ""));

        }else{

            // encrypt for group using one-time key.
            // we include an encrypted version of the one-time-key for each recipient, which is sent alongside the data.
            unsigned char chKey[32];
            unsigned char chIV[16];

            vector<unsigned char> vchOneTimeSecret = vchFromString(GetRandHash().ToString());

            // create the key and IV that we will use to encrypt the shared Enigma encryption key for this user.
            EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), NULL,
                              (unsigned char *)&vchOneTimeSecret[0], vchOneTimeSecret.size(), 25000, &chKey[0], &chIV[0]);

            // set key iv for data
            memcpy(&secret.chKey[0], chKey, 32);
            memcpy(&secret.chIV[0], chIV, 16);
            vector<unsigned char> vchKeyIv;

            vchKeyIv.insert(vchKeyIv.end(), &chKey[0], &chKey[32]);
            vchKeyIv.insert(vchKeyIv.end(), &chIV[0], &chIV[16]);

            // now encrypt the one-time key and IV for each recipient using the shared ECDH key we have for them
            for(uint i=0; i<recipientSecrets.size(); i++)
            {
                CCloakingSecret recipientSecret = recipientSecrets[i];
                vector<unsigned char> vchUserKey;
                if (cs->EncryptAES256(vchKeyIv, vchUserKey, recipientSecret.chKey, recipientSecret.chIV))
                {
                    string hexKeyIv = HexStr(vchUserKey.begin(), vchUserKey.end());
                    if(GetBoolArg("-printenigma", false))
                        OutputDebugStringF("hex key for user %s = %s, %s\n", recipientSecret.userPubKey.c_str(), HexStr(stringFromVch(vchUserKey)).c_str(), HexStr(stringFromVch(vchKeyIv)).c_str());
                    recipientKeys.push_back(make_pair(recipientSecret.GetKeyHash(), hexKeyIv));
                }
            }
        }

        // add the data type to the raw data for payload.
        // this ensures the object must be decrypted before the data type can be determined.
        payload.dataRaw.insert(payload.dataRaw.begin(), &payload.dataType, &payload.dataType+1);

        // the id is a hash of the raw data and the sender pubkey (hex)
        // this can be used by the reciever to verify the sender
        identifier = Hash(&myEnigmaPubkeyHex[0], &myEnigmaPubkeyHex[myEnigmaPubkeyHex.length()],
                &payload.dataRaw[0], (&payload.dataRaw[0])+payload.dataRaw.size());

#if defined(QT_GUI) && ENIGMA_DEBUGMSG_ENCRYPTION

        QString message = QString("---> Encrypting:\nkey them: %1\nkey us: %2\nkey: %3\niv: %4\nsecret: %4\n").arg(
                    QString::fromStdString(secret.userPubKey),
                    QString::fromStdString(cs->GetKey()->pubkeyHex),
                    QString::fromStdString(secret.keyHex),
                    QString::fromStdString(secret.ivHex),
                    QString::fromStdString(secret.secretHex)
                    );

        printf(message.toStdString().c_str());

        //QMessageBox::information(GuiRef, QString("Encrypting [%1]").arg(instanceNumber), "message");

        //QMessageBox::information(GuiRef, "hello", "hello");
#endif

        // encrypt using the one-time key (multiple recipients) or ECDH derived key (single recipient)
        bool encryptOk = cs->EncryptAES256(payload.dataRaw, payload.data, secret.chKey, secret.chIV);

        if (encryptOk){
            // the encrypted payload sits in [data], so we no longer need the raw (cleartext) data
            payload.dataRaw.clear();
        }
        return encryptOk;
    }
    catch (std::exception &e)
    {
        printf("ENIGMA: CCloakingData::Encrypt failed: %s\n", e.what());
        return false;
    }
}

bool CCloakingData::DoPow()
{
    return true;
}

bool CCloakingData::CheckPow()
{
    return true;
}

// create a compact signature (65 bytes), which allows reconstructing the used public key
// The format is one header byte, followed by two times 32 bytes for the serialized r and s values.
// The header byte: 0x1B = first key with even y, 0x1C = first key with odd y,
//                  0x1D = second key with even y, 0x1E = second key with odd y
bool CCloakingData::Sign(CCloakingEncryptionKey* myKey)
{
    uint256 signingHash = GetSigningHash();
    return myKey->Sign(signingHash, vchSig);
}

// decode from CCloakingData object
bool CCloakingData::Decode(void* objOut)
{
    try
    {
        this->SetDataTypeName();
        CDataStream ssData(payload.dataRaw, SER_NETWORK, PROTOCOL_VERSION);
        switch(payload.dataType)
        {
            case CLOAK_DATA_REQ_ACCEPT:
            {
                ssData >> *((CCloakingAcceptResponse*)objOut);
                break;
            }
            case CLOAK_DATA_ENIGMAREQ: // onion enigma request
            {
                ssData >> *((CCloakingRequest*)objOut);
                break;
            }
            case CLOAK_DATA_SIGN_REQUEST:
            {
                ssData >> *((CCloakingRequest*)objOut);
                break;
            }
            case CLOAK_DATA_SIGN_RESPONSE:
            {
                ssData >> *((CCloakingSignResponse*)objOut);
                break;
            }
            case CLOAK_DATA_RELAY_DATA: // onion layer
            {
                ssData >> *((CCloakingData*)objOut);
                break;
            }
            case CLOAK_DATA_TX: // onion tx
            {
                ssData >> *((CTransaction*)objOut);
                break;
            }
            case CLOAK_DATA_ENIGMA_MSG: // enigma message
            {
                ssData >> *((CCloakingMessage*)objOut);
                break;
            }
            default:
            {
                // unknown type - ignore
                printf("CCloakingData:Decode - unknown type %d.\n", payload.dataType);
            }
        }
    }
    catch (std::exception &e)
    {
        printf("ENIGMA: CCloakingData::Decode failed for type %d: %s\n", payload.dataType, e.what());
        return false;
    }
    return true;
}

bool CCloakingData::Encode(const void* data, enum CloakingDataType cloakDataType)
{
    return Encode(data, cloakDataType, NULL);
}

bool CCloakingData::Encode(const void* data, enum CloakingDataType cloakDataType, CCloakingEncryptionKey* myKey)
{
    try
    {
        payload.dataRaw.clear();
        CDataStream ssData(SER_NETWORK, PROTOCOL_VERSION);
        switch (cloakDataType)
        {
        case(CLOAK_DATA_REQ_ACCEPT): // enigma cloaking request accept
            ssData << *(CCloakingAcceptResponse*)data;
            break;

        case(CLOAK_DATA_SIGN_RESPONSE): // sign response from cloaker
            ssData << *(CCloakingSignResponse*)data;
            break;

        case(CLOAK_DATA_RELAY_DATA): // onion layer
            ssData << *(CCloakingData*)data;
            break;

        case(CLOAK_DATA_TX): // onion tx
            ssData << *(CTransaction*)data;
            break;

        case(CLOAK_DATA_SIGN_REQUEST): // sign request from sender
            // todo - slim down this message!
            ssData << *(CCloakingRequest*)data;
            break;

        case(CLOAK_DATA_ENIGMAREQ): // onion request
            ssData << *(CCloakingRequest*)data;
            break;

        case(CLOAK_DATA_ENIGMA_MSG): // onion request
            ssData << *(CCloakingMessage*)data;
            break;

        default:
            printf("EncodeAndTransmit - unknown type!\n");
            break;
        }

        // serialize the object into the cloakdata payload
        // we also populate the data holder as we may be sending the datapacket in the clear
        // and data needs to be populated prior to clear signing
        payload.data = payload.dataRaw = std::vector<unsigned char>(ssData.begin(), ssData.end());
        payload.dataType = cloakDataType;
        senderPubKeyHex = CCloakShield::GetShield()->GetRoutingKey()->pubkeyHex;

        // set data type name for debugging
        SetDataTypeName();

        // sign?
        if (myKey){
            this->Sign(myKey);
        }

    }catch(std::exception &e)
    {
        printf("ENIGMA: CCloakingData::Encode failed: %s\n", e.what());
        return false;
    }
    return true;
}

// encode and encrypt a cloaking data package for one or more recipients. the encrypted package can be wrapped inside an onion and routed if desired.
bool CCloakingData::EncodeEncryptTransmit(const void* data, enum CloakingDataType cloakDataType, vector<CCloakingSecret> recipients, CCloakingEncryptionKey* myKey, const bool encrypt, const bool transmit, const bool onionRoute)
{
    try
    {
        // encode the source object into a cloak data object, ready for encryption/
        // this also signs the clear data.
        Encode(data, cloakDataType, myKey);

        std::string pubkeyHex = myKey->pubkeyHex;

        // encrypt
        if (encrypt)
        {
            if (Encrypt(recipients, pubkeyHex)){
                senderPubKeyHex = pubkeyHex;
                this->Sign(myKey);
            }else{
                printf("EncodeAndTransmit: failed to encrypt.\n");
                return false;
            }
        }

        // transmit
        if (transmit){
            Transmit(true, onionRoute);
        }
        return true;
    }catch(std::exception &e)
    {
        printf("ENIGMA: CCloakingData::EncodeAndTransmit failed: %s\n", e.what());
        return false;
    }
}
