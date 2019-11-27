// Copyright (c) 2014 The CloakCoin Developers
//
// Announcement system
//

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <map>

#include "util.h"
#include "enigmaann.h"
#include "key.h"
#include "net.h"
#include "ui_interface.h"

#include "cloakshield.h"
#include "encryption.h"

string nMyEnigmaAddress;

map<uint160, CEnigmaAnnouncement> mapEnigmaAnns;
CCriticalSection cs_mapEnigmaAnns;
map<uint256, CNode*> mapEnigmaAnnNodes;
CCriticalSection cs_EnigmaAcks;
set<uint256> sEnigmaCheckUpAcks;

void CEnigmaCheckUp::SetNull()
{
    nSentTime = 0;
    nHashCheck = 0;
}

void CEnigmaCheckUp::RelayTo(CNode* pnode) const
{
    // returns true if wasn't already contained in the set
    if(GetBoolArg("-printenigma", false))
        printf("ENIGMA: CEnigmaCheckUp::RelayTo node\n");

    // before relaying, remove the hash from the acks list if it is already there
    LOCK(cs_EnigmaAcks);
    if(sEnigmaCheckUpAcks.find(nHashCheck) != sEnigmaCheckUpAcks.end())
        sEnigmaCheckUpAcks.erase(nHashCheck);

    pnode->PushMessage(ENIMGA_STR_CHECK, *this);
}

void CEnigmaCheckUpAck::SetNull()
{
    nSentTime = 0;
    nInReplyToHash = 0;
}

void CEnigmaCheckUpAck::RelayTo(CNode* pnode) const
{
    // returns true if wasn't already contained in the set
    if(GetBoolArg("-printenigma", false))
        printf("ENIGMA: CEnigmaCheckUpAck::RelayTo node\n");
    pnode->PushMessage(ENIMGA_STR_CHECK_ACK, *this);
}

// ========== ENIGMA Announcement ==============

CEnigmaAnnouncement::CEnigmaAnnouncement()
{
    SetNull();
}

CEnigmaAnnouncement::CEnigmaAnnouncement(CCloakingEncryptionKey* key)
{
    SetNull();
    pEnigmaSessionKeyHex = key->pubkeyHex;
    GetKeyHash();
    timestamp = GetAdjustedTime(); // expires in 1 minutes
    Sign(CCloakShield::GetShield()->GetRoutingKey());
}

void CEnigmaAnnouncement::SetNull()
{
    nVersion = ENIGMA_REQUEST_CURRENTVERSION;
    timestamp = 0;
    hops = 0;
    maxHops = CLOAKSHIELD_MAX_HOPS;
    keyHash = uint160(0);
}


void CEnigmaAnnouncement::CullEnigmaAnns()
{
    TRY_LOCK(cs_mapEnigmaAnns, lockAnns);
    if (lockAnns)
    {
        // Periodically scan the list of Enigma nodes and remove expired ones
        std::set<uint160> toRemove;
        for (map<uint160, CEnigmaAnnouncement>::iterator mi = mapEnigmaAnns.begin(); mi != mapEnigmaAnns.end(); mi++)
        {
            const CEnigmaAnnouncement& ann = (*mi).second;
            if(!ann.IsInEffect())
            {
                // Remove it
                if(GetBoolArg("-printenigma"))
                    printf("Removing expired ENIGMA node: %s\n", ann.pEnigmaSessionKeyHex.c_str());

                toRemove.insert((*mi).first);
            }
        }

        if (toRemove.size() > 0 && GetBoolArg("-printenigma"))
            printf("Culling %d of %d Enimga announcements\n", toRemove.size(), mapEnigmaAnns.size());

        BOOST_FOREACH(const uint160 hash, toRemove)
        {
            map<uint160, CEnigmaAnnouncement>::iterator iter = mapEnigmaAnns.find(hash);
            if( iter != mapEnigmaAnns.end() )
                mapEnigmaAnns.erase( iter );
        }
    }
}

std::string CEnigmaAnnouncement::ToString() const
{
    return strprintf(
        "CEnigmaAnnouncement(\n"
        "    nVersion                = %d\n"
        "    nExpiration             = %" PRI64d "\n"
        "    pEnigmaSessionKeyHex    = \"%s\"\n"
        ")\n",
        nVersion,
        timestamp,
        pEnigmaSessionKeyHex.c_str());
}

void CEnigmaAnnouncement::print() const
{
    printf("%s", ToString().c_str());
}

uint160 CEnigmaAnnouncement::GetKeyHash()
{
    if (keyHash == 0){
        keyHash = Hash160(vchFromString(this->pEnigmaSessionKeyHex));
    }
    return keyHash;
}

