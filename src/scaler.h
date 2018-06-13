/*
 * Copyright (C) 2018 Schreibikus https://github.com/schreibikus
 * License: http://www.gnu.org/licenses/gpl.html GPL version 3 or higher
 */

#include <QtCore>
#include <QtGui/QImage>

class Scaler : public QObject
{
    Q_OBJECT
public:
    Scaler(QObject *parent, const char *infile, const char *outfile, int width, int height);

public slots:
    void run();

signals:
    void finished();
private:
    QImage image;
    QString output;
    int width;
    int height;
};
