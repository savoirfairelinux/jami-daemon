/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
 *                                                                         *
 *   This program is free software; you can redistr2i18nibute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distr2i18nibuted in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "AccountWizard.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"

#include <klocale.h>

#include <netdb.h>


#define FIELD_SFL_ACCOUNT        "SFL"
#define FIELD_OTHER_ACCOUNT      "OTHER"
#define FIELD_SIP_ACCOUNT        "SIP"
#define FIELD_IAX_ACCOUNT        "IAX"
#define FIELD_EMAIL_ADDRESS      "EMAIL_ADDRESS"
#define FIELD_ENABLE_STUN        "ENABLE_STUN"
#define FIELD_STUN_SERVER        "STUN_SERVER"
#define FIELD_SIP_ALIAS          "SIP_ALIAS"
#define FIELD_SIP_SERVER         "SIP_SERVER"
#define FIELD_SIP_USER           "SIP_USER"
#define FIELD_SIP_PASSWORD       "SIP_PASSWORD"
#define FIELD_IAX_ALIAS          "IAX_ALIAS"
#define FIELD_IAX_SERVER         "IAX_SERVER"
#define FIELD_IAX_USER           "IAX_USER"
#define FIELD_IAX_PASSWORD       "IAX_PASSWORD"


#define SFL_ACCOUNT_HOST         "sip.sflphone.org"

/***************************************************************************
 *   Global functions for creating an account on sflphone.org              *
 *                                                                         *
 ***************************************************************************/

typedef struct {
	char success;
	char reason[200];
	char user[200];
	char passwd[200];
} rest_account;

int req(char *host, int port, char *req, char *ret) {

	int s;
	struct sockaddr_in servSockAddr;
	struct hostent *servHostEnt;
	long int length=0;
	long int status=0;
	int i=0;
	FILE *f;
	char buf[1024];
	
	bzero(&servSockAddr, sizeof(servSockAddr));
	servHostEnt = gethostbyname(host);
	if (servHostEnt == NULL) {
		strcpy(ret, "gethostbyname");
		return -1;
	}
	bcopy((char *)servHostEnt->h_addr, (char *)&servSockAddr.sin_addr, servHostEnt->h_length);
	servSockAddr.sin_port = htons(port);
	servSockAddr.sin_family = AF_INET;
  
	if ((s = socket(AF_INET,SOCK_STREAM,0)) < 0) {
		strcpy(ret, "socket");
		return -1;
	}
  
	if(connect(s, (const struct sockaddr *) &servSockAddr, (socklen_t) sizeof(servSockAddr)) < 0 ) {
		perror("foo");
		strcpy(ret, "connect");
		return -1;
	}
  
	f = fdopen(s, "r+");
	
	fprintf(f, "%s HTTP/1.1\r\n", req);
	fprintf(f, "Host: %s\r\n", host);
	fputs("User-Agent: SFLphone\r\n", f);
	fputs("\r\n", f);

	while (strncmp(fgets(buf, sizeof(buf), f), "\r\n", 2)) {
		const char *len_h = "content-length";
		const char *status_h = "HTTP/1.1";
		if (strncasecmp(buf, len_h, strlen(len_h)) == 0)
			length = atoi(buf + strlen(len_h) + 1);
		if (strncasecmp(buf, status_h, strlen(status_h)) == 0)
			status = atoi(buf + strlen(status_h) + 1);
	}
	for (i = 0; i < length; i++)
		ret[i] = fgetc(f);
	
	if (status != 200) {
		sprintf(ret, "http error: %ld", status);
		return -1;
	}

	fclose(f);
	shutdown(s, 2);
	close(s);
	return 0;
}

rest_account get_rest_account(char *host,char *email) {
	char ret[4096];
	rest_account ra;
	bzero(ret, sizeof(ret));
	printf("HOST: %s\n", host);
	strcpy(ret,"GET /rest/accountcreator?email=");
	strcat(ret, email);
	if (req(host, 80, ret, ret) != -1) {
		strcpy(ra.user, strtok(ret, "\n"));
		strcpy(ra.passwd, strtok(NULL, "\n"));\
		ra.success = 1;
	} else {
		ra.success = 0;
		strcpy(ra.reason, ret);
	}
	puts(ret);
	return ra;
} 

/***************************************************************************
 *   Class AccountWizard                                                   *
 *   Widget of the wizard for creating an account.                         *
 ***************************************************************************/

