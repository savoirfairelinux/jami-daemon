#include <QApplication>
#include <QtGui>
#include "ConfigDialog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString locale = QLocale::system().name();

    QTranslator translator;
    translator.load(QString("config_") + locale);
    app.installTranslator(&translator);

    ConfigurationDialog fenetre;
    fenetre.show();

    return app.exec();
} 
