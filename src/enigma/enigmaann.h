// Copyright (c) 2014 The CloakCoin Developers
// Copyright (c) 2013-2014 The FedoraCoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _MIXERANN_H_
#define _MIXERANN_H_ 1

#include <set>
#include <string>

#include "uint256.h"
#include "util.h"
#include "sync.h"

using namespace std;

class CNode;
class CCloakingEncryptionKey;

//=========== ENIGMA CheckUp ==============
class CEnigmaCheckUp
{
public:
	int64 nSentTime;
	uint256 nHashCheck;

    CEnigmaCheckUp()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (        
        READWRITE(this->nSentTime);
        READWRITE(this->nHashCheck);
    )

    void SetNull();
    void RelayTo(CNode* pnode) const;
};

//=========== ENIGMA CheckUpACK ===========
class CEnigmaCheckUpAck
{
public:
	int64 nSentTime;
        uint256 nInReplyToHash;

    CEnigmaCheckUpAck()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (        
        READWRITE(this->nSentTime);
        READWRITE(this->nInReplyToHash);
    )

    void SetNull();
    void RelayTo(CNode* pnode) const;
};

//=========== ENIGMA Announcement===============
class CEnigmaAnnouncement
{
private:
    uint160 keyHash;
public:
    int nVersion;
    int64 timestamp;
    std::string pEnigmaSessionKeyHex;
    std::vector<unsigned char> vchSig;
    unsigned short maxHops;
    unsigned short hops;

    //std::vector<unsigned char> nick;

    // memory only

    CEnigmaAnnouncement(CCloakingEncryptionKey* key);
    CEnigmaAnnouncement();

    IMPLEMENT_SERIALIZE
    (        
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(timestamp);
        READWRITE(pEnigmaSessionKeyHex);
        READWRITE(vchSig);
        READWRITE(maxHops);
        READWRITE(hops);

        /*
        if (nVersion >= 8){
            READWRITE(nick);
        }
        */
    )

    std::string ToString() const;
    void print() const;

    void SetNull();
    uint256 GetSigningHash() const;
    uint160 GetKeyHash();
    bool Authenticate();
    bool Sign(CCloakingEncryptionKey* myKey);
    bool IsInEffect() const;
    bool RelayTo(CNode* pnode, bool onionRoute = true) const;
    bool CheckSignature() const;
    bool ProcessAnnouncement(CNode* sender);
    bool IsMine() const;

    /*
     * Get copy of (active) announcement object by hash. Returns a null alert if it is not found.
     */
    static CEnigmaAnnouncement getAnnouncementByKeyHash(const uint160 &hash);

    // Elect an anonymizer by balance
    static std::vector<CEnigmaAnnouncement> electAnonymizers(const int count, const int64 nBalReq=0);
    static void CullEnigmaAnns();
};

#endif
