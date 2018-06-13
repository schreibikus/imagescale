/*
 * Copyright (C) 2018 Schreibikus https://github.com/schreibikus
 * License: http://www.gnu.org/licenses/gpl.html GPL version 3 or higher
 */

#include <QtCore>
#include "scaler.h"

int main(int argc, char *argv[])
{
    if(argc < 5)
    {
        fprintf(stderr, "Usage: %s inputimage outputimage WIDTH HEIGHT\n", argv[0]);
        return 1;
    }
    QCoreApplication a(argc, argv);

    Scaler *task = new Scaler(&a, argv[1], argv[2], atoi(argv[3]), atoi(argv[4]));

    QObject::connect(task, SIGNAL(finished()), &a, SLOT(quit()));

    // This will run the task from the application event loop.
    QTimer::singleShot(0, task, SLOT(run()));

    return a.exec();
}