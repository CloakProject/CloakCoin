#ifndef _CLOAKSHIELD_H_
#define _CLOAKSHIELD_H_ 1

#include <set>
#include <map>
#include <vector>
#include <string>
#include <boost/foreach.hpp>

#include "main.h"
#include "uint256.h"
#include "util.h"
#include "sync.h"
#include "lrucache.h"
#include <openssl/ec.h> // for EC_KEY definition

#include "cloakingdata.h"
#include "enigmapeer.h"
#include "encryption.h"

/*
TODO: switch to HMAC for verifying sender authenticity before open sourcing...

v1.0:
CloakData recipients should send a small receipt packet to acknowledge successful reciept.
Before Onion routing nodes should have responded to a checkup within a time limit.

*/

class CCloakingRequest;

class CCloakShield
{
public:

    static CCloakShield* CreateDefaultShield();
    static CCloakShield* GetShield();

    // cached secrets for peer encryption
    std::map<std::string, CCloakingSecret> mapCloakSecrets; // key = public key

    void SetNull();

    // todo: add method for recreating using an existing key
    CCloakShield();

    bool EncryptAES256(const std::vector<unsigned char> data, std::vector<unsigned char>& dataOut, const unsigned char* key, const unsigned char* iv);
    bool DecryptAES256(const std::vector<unsigned char> data, std::vector<unsigned char>& dataOut, const unsigned char* key, const unsigned char* iv);

    bool IsEnigmaPeerBanned(const std::string nodePubKey);
    int64 BanPeer(const std::string nodePubKey, const int64 banTimeSecs, const std::string message = "");
    void ClearBan(const std::string nodePubKey);

    void SendMessageToNode(const std::string nodePubKey, const std::string message);

    bool OnionRoutingAvailable(bool showMessage = true);

    bool OnionRouteTx(CTransaction tx);
    bool SendEnigmaRequest(CCloakingRequest* req);

    CCloakingEncryptionKey* GetKey();
    CCloakingEncryptionKey* GetRoutingKey();

    int NumNodesRequired();
    int NumRoutesRequired();
    int NumHopsRequired();

private:
    static CCloakShield* defaultShield;
    CCloakingEncryptionKey* currentEnigmaKey;
    CCloakingEncryptionKey* currentRoutingKey;
    CEnigmaPeerManager* peerManager;

    bool ProcessAES256(const std::vector<unsigned char> data, std::vector<unsigned char>& dataOut,
                              bool encrypt, const unsigned char* key, const unsigned char* iv);


};

#endif
