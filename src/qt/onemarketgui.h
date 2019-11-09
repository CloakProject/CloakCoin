#ifndef ONEMARKET_H
#define ONEMARKET_H

#include <QMainWindow>
#include <QDebug>
#include "net.h"

namespace Ui {
class OneMarket;
}

class OneMarket : public QMainWindow
{
    Q_OBJECT

public:
    explicit OneMarket(QWidget *parent = 0);
    ~OneMarket();
    void initializeOneMarket();

private slots:
    void on_createListingsSubmitButton_clicked();

private:
    Ui::OneMarket *ui;
    bool isInitialized;
};

#endif // ONEMARKET_H
