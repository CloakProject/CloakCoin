#include "tickertimer.h"

tickertimer::tickertimer(QObject *parent) :
    QObject(parent)
{
   this->timer = new QTimer(this);
   connect(this->timer, SIGNAL(timeout()), this, SLOT(update()));
}

void tickertimer::start()
{
    this->timer->start(60000);
}

void tickertimer::update()
{
    emit timerReady();
}
