#ifndef CLOAKNODE_H
#define CLOAKNODE_H

#include <set>
#include <map>
#include <vector>
#include <string>
#include <boost/foreach.hpp>

#include "uint256.h"
#include "../serialize.h"
#include "../sync.h"

using std::string;

class CEnigmaPeer
{
public:
    CEnigmaPeer(string pubkey);
    
    string peerPubkey;
    string secret;
    bool onionRoutingEnabled;
    
    bool isBanned();
    bool canOnionRoute();
    int64 bannedUntil;

private:    


};

class CEnigmaPeerManager
{
public:
    CEnigmaPeerManager();
    bool IsEmigmaPeerBanned(string nodePubKey);
    void SendMessageToNode(string nodePubKey, string message);
    int64 BanPeer(string nodePubKey, int64 bantimeSecs, string message);
    void ClearBan(string nodePubKey);

private:    
    CCriticalSection cs_enigmaPeers;
    std::map<std::string, CEnigmaPeer> nodes;

    CEnigmaPeer& GetNode(string nodePubKey);
};

#endif // CLOAKNODE_H
