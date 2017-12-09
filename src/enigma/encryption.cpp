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
#include "encryption.h"

extern bool lightCloakShieldIcon ; // should we light the cloak shield icon on the main gui status area?

void CCloakingSecret::SetNull()
{
    memset(&chKey, 0, sizeof chKey);
    memset(&chIV, 0, sizeof chIV);
    userPubKeyForHash = "";
    userPubKeyHashHex = "";
}

const std::string CCloakingSecret::GetKeyHash()
{
    // need to regen hash ?
    if (userPubKeyForHash.compare(userPubKey) != 0){
        userPubKeyHashHex = Hash160(vchFromString(userPubKey)).GetHex();
    }
    return userPubKeyHashHex;
}

// CCloakingEncryptionKey is our per-session pub/priv keypair for ECDH communication with peers
CCloakingEncryptionKey::CCloakingEncryptionKey() : CKey()
{
    try
    {
        MakeNewKey(true);
        pubkeyHex = HexStr(this->GetPubKey().Raw());
        pubkeyHexHash = Hash160(vchFromString(pubkeyHex)).GetHex();

    }catch (std::exception& e){
        error("CCloakingEncryptionKey::CCloakingEncryptionKey: %s.", e.what());
    }
}


// create (or return existing) hex encoded ECDH secret for a public key (using our current Enigma priv-key)
bool CCloakingEncryptionKey::GetSecret(std::string targetPubKey, CCloakingSecret& cloakSecretOut, bool addToMap)
{
    try
    {
        LOCK(pwalletMain->cs_CloakingSecrets);
        CCloakShield* cs = CCloakShield::GetShield();

        // return existing?
        if (cs->mapCloakSecrets.count(targetPubKey)){
            cloakSecretOut = cs->mapCloakSecrets[targetPubKey];
            return true;
        }

        EC_KEY *peerkey;
        int field_size;
        unsigned char *secret;
        size_t secret_len;

        std::vector<unsigned char> vchPubKey = ParseHex(targetPubKey.c_str());
        const unsigned char* pbegin = &vchPubKey[0];

        peerkey = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (o2i_ECPublicKey(&peerkey, &pbegin, vchPubKey.size()))
        {
            // In testing, d2i_ECPrivateKey can return true
            // but fill in pkey with a key that fails
            // EC_KEY_check_key, so:
            if (!EC_KEY_check_key(peerkey))
            {
                printf("CreateSecretECDH: EC_KEY_check_key failed");
                return false;
            }
        }

        // Calculate the size of the buffer for the shared secret
        field_size = EC_GROUP_get_degree(EC_KEY_get0_group(this->pkey));
        secret_len = (field_size+7)/8;

        // Allocate the memory for the shared secret
        if(NULL == (secret = (unsigned char*)OPENSSL_malloc(secret_len)))
        {
            printf("CreateSecretECDH: OPENSSL_malloc failed");
        }

        // Derive the shared secret
        secret_len = ECDH_compute_key(secret, secret_len, EC_KEY_get0_public_key(peerkey),
                            this->pkey, NULL);

        // Clean up
        EC_KEY_free(peerkey);

        if(secret_len <= 0)
        {
            if(GetBoolArg("-printenigma", false))
                OutputDebugStringF("CreateSecretECDH: secret_len <= 0");
            OPENSSL_free(secret);
            return NULL;
        }

        vector<unsigned char> vchSecret;
        vchSecret.insert( vchSecret.end(), secret, secret+(secret_len));

        vector<unsigned char> vchSecretHash;
        uint256 secretHash = Hash(vchSecret.begin(), vchSecret.end());
        vchSecretHash.insert( vchSecretHash.end(), secretHash.begin(), secretHash.end());

        // create the key and IV that we will use to encrypt the shared Enigma encryption key for this user.
        EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), NULL,
                          (unsigned char *)&vchSecretHash[0], vchSecretHash.size(), 25000, &cloakSecretOut.chKey[0], &cloakSecretOut.chIV[0]);

        cloakSecretOut.secretHex = HexStr(vchSecretHash.begin(), vchSecretHash.end());

        cloakSecretOut.keyHex = HexStr(&cloakSecretOut.chKey[0], &cloakSecretOut.chKey[32]);
        cloakSecretOut.ivHex = HexStr(&cloakSecretOut.chIV[0], &cloakSecretOut.chIV[16]);

        cloakSecretOut.userPubKey = targetPubKey;

        if (addToMap)
            cs->mapCloakSecrets[targetPubKey] = cloakSecretOut;

        return true;

    }catch (std::exception& e){
        error("CCloakingEncryptionKey::GetSecretECDH: %s.", e.what());
        return false;
    }
}
