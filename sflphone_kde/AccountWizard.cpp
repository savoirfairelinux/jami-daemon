/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
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


/***************************************************************************
 *   Class AccountWizard                                                   *
 *   Widget of the wizard for creating an account.                         *
 ***************************************************************************/

AccountWizard::AccountWizard(QWidget *parent)
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
	//setPixmap(QWizard::BannerPixmap, QPixmap(":/images/icons/dial.svg"));
	setWindowTitle(tr("Account Wizard"));
	setPixmap(QWizard::LogoPixmap, QPixmap(":/images/icons/sflphone.png"));

}


AccountWizard::~AccountWizard()
{
}

void AccountWizard::accept()
 {
     QByteArray className = field("className").toByteArray();
     QByteArray baseClass = field("baseClass").toByteArray();
     QByteArray macroName = field("macroName").toByteArray();
     QByteArray baseInclude = field("baseInclude").toByteArray();

     QString outputDir = field("outputDir").toString();
     QString header = field("header").toString();
     QString implementation = field("implementation").toString();
     
     QDialog::accept();
 }


/***************************************************************************
 *   Class WizardIntroPage                                                 *
 *   Widget of the introduction page of the wizard                         *
 ***************************************************************************/

WizardIntroPage::WizardIntroPage(QWidget *parent)
     : QWizardPage(parent)
{
	setTitle(tr("Account Creation Wizard"));
	setSubTitle(tr("Welcome to the Account creation wizard of SFLPhone"));

	introLabel = new QLabel(tr("This wizard will help you setting up an account."));
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
	setTitle(tr("Accounts"));
	setSubTitle(tr("Please choose between those options :"));

	radioButton_SFL = new QRadioButton(tr("Create a free SIP/IAX2 account on sflphone.org"));
	radioButton_manual = new QRadioButton(tr("Register an existing SIP/IAX2 account"));
	radioButton_SFL->setChecked(true);

	registerField("SFL", radioButton_SFL);
	registerField("manual", radioButton_manual);

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
	setTitle(tr("VoIP Protocols"));
	setSubTitle(tr("Choose the account type :"));

	radioButton_SIP = new QRadioButton(tr("Create a SIP (Session Initiation Protocol) account"));
	radioButton_IAX = new QRadioButton(tr("Create a IAX2 (InterAsterisk eXchange) account"));
	radioButton_SIP->setChecked(true);
	
	registerField("SIP", radioButton_SIP);
	registerField("IAX", radioButton_IAX);

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
	setTitle(tr("Optionnal Email Address"));
	setSubTitle(tr("This email address will be used to send your voicemail messages."));

	label_emailAddress = new QLabel(tr("Email address"));
	lineEdit_emailAddress = new QLineEdit();
	
	registerField("emailAddress", lineEdit_emailAddress);

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
		setTitle(tr("SIP Account Settings"));
	}
	else
	{
		setTitle(tr("IAX2 Account Settings"));
	}
	setSubTitle(tr("Please full these settings fields."));

	label_alias = new QLabel(tr("Alias"));
	label_server = new QLabel(tr("Server"));
	label_user = new QLabel(tr("User"));
	label_password = new QLabel(tr("Password"));
	
	lineEdit_alias = new QLineEdit;
	lineEdit_server = new QLineEdit;
	lineEdit_user = new QLineEdit;
	lineEdit_password = new QLineEdit;

	lineEdit_password->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	
	if(type == SIP)
	{
		registerField("alias_SIP", lineEdit_alias);
		registerField("server_SIP", lineEdit_server);
		registerField("user_SIP", lineEdit_user);
		registerField("password_SIP", lineEdit_password);
	}
	else
	{
		registerField("alias_IAX", lineEdit_alias);
		registerField("server_IAX", lineEdit_server);
		registerField("user_IAX", lineEdit_user);
		registerField("password_IAX", lineEdit_password);
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
	setTitle(tr("Network Address Translation (NAT)"));
	setSubTitle(tr("You should probably enable this option if you're placed under a firewall :"));

	checkBox_enableStun = new QCheckBox(tr("Enable STUN"));
	label_StunServer = new QLabel(tr("Stun Server"));
	lineEdit_StunServer = new QLineEdit();
	
	registerField("enableStun", checkBox_enableStun);
	registerField("stunServer", lineEdit_StunServer);

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
	setTitle(tr("Account Definition Finished"));
	setSubTitle(tr("After checking the settings you chose, click \"Finish\" to create the account."));

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);
}

WizardAccountConclusionPage::~WizardAccountConclusionPage()
{
	//delete label_emailAddress;
}

int WizardAccountConclusionPage::nextId() const
{
	return -1;
}
