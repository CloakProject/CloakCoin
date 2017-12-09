#include "enigmatablemodel.h"

#include "guiutil.h"
#include "walletmodel.h"
#include "guiconstants.h"
#include "wallet.h"
#include "base58.h"
#include "enigma/enigma.h"

#include <QFont>
#include <QTimer>
#include <QIcon>

using namespace std;

struct EnigmaTableEntryLessThan
{
    bool operator()(const EnigmaTableEntry &a, const EnigmaTableEntry &b) const
    {
        return a.initiatedon < b.initiatedon;
    }
    bool operator()(const EnigmaTableEntry &a, const int64 &b) const
    {
        return a.initiatedon < b;
    }
    bool operator()(const int64 &a, const EnigmaTableEntry &b) const
    {
        return a < b.initiatedon;
    }
};

struct EnigmaTableEntryGreaterThan
{
    bool operator()(const EnigmaTableEntry &a, const EnigmaTableEntry &b) const
    {
        return a.initiatedon > b.initiatedon;
    }
    bool operator()(const EnigmaTableEntry &a, const int64 &b) const
    {
        return a.initiatedon > b;
    }
    bool operator()(const int64 &a, const EnigmaTableEntry &b) const
    {
        return a > b.initiatedon;
    }
};

#define NAMEMAPTYPE map<vector<unsigned char>, uint256>

// Private implementation
class EnigmaTablePriv
{
public:
    CWallet *wallet;
    QList<EnigmaTableEntry> cachedEnigmaTable;
    EnigmaTableModel *parent;
    bool doSort;

    EnigmaTablePriv(CWallet *wallet, EnigmaTableModel *parent): wallet(wallet), parent(parent)
    {
        doSort = true;
    }

    /*
    void updateEntry(PosaTableEntry entry)
    {
        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                OutputDebugStringF("Warning: AddressTablePriv::updateEntry: Got CT_NOW, but entry is already in model\n");
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedAddressTable.insert(lowerIndex, AddressTableEntry(newEntryType, label, address));
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if(!inModel)
            {
                OutputDebugStringF("Warning: AddressTablePriv::updateEntry: Got CT_UPDATED, but entry is not in model\n");
                break;
            }
            lower->type = newEntryType;
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if(!inModel)
            {
                OutputDebugStringF("Warning: AddressTablePriv::updateEntry: Got CT_DELETED, but entry is not in model\n");
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedAddressTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }
    */

    void refreshEnigmaTable(bool force = false)
    {
        if (force)
        {
            parent->beginResetModel();
            cachedEnigmaTable.clear();
        }

        {			
            // populate
/*
            cachedPosaTable.append(PosaTableEntry(PosaTableEntry::MyCloaking,
                                                  QString::fromStdString(strprintf("%lf", 0)),
                                                  QString::fromStdString(strprintf("%lf", 0)),
                                                  QString::fromStdString(strprintf("%lf", 0)),
                                                  QString::fromStdString(strprintf("%lf", 0)),
                                                  QString::fromStdString(strprintf("%lf", 0)),
                                                  QString::fromStdString(strprintf("%lf", 0))
                                                  ));
*/
            typedef std::map<uint256, CCloakingRequest> CloakingMapType;

            vector<EnigmaTableEntry> entries;

            // todo: fix - this causes considerable slowdown!
            map<uint256, CCloakingRequest> mapOurReqs;
            LOCK(pwalletMain->cs_mapOurCloakingRequests);
            {                
                mapOurReqs.insert(pwalletMain->mapOurCloakingRequests.begin(), pwalletMain->mapOurCloakingRequests.end()); // key = CCloakingIdentifier hash
            }

            BOOST_FOREACH(CloakingMapType::value_type p, mapOurReqs)
            {
                CCloakingRequest r = p.second;
                uint256 idhash = r.identifier.GetHash();
                //bool isMine = mapOurReqs.find(idhash) != mapOurReqs.end();
                //int totalParts = r.nParticipantsRequired+1;


                EnigmaTableEntry entry(idhash, EnigmaTableEntry::MyCloaking,
                                     r.identifier.nInitiatedTime,
                                     r.nSendAmount,
                                     r.nParticipantsRequired+1,
                                     r.sParticipants.size()+1,
                                     r.signedByAddresses.size(),
                                     r.TimeLeftMs(),
                                     uint256(r.txid),
                                     r.autoRetry,
                                     r.retryCount,
                                     r.aborted
                                     );

                entries.push_back(entry);

                //if (!cachedPosaTable.contains(entry))
               //     cachedPosaTable.prepend(entry);
            }


            // sort our pending list
            qSort(entries.begin(), entries.end(), EnigmaTableEntryGreaterThan());

            for(int i=0; i<(int)entries.size(); i++)
            {
                EnigmaTableEntry entry = entries[i];

                // Find in model
                //QList<PosaTableEntry>::iterator lower = qLowerBound(
                //    cachedPosaTable.begin(), cachedPosaTable.end(), entry, PosaTableEntryLessThan());

                //QList<PosaTableEntry>::iterator upper = qUpperBound(
                //    cachedPosaTable.begin(), cachedPosaTable.end(), entry, PosaTableEntryLessThan());

                //int lowerIndex = (lower - cachedPosaTable.begin());
                //int upperIndex = (upper - cachedPosaTable.begin());
                //bool inModel = (lower != upper);

                bool found = false;
                for(int i=0; i<cachedEnigmaTable.length(); i++)
                {
                    EnigmaTableEntry* entryCached = &cachedEnigmaTable[i];
                    if (entry.requestId == entryCached->requestId) // found this entry? update details
                    {
                        if (!entry.CompareTo(*entryCached))
                        {
                            // changed, update
                            //*entryCached = entry;
                            updateEntry(entry);
                            //parent->emitDataChanged(i);
                        }
                        found = true;
                    }
                }
                if (!found)
                {
                    if (!force)
                    {
                        refreshEnigmaTable(true);
                        return;
                    }

                    parent->beginInsertRows(QModelIndex(), 0, 0);
                    cachedEnigmaTable.insert(0, entry);
                    //cachedAddressTable.append(lowerIndex, lowerIndex, entry);
                    parent->endInsertRows();
                }

            }

        }

        qSort(cachedEnigmaTable.begin(), cachedEnigmaTable.end(), EnigmaTableEntryGreaterThan());
        
        // qLowerBound() and qUpperBound() require our cachedCertIssuerTable list to be sorted in asc order
      //  if (doSort || true)
//        {
            //qSort(cachedPosaTable.begin(), cachedPosaTable.end(), PosaTableEntryGreaterThan());
     //       doSort = false;
     //   }

        if (force)
            parent->endResetModel();
    }

