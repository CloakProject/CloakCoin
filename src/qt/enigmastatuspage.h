#ifndef EnigmaStatusPage_H
#define EnigmaStatusPage_H

#include <QDialog>

namespace Ui {
    class EnigmaStatusPage;
}
class EnigmaTableModel;
class OptionsModel;
class QTimer;

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

/** Widget that shows a list of Cloaking operations.
  */
class EnigmaStatusPage : public QDialog
{
    Q_OBJECT

public:

    explicit EnigmaStatusPage(QWidget *parent = 0);
    ~EnigmaStatusPage();

    void setModel(EnigmaTableModel *model);
    void setOptionsModel(OptionsModel *optionsModel);
    const QString &getReturnValue() const { return returnValue; }    

public slots:
    void handleData(int index);

private slots:
    void cancelSend();
    void copyTxId();
    void ShowContextMenu(const QPoint &point);
    void updateCloakingAssistCounts();

private:    
    Ui::EnigmaStatusPage *ui;
    EnigmaTableModel *model;
    OptionsModel *optionsModel;
    QString returnValue;
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;
};

#endif // EnigmaStatusPage_H
