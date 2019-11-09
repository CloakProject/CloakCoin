#ifndef CONEMARKETCREATELISTING_H
#define CONEMARKETCREATELISTING_H

#include <set>
#include <string>

#include "uint256.h"
#include "util.h"
#include "sync.h"
#include "net.h"

class CNode;

class COneMarketCreateListing
{ 
public:
    int nVersion;
    std::string message;

    COneMarketCreateListing();
    ~COneMarketCreateListing();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        READWRITE(message);
    )

    bool RelayTo(CNode* pnode) const;
    void SetNull();
    void SetMessage(std::string);
};

#endif // CONEMARKETCREATELISTING_H
