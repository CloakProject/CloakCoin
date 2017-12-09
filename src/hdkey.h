#ifndef HDKEY_H
#define HDKEY_H

#include <stdexcept>
#include <vector>

#include "allocators.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"

#include <openssl/ec.h> // for EC_KEY definition

using namespace std;

extern void hmacSha512(unsigned char *out, const unsigned char *key, const unsigned int key_length, const unsigned char *text, const unsigned int text_length);

class HDSeed
{
public:
    HDSeed(const unsigned char* seed)
    {
        const unsigned char* seedval = (const unsigned char*)"426974636f696e2073656564";

        unsigned char a[64];
        hmacSha512(a, seedval, sizeof(seedval), seed, sizeof(seed));
        vector<unsigned char> hmac(a, a + sizeof(a)/sizeof(*a));
        master_key_.assign(hmac.begin(), hmac.begin() + 32);
        master_chain_code_.assign(hmac.begin() + 32, hmac.end());
    }

    const vector<unsigned char>& getSeed() const { return seed_; }
    const vector<unsigned char>& getMasterKey() const { return master_key_; }
    const vector<unsigned char>& getMasterChainCode() const { return master_chain_code_; }

private:
    vector<unsigned char> seed_;
    vector<unsigned char> master_key_;
    vector<unsigned char> master_chain_code_;
};

#endif // HDKEY_H