AccountWizard::AccountWizard(QWidget * parent)
 : QWizard(parent)
{
	setPage(Page_Intro, new WizardIntroPage);
	setPage(Page_AutoMan, new WizardAccountAutoManualPage);
	setPage(Page_Type, new WizardAccountTypePage);
	setPage(Page_Email, new WizardAccountEmailAddressPage);
	setPage(Page_SIPForm, new WizardAccountFormPage(SIP));
	setPage(Page_IAXForm, new WizardAccountFormPage(IAX));
	setPage(Page_Stun, new WizardAccountStunPage);
	setPage(Page_Conclusion, new WizardAccountConclusionPage);
	
	setStartId(Page_Intro);
	setWindowTitle(tr2i18n("Account Creation Wizard"));
	setWindowIcon(QIcon(ICON_SFLPHONE));
	setMinimumHeight(350);
// 	setPixmap(QWizard::LogoPixmap, QPixmap(ICON_SFLPHONE));
	setPixmap(QWizard::WatermarkPixmap, QPixmap(ICON_SFLPHONE));
// 	setPixmap(QWizard::BannerPixmap, QPixmap(ICON_SFLPHONE));
// 	setPixmap(QWizard::BackgroundPixmap, QPixmap(ICON_SFLPHONE));
}


AccountWizard::~AccountWizard()
{
}

void AccountWizard::accept()
{
	QString ret;
	MapStringString accountDetails;
	
	QString & alias = accountDetails[QString(ACCOUNT_ALIAS)];
	QString & server = accountDetails[QString(ACCOUNT_HOSTNAME)];
	QString & user = accountDetails[QString(ACCOUNT_USERNAME)];
	QString & password = accountDetails[QString(ACCOUNT_PASSWORD)];
	QString & protocol = accountDetails[QString(ACCOUNT_TYPE)];
	QString & mailbox = accountDetails[QString(ACCOUNT_MAILBOX)];
	QString & enabled = accountDetails[QString(ACCOUNT_ENABLED)];
	
	bool createAccount = false;
	bool sip = false;
	bool SFL = field(FIELD_SFL_ACCOUNT).toBool();
	if(SFL)
	{
		QString emailAddress = field(FIELD_EMAIL_ADDRESS).toString();
		char charEmailAddress[1024];
		strncpy(charEmailAddress, emailAddress.toLatin1(), sizeof(charEmailAddress) - 1);

		rest_account acc = get_rest_account(SFL_ACCOUNT_HOST, charEmailAddress);
		if(acc.success)
		{
			ret += tr2i18n("Creation of account succeed with these parameters") + " :\n";
			alias = QString(acc.user) + "@" + SFL_ACCOUNT_HOST;
			server = QString(SFL_ACCOUNT_HOST);
			user = QString(acc.user);
			password = QString(acc.passwd);
			protocol = QString(ACCOUNT_TYPE_SIP);
			createAccount = true;
			sip = true;
		}
		else
		{
			ret += tr2i18n("Creation of account has failed for the reason") + " :\n";
			ret += acc.reason;
		}
	}
	else
	{
		ret += tr2i18n("Register of account succeed with these parameters") + " :\n";
		bool SIPAccount = field(FIELD_SIP_ACCOUNT).toBool();
		if(SIPAccount)
		{
			alias = field(FIELD_SIP_ALIAS).toString();
			server = field(FIELD_SIP_SERVER).toString();
			user = field(FIELD_SIP_USER).toString();
			password = field(FIELD_SIP_PASSWORD).toString();
			protocol = QString(ACCOUNT_TYPE_SIP);
			sip = true;
			
		}
		else
		{
			alias = field(FIELD_IAX_ALIAS).toString();
			server = field(FIELD_IAX_SERVER).toString();
			user = field(FIELD_IAX_USER).toString();
			password = field(FIELD_IAX_PASSWORD).toString();
			protocol = QString(ACCOUNT_TYPE_IAX);
		}
		createAccount = true;
	}
	if(createAccount)
	{
// 		mailbox = ACCOUNT_MAILBOX_DEFAULT_VALUE;
		enabled = ACCOUNT_ENABLED_TRUE;
		ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
		QString accountId = configurationManager.addAccount(accountDetails);
		//configurationManager.sendRegister(accountId, 1);
		if(sip)
		{
			bool enableStun = field(FIELD_ENABLE_STUN).toBool();
			QString stunServer = field(FIELD_STUN_SERVER).toString();
			if(enableStun != configurationManager.isStunEnabled()) configurationManager.enableStun();
			if(enableStun) configurationManager.setStunServer(stunServer);
		}
		ret += tr2i18n("Alias") + " : " + alias + "\n";
		ret += tr2i18n("Server") + " : " + server + "\n";
		ret += tr2i18n("User") + " : " + user + "\n";
		ret += tr2i18n("Password") + " : " + password + "\n";
		ret += tr2i18n("Protocol") + " : " + protocol + "\n";
		ret += tr2i18n("Mailbox") + " : " + mailbox + "\n";
	}
	qDebug() << ret;
	QDialog::accept();
}
 



/***************************************************************************
 *   Class WizardIntroPage                                                 *
 *   Widget of the intr2i18noduction page of the wizard                         *
 ***************************************************************************/