uint256 CEnigmaAnnouncement::GetSigningHash() const
{
    // serialize everything except sig and hop count (as that's mutable)
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << this->nVersion;
    ss << this->timestamp;
    ss << this->pEnigmaSessionKeyHex;
    ss << this->maxHops;
    return ss.GetHash();
}

// create a compact signature (65 bytes), which allows reconstructing the used public key
// The format is one header byte, followed by two times 32 bytes for the serialized r and s values.
// The header byte: 0x1B = first key with even y, 0x1C = first key with odd y,
//                  0x1D = second key with even y, 0x1E = second key with odd y
bool CEnigmaAnnouncement::Sign(CCloakingEncryptionKey* myKey)
{
    return myKey->Sign(GetSigningHash(), vchSig);
}

bool CEnigmaAnnouncement::Authenticate()
{
    CKey key;
    if (!key.SetPubKey(ParseHex(pEnigmaSessionKeyHex)))
        return error("CEnigmaAnnouncement::Authenticate() : SetPubKey failed");
    if (!key.Verify(GetSigningHash(), vchSig))
        return error("CEnigmaAnnouncement::Authenticate() : verify signature failed");

    return true;
}

bool CEnigmaAnnouncement::IsInEffect() const
{
    // consider annoucement to still be valid until [CLOAKSHIELD_ANNOUNCE_TIMEOUT_SECS/2] seconds have passed since expiration
    // this takes into account the time taken for the annoucement to travel over the network...
    return (GetAdjustedTime() < (timestamp + CLOAKSHIELD_ANNOUNCE_TIMEOUT_SECS + nMaxObservedTimeOffset));
}

bool CEnigmaAnnouncement::IsMine() const
{
    // make sure the announcement isn't us
    return Hash(nMyEnigmaAddress.begin(), nMyEnigmaAddress.end()) == Hash(pEnigmaSessionKeyHex.begin(), pEnigmaSessionKeyHex.end());
}

bool CEnigmaAnnouncement::RelayTo(CNode* pnode, bool onionRoute) const
{
    if (!IsInEffect())
        return false;

    // returns true if wasn't already contained in the set
    if (pnode->setKnownAnns.insert(GetSigningHash()).second)
    {
        if(GetBoolArg("-printenigma"))
            printf("ENIGMA: CEnigmaAnnouncement::RelayTo node\n");
        pnode->PushMessage(ENIMGA_STR_ANNOUNCE, *this);
        return true;
    }
    return false;
}

CEnigmaAnnouncement CEnigmaAnnouncement::getAnnouncementByKeyHash(const uint160 &hash)
{
    CEnigmaAnnouncement retval;
    {
        LOCK(cs_mapEnigmaAnns);
        map<uint160, CEnigmaAnnouncement>::iterator mi = mapEnigmaAnns.find(hash);
        if (mi != mapEnigmaAnns.end())
            retval = mi->second;
    }
    return retval;
}

// elect [count] anonymizers (Enigma peers) with at least [nBalReq] available balance.
// use count=0 to return all active nodes
std::vector<CEnigmaAnnouncement> CEnigmaAnnouncement::electAnonymizers(const int count, const int64 nBalReq)
{
    std::vector<CEnigmaAnnouncement> enigmaAnnsAvail;

    LOCK(cs_mapEnigmaAnns);
    BOOST_FOREACH(PAIRTYPE(const uint160, CEnigmaAnnouncement)& item, mapEnigmaAnns)
    {
        const CEnigmaAnnouncement& ann = item.second;
        if (!ann.IsMine()/* && ann.nBalance >= nBalReq*/)
            enigmaAnnsAvail.push_back(ann);
    }

    if (enigmaAnnsAvail.size() > 0)
    {
        // shuffle
        std::random_shuffle(enigmaAnnsAvail.begin(), enigmaAnnsAvail.end(), GetRandInt);

        // crop to size
        if ((int)enigmaAnnsAvail.size()>count && count > 0)
            enigmaAnnsAvail.resize(count);
    }
    return enigmaAnnsAvail;
}

bool CEnigmaAnnouncement::ProcessAnnouncement(CNode* sender)
{
    if (hops >= maxHops)
        return false;

    if (nVersion != ENIGMA_REQUEST_CURRENTVERSION) // refuse to process if this is an old-version announcement!
        return false;

    if (!Authenticate())
    {
        if (sender){
            // node sent a junk announcement - penalize them!
            sender->Misbehaving(10);
        }
        return false;
    }

    if (!IsInEffect() || IsMine())
        return false;

    hops++;

    {
        uint160 keyHash = GetKeyHash();
        LOCK(cs_mapEnigmaAnns);
        mapEnigmaAnns[keyHash] = *this;
    }
    return true;
}

