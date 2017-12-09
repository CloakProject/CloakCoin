// Copyright (c) 2014 The ShadowCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STEALTH_H
#define BITCOIN_STEALTH_H

#include "util.h"
#include "serialize.h"

#include <stdlib.h> 
#include <stdio.h> 
#include <vector>
#include <inttypes.h>


typedef std::vector<uint8_t> data_chunk;

const size_t ec_secret_size = 32;
const size_t ec_compressed_size = 33;
const size_t ec_uncompressed_size = 65;

typedef struct ec_secret { uint8_t e[ec_secret_size]; } ec_secret;
typedef data_chunk ec_point;

typedef uint32_t stealth_bitfield;

struct stealth_prefix
{
    uint8_t number_bits;
    stealth_bitfield bitfield;
};

template <typename T, typename Iterator>
T from_big_endian(Iterator in)
{
    //VERIFY_UNSIGNED(T);
    T out = 0;
    size_t i = sizeof(T);
    while (0 < i)
        out |= static_cast<T>(*in++) << (8 * --i);
    return out;
}

template <typename T, typename Iterator>
T from_little_endian(Iterator in)
{
    //VERIFY_UNSIGNED(T);
    T out = 0;
    size_t i = 0;
    while (i < sizeof(T))
        out |= static_cast<T>(*in++) << (8 * i++);
    return out;
}

class CStealthAddress
{
public:
    CStealthAddress()
    {
        options = 0;
        isEnigmaAddress = false;
        number_signatures = 0;

    }
    
    uint8_t options;
    ec_point scan_pubkey;
    ec_point spend_pubkey;
    //std::vector<ec_point> spend_pubkeys;
    size_t number_signatures;
    stealth_prefix prefix;
    
    mutable std::string label;
    data_chunk scan_secret;
    data_chunk spend_secret;

    bool isEnigmaAddress;
    
    bool SetEncoded(const std::string& encodedAddress);
    std::string Encoded() const;

    bool operator <(const CStealthAddress& y) const
    {
        return memcmp(&scan_pubkey[0], &y.scan_pubkey[0], ec_compressed_size) < 0;
    }

    uint256 GetEnigmaAddressPrivateHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << this->scan_pubkey;
        ss << this->spend_pubkey;
        ss << this->scan_secret;
        ss << this->spend_secret;

        return ss.GetHash();
    }

    // get private hash for this stealth address;
    uint256 GetEnigmaHash(uint256 addressHash, uint256 sourceHash, uint256 index) const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << addressHash;
        ss << sourceHash;
        ss << index;

        uint256 res = ss.GetHash();
        //printf("hash %s / source %s / index %s / got %s\n", addressHash.GetHex().c_str(), sourceHash.GetHex().c_str(), index.GetHex().c_str(), res.GetHex().c_str());
        return res;
    }

    // get private hash for this stealth address;
    uint256 GetEnigmaHash(uint256 sourceHash, uint256 index) const
    {
        uint256 addressHash = GetEnigmaAddressPrivateHash();
        return GetEnigmaHash(addressHash, sourceHash, index);
    }

    bool IsOwned() const
    {
        return scan_secret.size() == ec_secret_size;
    }
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->options);
        READWRITE(this->scan_pubkey);
        READWRITE(this->spend_pubkey);
        READWRITE(this->label);
        
        READWRITE(this->scan_secret);
        READWRITE(this->spend_secret);
    );
};

void AppendChecksum(data_chunk& data);

bool VerifyChecksum(const data_chunk& data);

bool GenerateEnigmaSecret(const CStealthAddress &stealthAddress, const uint256 &sourceHash, const uint256 &cloakerHash, const unsigned int &index, ec_secret &secret);
int GenerateRandomSecret(ec_secret& out);

int SecretToPublicKey(const ec_secret& secret, ec_point& out);

int StealthSecret(ec_secret& secret, const ec_point& pubkey, const ec_point& pkSpend, ec_secret& sharedSOut, ec_point& pkOut);
int StealthSecretSpend(ec_secret& scanSecret, ec_point& ephemPubkey, ec_secret& spendSecret, ec_secret& secretOut);
int StealthSharedToSecretSpend(ec_secret& sharedS, ec_secret& spendSecret, ec_secret& secretOut);

bool IsStealthAddress(const std::string& encodedAddress);


class SecMsgCrypter
{
private:
    unsigned char chKey[32];
    unsigned char chIV[16];
    bool fKeySet;
public:
    
    SecMsgCrypter()
    {
        // Try to keep the key data out of swap (and be a bit over-careful to keep the IV that we don't even use out of swap)
        // Note that this does nothing about suspend-to-disk (which will put all our key data on disk)
        // Note as well that at no point in this program is any attempt made to prevent stealing of keys by reading the memory of the running process.
        LockedPageManager::instance.LockRange(&chKey[0], sizeof chKey);
        LockedPageManager::instance.LockRange(&chIV[0], sizeof chIV);
        fKeySet = false;
    }
    
    ~SecMsgCrypter()
    {
        // clean key
        memset(&chKey, 0, sizeof chKey);
        memset(&chIV, 0, sizeof chIV);
        fKeySet = false;
        
        LockedPageManager::instance.UnlockRange(&chKey[0], sizeof chKey);
        LockedPageManager::instance.UnlockRange(&chIV[0], sizeof chIV);
    }
    
    bool SetKey(const std::vector<unsigned char>& vchNewKey, unsigned char* chNewIV);
    bool SetKey(const unsigned char* chNewKey, unsigned char* chNewIV);
    bool Encrypt(unsigned char* chPlaintext, uint32_t nPlain, std::vector<unsigned char> &vchCiphertext);
    bool Decrypt(unsigned char* chCiphertext, uint32_t nCipher, std::vector<unsigned char>& vchPlaintext);
};
#endif  // BITCOIN_STEALTH_H