    void updateEntry(const EnigmaTableEntry &entry)
    {
        if(!parent)
        {
            return;
        }

        qSort(cachedEnigmaTable.begin(), cachedEnigmaTable.end(), EnigmaTableEntryLessThan());

        // Find in model
        QList<EnigmaTableEntry>::iterator lower = qLowerBound(
            cachedEnigmaTable.begin(), cachedEnigmaTable.end(), entry, EnigmaTableEntryLessThan());

        QList<EnigmaTableEntry>::iterator upper = qUpperBound(
            cachedEnigmaTable.begin(), cachedEnigmaTable.end(), entry, EnigmaTableEntryLessThan());

        int lowerIndex = (lower - cachedEnigmaTable.begin());
        //int upperIndex = (upper - cachedEnigmaTable.begin());
        bool inModel = (lower != upper);

        if(!inModel)
        {
            OutputDebugStringF("Warning: EnigmaTablePriv::updateEntry: Got CT_UPDATED, but entry is not in model\n");
            return;
        }

        lower->expiresInMs = entry.expiresInMs;
        lower->participantsacquired = entry.participantsacquired;
        lower->participantssigned = entry.participantssigned;
        lower->txid = entry.txid;
        lower->autoRetry = entry.autoRetry;
        lower->aborted = entry.aborted;

        parent->emitDataChanged(lowerIndex);
    }

    int size()
    {
        return cachedEnigmaTable.size();
    }

    EnigmaTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedEnigmaTable.size())
        {
            return &cachedEnigmaTable[idx];
        }
        else
        {
            return 0;
        }
    }
};

EnigmaTableModel::EnigmaTableModel(CWallet *wallet, WalletModel *parent) : QAbstractTableModel(parent),walletModel(parent),wallet(wallet),priv(0)
{
    columns << tr("Type") << tr("Initiated On") << tr("Total") << tr("Participants") << tr("Acquired") << tr("Signed") << tr("Status") << tr("Tx Id") << tr("Req Id");
    priv = new EnigmaTablePriv(wallet, this);
    priv->refreshEnigmaTable();

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(Update()));
    timer->start(MODEL_UPDATE_DELAY*2);
}

EnigmaTableModel::~EnigmaTableModel()
{
    delete priv;
}

void EnigmaTableModel::Update()
{
    if (fShutdown)
        return;

    priv->refreshEnigmaTable();
}

int EnigmaTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int EnigmaTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant EnigmaTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    EnigmaTableEntry *rec = static_cast<EnigmaTableEntry*>(index.internalPointer());

    if (role == Qt::DecorationRole)
    {
        switch(index.column())
        {
            case Type:
            {
               if (rec->txid != 0)
               {
                   return QIcon(":/icons/transaction_confirmed");
               }
            }
            /*
            case ExpiresInMs:
            {
               qint64 amt = (rec->expiresInMs / POSA_REQUEST_LIFETIME_SECS) * 5;
               if (amt < 1) amt = 1;
               if (amt > 5) amt = 5;
               switch(amt)
               {
               case 1: return QIcon(":/icons/transaction_1");
               case 2: return QIcon(":/icons/transaction_2");
               case 3: return QIcon(":/icons/transaction_3");
               case 4: return QIcon(":/icons/transaction_4");
               default: return QIcon(":/icons/transaction_5");
               };
            }
            */
            //case Status:
             //   return txStatusDecoration(rec);
            //case ToAddress:
             //   return txAddressDecoration(rec);
        }
        //break;
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Type:
            return rec->type == EnigmaTableEntry::MyCloaking ? "Send" : "Cloak";
        case InitiatedOn:
            return GUIUtil::dateTimeStrISO(rec->initiatedon);
        case TotalAmount:
            return QString::fromStdString(FormatMoney(rec->totalamount, false));
        case ParticipantsRequired:
            return rec->participantsrequired;
        case ParticipantsAcquired:
            return rec->participantsacquired;
        case ParticipantsSigned:
            return rec->participantssigned;
        case ExpiresInMs:
            if (rec->aborted)
                return "Aborted";
            if (rec->txid != 0)                                                                 // Got tx id? We're done
                return "Complete";
            if (rec->participantssigned < rec->participantsrequired && rec->expiresInMs > 0) {   // Not enough participants but still running? Fine
                if (rec->retryCount == 0)
                    return "Polling: "+QString::number(rec->expiresInMs)+" secs left";
                else
                    return "Retry "+QString::number(rec->retryCount)+": "+QString::number(rec->expiresInMs)+" secs left";
            }
            if (rec->autoRetry && rec->retryCount < ENIGMA_MAX_RETRIES && fEnigmaAutoRetry)         // Attempt failed :(
                return "Will retry ("+QString::number(ENIGMA_MAX_RETRIES - rec->retryCount)+" left)";
            else
                return "Failed (Expired)";
        case TxId:
            return QString::fromStdString(rec->txid.GetHex());
        case ReqId:
            return QString::fromStdString(rec->requestId.GetHex());
        }
    }
    else if (role == Qt::FontRole)
    {
        QFont font;
//        if(index.column() == Name)
//        {
//            font = GUIUtil::bitcoinAddressFont();
//        }
        return font;
    }
    return QVariant();
}

bool EnigmaTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid())
        return false;
    //PosaTableEntry *rec = static_cast<PosaTableEntry*>(index.internalPointer());

    return false;
}

QVariant EnigmaTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags EnigmaTableModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;
    //PosaTableEntry *rec = static_cast<PosaTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    
    return retval;
}

/*
Qt::ItemFlags AddressTableModel::flags(const QModelIndex & index) const
{
    if(!index.isValid())
        return 0;
    AddressTableEntry *rec = static_cast<AddressTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    // Can edit address and label for sending addresses,
    // and only label for receiving addresses.
    if(rec->type == AddressTableEntry::Sending ||
      (rec->type == AddressTableEntry::Receiving && index.column()==Label))
    {
        retval |= Qt::ItemIsEditable;
    }
    return retval;
}
*/

QModelIndex EnigmaTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    EnigmaTableEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void EnigmaTableModel::updateEntry(const EnigmaTableEntry &entry)
{
    // Update Enigma cloaking model from Bitcoin core
    priv->updateEntry(entry);
}

QString EnigmaTableModel::addRow(const EnigmaTableEntry &entry)
{
/*    std::string strTitle = title.toStdString();
    std::string strOffer = offer.toStdString();
    std::string strCategory = category.toStdString();
    std::string strPrice = price.toStdString();
    std::string strQuantity = quantity.toStdString();
    std::string strExpdepth = expdepth.toStdString();
	std::string strDescription = description.toStdString();
    editStatus = OK;

    if(false)
    {
        editStatus = INVALID_OFFER;
        return QString();
    }
    // Check for duplicate offeres
    {
        LOCK(wallet->cs_wallet);
        if(false)
        {
            editStatus = DUPLICATE_OFFER;
            return QString();
        }
    }

    // Add entry

    return QString::fromStdString(strOffer);
*/
  QString r;
  return r;
}

bool EnigmaTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    // refuse to remove operations.
    return false;
}

void EnigmaTableModel::emitDataChanged(int idx)
{
    emit dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length()-1, QModelIndex()));
}
