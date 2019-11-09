#include "enigmapeer.h"
#include "enigma.h"

CEnigmaPeer::CEnigmaPeer(string pubKey)
{
    bannedUntil = 0;
    peerPubkey = pubKey;
}

bool CEnigmaPeer::isBanned()
{
    return GetAdjustedTime() <= bannedUntil;
}

CEnigmaPeer& CEnigmaPeerManager::GetNode(string nodePubKey)
{
    // NEED A LOCK HERE?
    if (nodes.find(nodePubKey) == nodes.end()){
        nodes.insert(std::make_pair(nodePubKey, CEnigmaPeer(nodePubKey)));
    }
    return nodes.find(nodePubKey)->second;
}

void CEnigmaPeerManager::SendMessageToNode(string nodePubKey, string message)
{
    LOCK(cs_mapEnigmaAnns);
    CCloakingData data;
    CCloakingMessage msg;
    data.Encode((void*)&message, CLOAK_DATA_ENIGMA_MSG, CCloakShield::GetShield()->GetRoutingKey());
    data.Transmit(true, ONION_ROUTING_ENABLED);
}

CEnigmaPeerManager::CEnigmaPeerManager()
{

}

bool CEnigmaPeerManager::IsEmigmaPeerBanned(string nodePubKey)
{
    LOCK(cs_mapEnigmaAnns);
    return GetNode(nodePubKey).isBanned();
}

int64 CEnigmaPeerManager::BanPeer(string nodePubKey, int64 bantimeSecs, string message)
{
    LOCK(cs_mapEnigmaAnns);
    CEnigmaPeer& node = GetNode(nodePubKey);
    node.bannedUntil = max(GetAdjustedTime() , node.bannedUntil) + bantimeSecs;

    if (message.length()>0 && GetBoolArg("-printenigma"))
        printf("BanPeer: %s - %s", nodePubKey.c_str(), message.c_str());

    return node.bannedUntil;
}

void CEnigmaPeerManager::ClearBan(string nodePubKey)
{
    LOCK(cs_mapEnigmaAnns);
    CEnigmaPeer& node = GetNode(nodePubKey);
    node.bannedUntil=0;
}

//SetEnigmaPeerMisbehaving
