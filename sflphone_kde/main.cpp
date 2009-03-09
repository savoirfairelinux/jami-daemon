#include <QApplication>
#include <QtGui>
#include "ConfigDialog.h"
#include "SFLPhone.h"

int main(int argc, char *argv[])
{
	try
	{
		QApplication app(argc, argv);
	
		QString locale = QLocale::system().name();
	
		QTranslator translator;
		translator.load(QString("config_") + locale);
		app.installTranslator(&translator);
	
		SFLPhone fenetre;
		fenetre.show();
	
		return app.exec();	
	}
	catch(const char * msg)
	{
		printf("%s\n",msg);
	}
} 
