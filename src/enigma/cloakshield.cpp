/*
TODO: check pubkey from enigma node is valid before trying to generate ECDH shared secret. this could
be a potential attack vector used to crash clients by sending junk pubkeys to other nodes.
*/

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <map>
#include "enigma.h"
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

#include "cloakshield.h"

CCloakShield* CCloakShield::defaultShield;

bool lightCloakShieldIcon = false; // should we light the cloak shield icon on the main gui status area?
bool fOnionRouteAll = false; // should we onion route all transactions, including non enigma ones? (for now we also send by standard means until we have onion reciepts)
int nCloakShieldNumNodes = 3; // number of nodes used per onion route
int nCloakShieldNumHops = 3; // number of hops/jumps on an onion route
int nCloakShieldNumRoutes = 3; // number of simultaneous routes that an onion packet will be routed across

std::map<std::string, int> peerDosCounts; // list of DoS scores for current peers. used to tally DoS scores for banning. We should probably unban/reduce periodically, as opposed to waiting for a client restart to reset counts.


// ***********************************************************************************************************************************

CCloakShield* CCloakShield::CreateDefaultShield()
{
    defaultShield = new CCloakShield();
    return defaultShield;
}

CCloakShield::CCloakShield()
{
    // todo: add method for recreating using an existing key
    SetNull();
    currentEnigmaKey = new CCloakingEncryptionKey();
    currentRoutingKey = new CCloakingEncryptionKey();
    peerManager = new CEnigmaPeerManager();
}

CCloakShield* CCloakShield::GetShield()
{
    return defaultShield;
}

void CCloakShield::SetNull()
{
}

bool CCloakShield::IsEnigmaPeerBanned(const std::string nodePubKey)
{
    return peerManager->IsEmigmaPeerBanned(nodePubKey);
}

int64 CCloakShield::BanPeer(const std::string nodePubKey, const int64 banTimeSecs, const std::string message)
{
    return peerManager->BanPeer(nodePubKey, banTimeSecs, message);
}

void CCloakShield::ClearBan(const std::string nodePubKey)
{
    peerManager->ClearBan(nodePubKey);
}

bool CCloakShield::OnionRoutingAvailable(bool showMessage)
{
    try
    {
        int minNodesNeeded = this->NumNodesRequired();

        // do we have [required nodes+CLOAK_SHIELD_MIN_NODE_BUFFER] available?
        int countEnigmaNodes =  CEnigmaAnnouncement::electAnonymizers(0).size();

        if (countEnigmaNodes < minNodesNeeded)
        {
            if (showMessage && (ONION_ROUTING_ENABLED && fEnableOnionRouting))
            {
                string strMessage = strprintf(_("Not enough Enigma nodes to Onion Route with CloakShield. Need %d, have %d."), minNodesNeeded, countEnigmaNodes);
                uiInterface.ThreadSafeMessageBox(strMessage, "CloakCoin", CClientUIInterface::OK | CClientUIInterface::ICON_STOP | CClientUIInterface::MODAL);
            }
            return false;
        }
        return true;
    }catch(std::exception &e)
    {
        printf("ENIGMA: CCloakShield::OnionRoutingAvailable failed: %s\n", e.what());
        return false;
    }

}

// onion route a transaction (enigma or standard) using cloak shield
bool CCloakShield::SendEnigmaRequest(CCloakingRequest* req)
{
    try
    {
        // encode request in cloakdata
        CCloakingData cd;

        if (!cd.Encode((void*)req, CLOAK_DATA_ENIGMAREQ, CCloakShield::defaultShield->GetRoutingKey()))
            return false;

        return cd.Transmit(true, (ONION_ROUTING_ENABLED && fEnableOnionRouting));
    }catch(std::exception &e)
    {
        printf("ENIGMA: CCloakShield::OnionRouteEnigmaRequest failed: %s\n", e.what());
        return false;
    }
}

CCloakingEncryptionKey* CCloakShield::GetKey()
{
    return currentEnigmaKey;
}

CCloakingEncryptionKey* CCloakShield::GetRoutingKey()
{
    return currentRoutingKey;
}

int CCloakShield::NumNodesRequired()
{
    int res =  GetBoolArg("-testnet", false) ? CLOAKSHIELD_NUM_NODES_TEST : nCloakShieldNumNodes + CLOAK_SHIELD_MIN_NODE_BUFFER;
    return res;
}

