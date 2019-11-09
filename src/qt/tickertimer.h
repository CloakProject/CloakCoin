#ifndef TICKERTIMER_H
#define TICKERTIMER_H

#include <QObject>
#include <QTimer>
#include <QDebug>

class tickertimer : public QObject
{
    Q_OBJECT
public:
    explicit tickertimer(QObject *parent = 0);
    void start();
    QTimer *timer;
signals:
    void timerReady();
public slots:
    void update();
private:

};

#endif // TICKERTIMER_H
