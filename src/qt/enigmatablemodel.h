#ifndef ENIGMATABLEMODEL_H
#define ENIGMATABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include "init.h"

class EnigmaTablePriv;
class CWallet;
class WalletModel;

enum EnigmaType {MyCloaking=0,Participant};

struct EnigmaTableEntry
{
    enum Type {
        MyCloaking,
        Participant
    };

    uint256 requestId;
    Type type;
    int64 initiatedon;
    int64 totalamount;
    short participantsrequired;
    short participantsacquired;
    short participantssigned;
    int64 expiresInMs;
    uint256 txid;    
    bool autoRetry;
    short retryCount;
    bool aborted;

    EnigmaTableEntry() {}
    EnigmaTableEntry(const uint256 &reqid, Type type, const int64 &initiatedon, const int64 &totalamount,
                   const short &participantsrequired, const short &participantsacquired,const short &participantssigned,
                   const int64 &expiresInMs, const uint256 &txid, const bool &autoRetry, const short &retryCount, const bool aborted):
        requestId(reqid), type(type), initiatedon(initiatedon), totalamount(totalamount), participantsrequired(participantsrequired),
        participantsacquired(participantsacquired), participantssigned(participantssigned), expiresInMs(expiresInMs), txid(txid), autoRetry(autoRetry), retryCount(retryCount), aborted(aborted) {}

    bool CompareTo(EnigmaTableEntry entry)
    {
                return (this->expiresInMs == entry.expiresInMs &&
                         this->requestId == entry.requestId &&
                         this->participantsacquired == entry.participantsrequired &&
                         this->participantssigned == entry.participantssigned &&
                         this->txid == entry.txid &&
                         this->autoRetry == entry.autoRetry &&
                         this->retryCount == entry.retryCount &&
                         this->aborted == entry.aborted
                         );
    }
};

typedef enum EnigmaType EnigmaModelType;
/**
   Qt model of the offer book in the core. This allows views to access and modify the offer book.
 */
class EnigmaTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit EnigmaTableModel(CWallet *wallet, WalletModel *parent = 0);
    ~EnigmaTableModel();

    enum ColumnIndex {
	  Type = 0,
	  InitiatedOn = 1,
      TotalAmount = 2,
      ParticipantsRequired = 3,
      ParticipantsAcquired = 4,
      ParticipantsSigned = 5,
      ExpiresInMs = 6,
      TxId = 7,
      ReqId = 8
    };

    enum RoleIndex {
        TypeRole = Qt::UserRole /**< Type of offer (#Send or #Receive) */
    };   

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex &index) const;
    /*@}*/

    /* Add an offer to the model.
       Returns the added offer on success, and an empty string otherwise.
     */
    QString addRow(const EnigmaTableEntry &entry);

private:
    WalletModel *walletModel;
    CWallet *wallet;
    EnigmaTablePriv *priv;
    QStringList columns;
    EnigmaModelType modelType;
    /** Notify listeners that data changed. */
    void emitDataChanged(int index);

public slots:
    /* Update offer list from core.
     */
    void updateEntry(const EnigmaTableEntry &entry);
    void Update();
    friend class EnigmaTablePriv;
};

#endif // ENIGMATABLEMODEL_H