int CCloakShield::NumRoutesRequired()
{
    return GetBoolArg("-testnet", false) ? CLOAKSHIELD_NUM_ROUTES_TEST : nCloakShieldNumRoutes;
}

int CCloakShield::NumHopsRequired()
{
    return GetBoolArg("-testnet", false) ? CLOAKSHIELD_NUM_HOPS_TEST : nCloakShieldNumHops;
}

void CCloakShield::SendMessageToNode(const std::string nodePubKey, const std::string message)
{
    peerManager->SendMessageToNode(nodePubKey, message);
}

// onion route a transaction (enigma or standard) using cloak shield
bool CCloakShield::OnionRouteTx(CTransaction tx)
{
    try
    {
        // todo: when onion routing queue system and read reciepts are in place
        // we should not need to fire the tx by standard means. this is done for
        // now as onion routing is not currently failsafe and it's important that
        // the tx gets relayed.

        // relay the tx
        uint256 hash = tx.GetHash();
        RelayMessage(CInv(MSG_TX, hash), tx);

        // and attempt to onion route too?
        if (ONION_ROUTING_ENABLED)
        {
            CCloakingData cd;
            if (!cd.Encode((void*)&tx, CLOAK_DATA_TX), CCloakShield::defaultShield->GetRoutingKey())
                return false;

            cd.Transmit(NULL, true, fOnionRouteAll);
        }
        return true;

    }catch(std::exception &e)
    {
        printf("ENIGMA: CCloakShield::OnionRouteTx failed: %s\n", e.what());
        return false;
    }
}

bool CCloakShield::EncryptAES256(const std::vector<unsigned char> data, std::vector<unsigned char>& dataOut, const unsigned char* key, const unsigned char* iv)
{
    return ProcessAES256(data, dataOut, true, key, iv);
}

bool CCloakShield::DecryptAES256(const std::vector<unsigned char> data, std::vector<unsigned char>& dataOut, const unsigned char* key, const unsigned char* iv)
{
    return ProcessAES256(data, dataOut, false, key, iv);
}

// return AES encrypted version of data
bool CCloakShield::ProcessAES256(const std::vector<unsigned char> data, std::vector<unsigned char>& dataOut,
                   bool encrypt, const unsigned char* key, const unsigned char* iv)
{
    try
    {
        if (data.size() == 0)
        {
            if(GetBoolArg("-printenigma", false))
                OutputDebugStringF("**ERROR** EncryptAES256 called with no data!\n");
        }
        bool fOk = true;

        // max ciphertext len for a n bytes of plaintext is
        // n + AES_BLOCK_SIZE - 1 bytes
        int nLen = data.size();
        int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
        dataOut = std::vector<unsigned char> (nCLen);

        EVP_CIPHER_CTX ctx;
        EVP_CIPHER_CTX_init(&ctx);

        vector<unsigned char> vchKey;
        vchKey.insert( vchKey.end(), &key[0], &key[32]);

        vector<unsigned char> vchIv;
        vchIv.insert( vchIv.end(), &iv[0], &iv[16]);

        if (encrypt)
        {
            //OutputDebugStringF("encrypt with key %s, IV %s\n", HexStr(vchKey).c_str(), HexStr(vchIv).c_str());
            if (fOk) fOk = EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, key, iv);
            if (fOk) fOk = EVP_EncryptUpdate(&ctx, &dataOut[0], &nCLen, &data[0], nLen);
            if (fOk) fOk = EVP_EncryptFinal_ex(&ctx, (&dataOut[0])+nCLen, &nFLen);
        }else{
            //OutputDebugStringF("decrypt with key %s, IV %s\n", HexStr(vchKey).c_str(), HexStr(vchIv).c_str());
            if (fOk) fOk = EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, key, iv);
            if (fOk) fOk = EVP_DecryptUpdate(&ctx, &dataOut[0], &nCLen, &data[0], nLen);
            if (fOk) fOk = EVP_DecryptFinal_ex(&ctx, (&dataOut[0])+nCLen, &nFLen);
        }
        EVP_CIPHER_CTX_cleanup(&ctx);

        if (!fOk) return false;

        dataOut.resize(nCLen + nFLen);

        return true;
    }catch(std::exception &e)
    {
        printf("ENIGMA: CCloakShield::EncryptAES256 failed: %s\n", e.what());
        return false;
    }
}

