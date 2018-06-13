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