WizardIntroPage::WizardIntroPage(QWidget *parent)
     : QWizardPage(parent)
{
	setTitle(tr2i18n("Account Creation Wizard"));
	setSubTitle(tr2i18n("Welcome to the Account creation wizard of SFLPhone"));

	introLabel = new QLabel(tr2i18n("This wizard will help you setting up an account."));
	introLabel->setWordWrap(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(introLabel);
	setLayout(layout);
}
 
 
WizardIntroPage::~WizardIntroPage()
{
	delete introLabel;
}

int WizardIntroPage::nextId() const
{
	return AccountWizard::Page_AutoMan;
}

/***************************************************************************
 *   Class WizardAccountAutoManualPage                                     *
 *   Page in which user choses to create an account on                     *
 *   sflphone.org or register a new one.                                   *
 ***************************************************************************/

WizardAccountAutoManualPage::WizardAccountAutoManualPage(QWidget *parent)
     : QWizardPage(parent)
{
	setTitle(tr2i18n("Accounts"));
	setSubTitle(tr2i18n("Please choose between those options :"));

	radioButton_SFL = new QRadioButton(tr2i18n("Create a free SIP/IAX2 account on sflphone.org"));
	radioButton_manual = new QRadioButton(tr2i18n("Register an existing SIP/IAX2 account"));
	radioButton_SFL->setChecked(true);

	registerField(FIELD_SFL_ACCOUNT, radioButton_SFL);
	registerField(FIELD_OTHER_ACCOUNT, radioButton_manual);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(radioButton_SFL);
	layout->addWidget(radioButton_manual);
	setLayout(layout);
}
 
 
WizardAccountAutoManualPage::~WizardAccountAutoManualPage()
{
	delete radioButton_SFL;
	delete radioButton_manual;
}

int WizardAccountAutoManualPage::nextId() const
{
	if(radioButton_SFL->isChecked())
	{
		return AccountWizard::Page_Email;
	}
	else
	{
		return AccountWizard::Page_Type;
	}
}

/***************************************************************************
 *   Class WizardAccountTypePage                                           *
 *   Page in which user choses between SIP and IAX account.                *
 ***************************************************************************/

WizardAccountTypePage::WizardAccountTypePage(QWidget *parent)
     : QWizardPage(parent)
{
	setTitle(tr2i18n("VoIP Protocols"));
	setSubTitle(tr2i18n("Choose the account type") + " :");

	radioButton_SIP = new QRadioButton(tr2i18n("Register a SIP (Session Initiation Protocol) account"));
	radioButton_IAX = new QRadioButton(tr2i18n("Register a IAX2 (InterAsterisk eXchange) account"));
	radioButton_SIP->setChecked(true);
	
	registerField(FIELD_SIP_ACCOUNT, radioButton_SIP);
	registerField(FIELD_IAX_ACCOUNT, radioButton_IAX);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(radioButton_SIP);
	layout->addWidget(radioButton_IAX);
	setLayout(layout);
}
 
 
WizardAccountTypePage::~WizardAccountTypePage()
{
	delete radioButton_SIP;
	delete radioButton_IAX;
}

int WizardAccountTypePage::nextId() const
{
	if(radioButton_SIP->isChecked())
	{
		return AccountWizard::Page_SIPForm;
	}
	else
	{
		return AccountWizard::Page_IAXForm;
	}
}

/***************************************************************************
 *   Class WizardAccountEmailAddressPage                                   *
 *   Page in which user choses between SIP and IAX account.                *
 ***************************************************************************/

WizardAccountEmailAddressPage::WizardAccountEmailAddressPage(QWidget *parent)
     : QWizardPage(parent)
{
	setTitle(tr2i18n("Optionnal Email Address"));
	setSubTitle(tr2i18n("This email address will be used to send your voicemail messages."));

	label_emailAddress = new QLabel(tr2i18n("Email address"));
	lineEdit_emailAddress = new QLineEdit();
	
	registerField(FIELD_EMAIL_ADDRESS, lineEdit_emailAddress);

	QFormLayout *layout = new QFormLayout;
	layout->setWidget(0, QFormLayout::LabelRole, label_emailAddress);
	layout->setWidget(0, QFormLayout::FieldRole, lineEdit_emailAddress);
	setLayout(layout);
}
 
 
WizardAccountEmailAddressPage::~WizardAccountEmailAddressPage()
{
	delete label_emailAddress;
	delete lineEdit_emailAddress;
}

int WizardAccountEmailAddressPage::nextId() const
{
	return AccountWizard::Page_Stun;
}

/***************************************************************************
 *   Class WizardAccountFormPage                                           *
 *   Page of account settings.                                             *
 ***************************************************************************/

WizardAccountFormPage::WizardAccountFormPage(int type, QWidget *parent)
     : QWizardPage(parent)
{
	this->type = type;
	if(type == SIP)
	{
		setTitle(tr2i18n("SIP Account Settings"));
	}
	else
	{
		setTitle(tr2i18n("IAX2 Account Settings"));
	}
	setSubTitle(tr2i18n("Please full these settings fields."));

	label_alias = new QLabel(tr2i18n("Alias") + " *");
	label_server = new QLabel(tr2i18n("Server") + " *");
	label_user = new QLabel(tr2i18n("User") + " *");
	label_password = new QLabel(tr2i18n("Password") + " *");
	
	lineEdit_alias = new QLineEdit;
	lineEdit_server = new QLineEdit;
	lineEdit_user = new QLineEdit;
	lineEdit_password = new QLineEdit;

	lineEdit_password->setEchoMode(QLineEdit::Password);
	
	if(type == SIP)
	{
		registerField(QString(FIELD_SIP_ALIAS) + "*", lineEdit_alias);
		registerField(QString(FIELD_SIP_SERVER) + "*", lineEdit_server);
		registerField(QString(FIELD_SIP_USER) + "*", lineEdit_user);
		registerField(QString(FIELD_SIP_PASSWORD) + "*", lineEdit_password);
	}
	else
	{
		registerField(QString(FIELD_IAX_ALIAS) + "*", lineEdit_alias);
		registerField(QString(FIELD_IAX_SERVER) + "*", lineEdit_server);
		registerField(QString(FIELD_IAX_USER) + "*", lineEdit_user);
		registerField(QString(FIELD_IAX_PASSWORD) + "*", lineEdit_password);
	}
	
	QFormLayout *layout = new QFormLayout;
	
	
	layout->setWidget(0, QFormLayout::LabelRole, label_alias);
	layout->setWidget(0, QFormLayout::FieldRole, lineEdit_alias);
   layout->setWidget(1, QFormLayout::LabelRole, label_server);
	layout->setWidget(1, QFormLayout::FieldRole, lineEdit_server);
   layout->setWidget(2, QFormLayout::LabelRole, label_user);
	layout->setWidget(2, QFormLayout::FieldRole, lineEdit_user);
   layout->setWidget(3, QFormLayout::LabelRole, label_password);
	layout->setWidget(3, QFormLayout::FieldRole, lineEdit_password);
	
	setLayout(layout);
}
 
 
WizardAccountFormPage::~WizardAccountFormPage()
{
	delete label_alias;
	delete label_server;
	delete label_user;
	delete label_password;
	delete lineEdit_alias;
	delete lineEdit_server;
	delete lineEdit_user;
	delete lineEdit_password;
}

int WizardAccountFormPage::nextId() const
{
	if(type == SIP)
	{
		return AccountWizard::Page_Stun;
	}
	else
	{
		return AccountWizard::Page_Conclusion;
	}
}

/***************************************************************************
 *   Class WizardAccountStunPage                                           *
 *   Page of Stun settings.                                                *
 ***************************************************************************/

WizardAccountStunPage::WizardAccountStunPage(QWidget *parent)
     : QWizardPage(parent)
{
	setTitle(tr2i18n("Network Address Translation (NAT)"));
	setSubTitle(tr2i18n("You should probably enable this option if you're placed under a firewall"));

	checkBox_enableStun = new QCheckBox(tr2i18n("Enable STUN"));
	label_StunServer = new QLabel(tr2i18n("Stun Server"));
	lineEdit_StunServer = new QLineEdit();
	
	registerField(FIELD_ENABLE_STUN, checkBox_enableStun);
	registerField(FIELD_STUN_SERVER, lineEdit_StunServer);

	QFormLayout *layout = new QFormLayout;
	layout->addWidget(checkBox_enableStun);
	layout->addWidget(label_StunServer);
	layout->addWidget(lineEdit_StunServer);
	setLayout(layout);
}


WizardAccountStunPage::~WizardAccountStunPage()
{
	delete checkBox_enableStun;
	delete label_StunServer;
	delete lineEdit_StunServer;
}

int WizardAccountStunPage::nextId() const
{
	return AccountWizard::Page_Conclusion;
}

/***************************************************************************
 *   Class WizardAccountConclusionPage                                     *
 *   Conclusion page.                                                      *
 ***************************************************************************/

WizardAccountConclusionPage::WizardAccountConclusionPage(QWidget *parent)
     : QWizardPage(parent)
{
	setTitle(tr2i18n("Account Definition Finished"));
	setSubTitle(tr2i18n("After checking the settings you chose, click \"Finish\" to create the account."));

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);
}

WizardAccountConclusionPage::~WizardAccountConclusionPage()
{
}

int WizardAccountConclusionPage::nextId() const
{
	return -1;
}
