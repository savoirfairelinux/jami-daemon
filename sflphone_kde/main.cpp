#include <QApplication>
#include <QtCore/QString>
#include <QtGui/QCursor>
#include "kapplication.h"
#include "kcmdlineargs.h"
#include "kaboutdata.h"
#include <kabc/vcardformat.h>

#include "ConfigDialog.h"
#include "SFLPhone.h"
#include "AccountWizard.h"
#include "instance_interface_p.h"
#include "instance_interface_singleton.h"
#include "sflphone_const.h"

static const char description[] = I18N_NOOP("A KDE 4 Client for SflPhone");

static const char version[] = "0.1";

int main(int argc, char **argv)
{
	FILE *fp;
	int status;
	char path[PATH_MAX];


	fp = popen("ls *", "r");
	if (fp == NULL)
		qDebug() << "marche pas";
	while (fgets(path, PATH_MAX, fp) != NULL)
	printf("%s", path);

	status = pclose(fp);


	try
	{
		InstanceInterface & instance = InstanceInterfaceSingleton::getInstance();
		instance.Register(getpid(), APP_NAME);
		KAboutData about("sflphone_kde", 0, ki18n("sflphone_kde"), version, ki18n(description),
		                 KAboutData::License_GPL, ki18n("(C) 2009 Jérémy Quentin"), KLocalizedString(), 0, "jeremy.quentin@gsavoirfairelinux.com");
		about.addAuthor( ki18n("Jérémy Quentin"), KLocalizedString(), "jeremy.quentin@gmail.com" );
		KCmdLineArgs::init(argc, argv, &about);
		KCmdLineOptions options;
		//options.add("+[URL]", ki18n( "Document to open" ));
		KApplication app;
	
		QString locale = QLocale::system().name();
	
		QTranslator translator;
		translator.load(QString("config_") + locale);
		app.installTranslator(&translator);
	
		SFLPhone fenetre;

		fenetre.move(QCursor::pos());
		fenetre.show();
	
		return app.exec();	
	}
	catch(const char * msg)
	{
		printf("%s\n",msg);
	}
} 
