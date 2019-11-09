#include "conemarketcreatelisting.h"

COneMarketCreateListing::COneMarketCreateListing()
{
    SetNull();
    printf("OneMarket: COneMarketCreateListing::init\n");
}

COneMarketCreateListing::~COneMarketCreateListing(){

}

bool COneMarketCreateListing::RelayTo(CNode* pnode) const
{
    printf("OneMarket: COneMarketCreateListing::RelayTo node\n");
    pnode->PushMessage("createlisting", *this);
    return true;
}

void COneMarketCreateListing::SetNull()
{
    message.clear();
}

void COneMarketCreateListing::SetMessage(std::string theMessage)
{
    printf("OneMarket: COneMarketCreateListing Set Message=%s",theMessage.c_str());
    message = theMessage;
}
