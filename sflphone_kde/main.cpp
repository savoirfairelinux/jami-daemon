#include <QApplication>
#include <QtCore/QString>
#include <QtGui/QCursor>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QAction>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>

#include "ConfigDialog.h"
#include "SFLPhone.h"
#include "AccountWizard.h"
#include "instance_interface_singleton.h"
#include "sflphone_const.h"

static const char description[] = I18N_NOOP("A KDE 4 Client for SflPhone");

static const char version[] = "0.1";

int main(int argc, char **argv)
{

	///home/jquentin/.kde/share/apps/kabc
/*	FILE *fp;
	int status;
	char path[PATH_MAX];


	fp = popen("ls *", "r");
	if (fp == NULL)
		qDebug() << "marche pas";
	while (fgets(path, PATH_MAX, fp) != NULL)
	printf("%s", path);

	status = pclose(fp);
*/

	try
	{
		InstanceInterface & instance = InstanceInterfaceSingleton::getInstance();
		instance.Register(getpid(), APP_NAME);
		
		KAboutData about(
		   "sflphone_kde", 
		   0, 
		   ki18n("sflphone_kde"), 
		   version, 
		   ki18n(description),
		   KAboutData::License_GPL, 
		   ki18n("(C) 2009 Jérémy Quentin"), 
		   KLocalizedString(), 
		   0, 
		   "jeremy.quentin@savoirfairelinux.com");
		
		about.addAuthor( ki18n("Jérémy Quentin"), KLocalizedString(), "jeremy.quentin@gmail.com" );
		KCmdLineArgs::init(argc, argv, &about);
		KCmdLineOptions options;
		//options.add("+[URL]", ki18n( "Document to open" ));
		KCmdLineArgs::addCmdLineOptions(options);
		KApplication app;
		
		SFLPhone * fenetre = new SFLPhone();
		
		QString locale = QLocale::system().name();
	
		QTranslator translator;
		translator.load(QString("config_") + locale);
		app.installTranslator(&translator);
	
	/*
		QApplication app(argc,argv);
		//
		QMainWindow * fenetre = new QMainWindow();
		QMenu * menu = new QMenu("menubb",0);
		fenetre->menuBar()->addMenu(menu);
		//QMenu * menu = fenetre->menuBar()->addMenu("menu");
		QAction * action = new QAction("actioncc", 0);
		action->setText("actionbb");
		menu->addAction(action);
		//fenetre->menuBar()->addMenu("menu");
*/
		fenetre->move(QCursor::pos().x() - fenetre->geometry().width()/2, QCursor::pos().y() - fenetre->geometry().height()/2);
		fenetre->show();
	
		return app.exec();	
	}
	catch(const char * msg)
	{
		printf("%s\n",msg);
	}
} 
