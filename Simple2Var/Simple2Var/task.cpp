#include "task.h"

Task::Task(QObject *parent) :
    QObject(parent)
{
}

void Task::run()
{
    // Do processing here

    // And later on:
    emit finished();
}
