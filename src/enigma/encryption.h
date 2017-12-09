#ifndef ENCRYPTION_H
#define ENCRYPTION_H 1

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
#include "key.h"

#include "cloakingdata.h"
#include "enigmapeer.h"

// Shared secret key (derived using ECDH), used for encrypting data securely between nodes.
class CCloakingSecret
{
public:
    std::string userPubKey; // pub key of participant or other user
    //std::string hexSecret; // hex encoded shared secret

    // key and IV used for encrypting the key for this particular pub key.
    // the reciever can then decrypt the key using the shared key derived from
    // the ECDH shared secret.
    unsigned char chKey[32];
    unsigned char chIV[16];

    std::string secretHex; // secret as hex
    std::string keyHex; // secret as hex
    std::string ivHex; // secret as hex

    void SetNull();
    const std::string GetKeyHash();

    CCloakingSecret()
    {
        SetNull();
    }

protected:
    std::string userPubKeyHashHex;
    std::string userPubKeyForHash;
};

// CKey (our PubKey & PrivKey) specifically used for Enigma ECDH. This is regenerated at runtime.
class CCloakingEncryptionKey : public CKey
{
public:
    std::string pubkeyHex; // our pubkey (raw/hex)
    std::string pubkeyHexHash; // our pubkey hash (raw/hex)

    CCloakingEncryptionKey();
    bool GetSecret(std::string targetPubKey, CCloakingSecret& cloakSecretOut, bool addToMap = true);
};

#endif // ENCRYPTION_H
