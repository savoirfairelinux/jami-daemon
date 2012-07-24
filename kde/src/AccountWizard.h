/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
 **************************************************************************/

#ifndef ACCOUNTWIZARD_H
#define ACCOUNTWIZARD_H

#include <QWizard>

//Qt
class QLabel;
class QRadioButton;
class KLineEdit;
class QCheckBox;

/**
   @author Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>
*/
class AccountWizard : public QWizard
{
Q_OBJECT

public:

   enum { Page_Intro, Page_AutoMan, Page_Type, Page_Email, Page_SIPForm, Page_IAXForm, Page_Stun, Page_Conclusion };

   AccountWizard(QWidget * parent = 0);
   ~AccountWizard();
   void accept();

};

/***************************************************************************
 *   Class WizardIntroPage                                                 *
 *   Widget of the introduction page of the wizard                         *
 ***************************************************************************/

class WizardIntroPage : public QWizardPage
{
   Q_OBJECT

public:
   WizardIntroPage(QWidget *parent = 0);
   ~WizardIntroPage();
   int nextId() const;

private:
   QLabel * introLabel;
};

/***************************************************************************
 *   Class WizardAccountAutoManualPage                                     *
 *   Page in which user choses to create an account on                     *
 *   sflphone.org or register a new one.                                   *
 ***************************************************************************/

class WizardAccountAutoManualPage : public QWizardPage
{
   Q_OBJECT

public:
   WizardAccountAutoManualPage(QWidget *parent = 0);
   ~WizardAccountAutoManualPage();
   int nextId() const;

private:
   QRadioButton* radioButton_SFL;
   QRadioButton* radioButton_manual;
};

/***************************************************************************
 *   Class WizardAccountTypePage                                           *
 *   Page in which user choses between SIP and IAX account.                *
 ***************************************************************************/

class WizardAccountTypePage : public QWizardPage
{
   Q_OBJECT

public:
   WizardAccountTypePage(QWidget *parent = 0);
   ~WizardAccountTypePage();
   int nextId() const;

private:
   QRadioButton* radioButton_SIP;
   QRadioButton* radioButton_IAX;
};

/***************************************************************************
 *   Class WizardAccountEmailAddressPage                                   *
 *   Page in which user choses between SIP and IAX account.                *
 ***************************************************************************/

class WizardAccountEmailAddressPage : public QWizardPage
{
   Q_OBJECT

public:
   WizardAccountEmailAddressPage(QWidget *parent = 0);
   ~WizardAccountEmailAddressPage();
   int nextId() const;

private:
   QLabel * label_emailAddress;
   KLineEdit * lineEdit_emailAddress;
   QLabel * label_enableZrtp;
   QCheckBox * checkBox_enableZrtp;
};

/***************************************************************************
 *   Class WizardAccountSIPFormPage                                        *
 *   Page of account settings.                                             *
 ***************************************************************************/

class WizardAccountSIPFormPage : public QWizardPage
{
   Q_OBJECT

public:

   WizardAccountSIPFormPage(QWidget *parent = 0);
   ~WizardAccountSIPFormPage();
   int nextId() const;

private:
   int type;

   QLabel* label_alias           ;
   QLabel* label_server          ;
   QLabel* label_user            ;
   QLabel* label_password        ;
   QLabel* label_voicemail       ;
   QLabel* label_enableZrtp      ;

   KLineEdit* lineEdit_alias     ;
   KLineEdit* lineEdit_server    ;
   KLineEdit* lineEdit_user      ;
   KLineEdit* lineEdit_password  ;
   KLineEdit* lineEdit_voicemail ;
   QCheckBox* checkBox_enableZrtp;
};

/***************************************************************************
 *   Class WizardAccountIAXFormPage                                        *
 *   Page of account settings.                                             *
 ***************************************************************************/

class WizardAccountIAXFormPage : public QWizardPage
{
   Q_OBJECT

public:

   WizardAccountIAXFormPage(QWidget *parent = 0);
   ~WizardAccountIAXFormPage();
   int nextId() const;

private:
   int type;

   QLabel* label_alias          ;
   QLabel* label_server         ;
   QLabel* label_user           ;
   QLabel* label_password       ;
   QLabel* label_voicemail      ;

   KLineEdit* lineEdit_alias    ;
   KLineEdit* lineEdit_server   ;
   KLineEdit* lineEdit_user     ;
   KLineEdit* lineEdit_password ;
   KLineEdit* lineEdit_voicemail;
};

/***************************************************************************
 *   Class WizardAccountStunPage                                           *
 *   Page of Stun settings.                                                *
 ***************************************************************************/

class WizardAccountStunPage : public QWizardPage
{
   Q_OBJECT

public:
   WizardAccountStunPage(QWidget *parent = 0);
   ~WizardAccountStunPage();
   int nextId() const;

private:
   QCheckBox* checkBox_enableStun;
   QLabel*    label_StunServer   ;
   KLineEdit* lineEdit_StunServer;
};

/***************************************************************************
 *   Class WizardAccountConclusionPage                                     *
 *   Conclusion page.                                                      *
 ***************************************************************************/

class WizardAccountConclusionPage : public QWizardPage
{
   Q_OBJECT

public:
   WizardAccountConclusionPage(QWidget *parent = 0);
   ~WizardAccountConclusionPage();
   int nextId() const;

private:
};

#endif
