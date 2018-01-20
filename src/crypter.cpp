// Copyright (c) 2009-2012 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compat.h"
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <vector>
#include <string>
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "crypter.h"

bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
                          (unsigned char *)&strKeyData[0], strKeyData.size(), nRounds, chKey, chIV);

    if (i != (int)WALLET_CRYPTO_KEY_SIZE)
    {
        memset(&chKey, 0, sizeof chKey);
        memset(&chIV, 0, sizeof chIV);
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetDataKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
                          (unsigned char *)&strKeyData[0], strKeyData.size(), nRounds, chDataKey, chDataIV);

    if (i != (int)WALLET_CRYPTO_KEY_SIZE)
    {
        memset(&chDataKey, 0, sizeof chDataKey);
        memset(&chDataIV, 0, sizeof chDataIV);
        return false;
    }

    fDataKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_KEY_SIZE)
        return false;

    memcpy(&chKey[0], &chNewKey[0], sizeof chKey);
    memcpy(&chIV[0], &chNewIV[0], sizeof chIV);

    fKeySet = true;
    return true;
}

bool CCrypter::EncryptWalletFile(FILE *ifp, FILE *ofp)
{
    boost::filesystem::path inputPath = GetDataDir() / "wallet.dat";
    boost::filesystem::path outputPath = GetDataDir() / "wallet.dat.encrypted";

    ifp = fopen(inputPath.string().c_str(), "rb");
    ofp = fopen(outputPath.string().c_str(), "wb");

    if ( NULL == ifp )
    {
        return false;
    }

    if ( NULL == ofp )
    {
        return false;
    }

    if (!fDataKeySet){
        fclose(ifp);
        fclose(ofp);
        return false;
    }

    //Get file size
    fseek(ifp, 0L, SEEK_END);
    int fsize = ftell(ifp);
    //set back to normal
    fseek(ifp, 0L, SEEK_SET);

    int outLen1 = fsize + AES_BLOCK_SIZE; int outLen2 = 0;
    unsigned char *indata = (unsigned char*)malloc(fsize);
    unsigned char *outdata = (unsigned char*)malloc(fsize*2);

    //Read File
    fread(indata, sizeof(char), fsize, ifp);//Read Entire File

    //Set up encryption
    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chDataKey, chDataIV);
    if (fOk) fOk = EVP_EncryptUpdate(&ctx, outdata, &outLen1, indata, fsize);
    if (fOk) fOk = EVP_EncryptFinal_ex(&ctx, outdata + outLen1, &outLen2);
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    fwrite(outdata, sizeof(char), outLen1 + outLen2, ofp);
    fclose(ifp);
    fclose(ofp);
    return true;
}

bool CCrypter::DecryptWalletFile(FILE *ifp, FILE *ofp)
{
    if (!fDataKeySet) {
        fclose(ifp);
        fclose(ofp);
        return false;
    }

    //Get file size
    fseek(ifp, 0L, SEEK_END);
    int fsize = ftell(ifp);
    //set back to normal
    fseek(ifp, 0L, SEEK_SET);

    // plaintext will always be equal to or lesser than length of ciphertext
    int outLen1 = fsize, outLen2 = 0;

    unsigned char *indata = (unsigned char*)malloc(fsize);
    //plaintext file size will always be equal to or lesser than the encrypted file
    unsigned char *outdata = (unsigned char*)malloc(fsize);

    //Read File
    fread(indata, sizeof(char), fsize, ifp);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chDataKey, chDataIV);
    if (fOk) fOk = EVP_DecryptUpdate(&ctx, outdata, &outLen1, indata, fsize);
    if (fOk) fOk = EVP_DecryptFinal_ex(&ctx, outdata + outLen1, &outLen2);
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    fwrite(outdata, sizeof(char), outLen1 + outLen2, ofp);
    fclose(ifp);
    fclose(ofp);
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext)
{
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char> (nCLen);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV);
    if (fOk) fOk = EVP_EncryptUpdate(&ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(&ctx, (&vchCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext)
{
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    vchPlaintext = CKeyingMaterial(nPLen);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV);
    if (fOk) fOk = EVP_DecryptUpdate(&ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(&ctx, (&vchPlaintext[0])+nPLen, &nFLen);
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    vchPlaintext.resize(nPLen + nFLen);
    return true;
}

bool EncryptSecret(CKeyingMaterial& vMasterKey, const CSecret &vchPlaintext, const uint256& nIV, std::vector<unsigned char> &vchCiphertext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt((CKeyingMaterial)vchPlaintext, vchCiphertext);
}

bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CSecret& vchPlaintext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, *((CKeyingMaterial*)&vchPlaintext));
}
