#include "editaddressdialog.h"
#include "ui_editaddressdialog.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "bitcoinaddressvalidator.h"

#include <QDataWidgetMapper>
#include <QMessageBox>

EditAddressDialog::EditAddressDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditAddressDialog), mapper(0), mode(mode), model(0)
{
    ui->setupUi(this);

    switch(mode)
    {
    case NewCloakReceivingAddress:
    case NewReceivingAddress:
        setWindowTitle(tr("New receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case NewCloakSendingAddress:
    case NewSendingAddress:
        setWindowTitle(tr("New sending address"));
        ui->addressEdit->setEnabled(true);
        break;
    case EditCloakReceivingAddress:
    case EditReceivingAddress:
        setWindowTitle(tr("Edit receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case EditCloakSendingAddress:
    case EditSendingAddress:
        setWindowTitle(tr("Edit sending address"));
        ui->addressEdit->setEnabled(true);
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);

    bool isStealth = mode == NewCloakReceivingAddress || mode == EditCloakReceivingAddress
            || mode == NewCloakSendingAddress || mode == EditCloakReceivingAddress;

    GUIUtil::setupAddressWidget(ui->addressEdit, this, isStealth);
}

EditAddressDialog::~EditAddressDialog()
{
    delete ui;
}

void EditAddressDialog::setModel(AddressTableModel *model)
{
    this->model = model;
    mapper->setModel(model);
    mapper->addMapping(ui->labelEdit, AddressTableModel::Label);
    mapper->addMapping(ui->addressEdit, AddressTableModel::Address);
//    mapper->addMapping(ui->stealthCB, AddressTableModel::Type);
}

void EditAddressDialog::loadRow(int row)
{
    bool isStealth = model->data(model->index(row, AddressTableModel::Address, QModelIndex()), Qt::DisplayRole).toString().length() > BitcoinAddressValidator::MaxAddressLength;
    GUIUtil::setupAddressWidget(ui->addressEdit, this, isStealth);
    mapper->setCurrentIndex(row);
}

bool EditAddressDialog::saveCurrentRow()
{
    if(!model)
        return false;

    switch(mode)
    {
    case NewCloakReceivingAddress:
    case NewCloakSendingAddress:
    case NewReceivingAddress:
    case NewSendingAddress:
    {
        QString addType = mode == NewSendingAddress || mode == NewCloakSendingAddress ? AddressTableModel::Send : AddressTableModel::Receive;
        int typeInd = mode == NewCloakSendingAddress || mode == NewCloakReceivingAddress ? AddressTableModel::AT_Stealth : AddressTableModel::AT_Normal;

        address = model->addRow(
                    addType,
                    ui->labelEdit->text(),
                    ui->addressEdit->text(),
                    typeInd);
        break;
    }
    case EditCloakReceivingAddress:
    case EditCloakSendingAddress:
    case EditReceivingAddress:
    case EditSendingAddress:
        if(mapper->submit())
        {
            address = ui->addressEdit->text();
        }
        break;
    }
    return !address.isEmpty();
}

void EditAddressDialog::accept()
{
    if(!model)
        return;

    if(!saveCurrentRow())
    {
        switch(model->getEditStatus())
        {
        case AddressTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case AddressTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is already in the address book.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not a valid CloakCoin address.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        case AddressTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("Could not unlock wallet."),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        case AddressTableModel::KEY_GENERATION_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("New key generation failed."),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        case AddressTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        }

        return;
    }
    QDialog::accept();
}

QString EditAddressDialog::getAddress() const
{
    return address;
}

void EditAddressDialog::setAddress(const QString &address)
{
    this->address = address;
    ui->addressEdit->setText(address);
}
