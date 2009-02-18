#include <QtGui>
#include <QtCore>
#include <iostream>
#include <stdarg.h>
#include "sflphone_const.h"
#include "metatypes.h"
#include "ConfigDialog.h"

using namespace std;

ConfigurationDialog::ConfigurationDialog(QDialog *parent) : QDialog(parent)
{

	setupUi(this);

	registerCommTypes();
	
	daemon = new DaemonInterface("org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager",
                           QDBusConnection::sessionBus(), this);


	QDBusReply<QStringList> r = (daemon->getAccountList());
	QStringList accountIds= r.value();
	//accountIds = r.value();
	QString str = accountIds[0];
	QDBusReply<MapStringString> r2 = (daemon->getAccountDetails(str));
	//string st = accountIds[0].toStdString();
	//QString str = "youpi";
	//cout << str;
	//accountList = new AccountList(accountIds);

}

void ConfigurationDialog::on_buttonNouveauCompte_clicked()
{
	QString itemName = QInputDialog::getText(this, "Item", "Enter new item");
	itemName = itemName.simplified();
	if (!itemName.isEmpty()) {
		listWidgetComptes->addItem(itemName);
		int r = listWidgetComptes->count() - 1;
		QListWidgetItem * item = listWidgetComptes->item(r);
		item->setCheckState(Qt::Unchecked);
		item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
		accountList->addAccount(*item, itemName);
		listWidgetComptes->setCurrentRow(r);
	}
}

void ConfigurationDialog::on_listWidgetComptes_currentItemChanged ( QListWidgetItem * current, QListWidgetItem * previous )
{
	saveAccount(previous);
	loadAccount(current);
}

void ConfigurationDialog::loadAccount(QListWidgetItem * item)
{
	Account * a = accountList->getAccountByItem(item);
	if(! a )
	{
		cout << "Chargement d'un compte inexistant\n";
	}
	else
	{
		edit1_Alias->setText( a->getAccountDetail(*(new QString(ACCOUNT_ALIAS))));
		edit2_Protocole->setCurrentIndex(getProtocoleIndex(a->getAccountDetail(*(new QString(ACCOUNT_TYPE)))));
		edit3_Serveur->setText( a->getAccountDetail(*(new QString(ACCOUNT_HOSTNAME))));
		edit4_Usager->setText( a->getAccountDetail(*(new QString(ACCOUNT_USERNAME))));
		edit5_Mdp->setText( a->getAccountDetail(*(new QString(ACCOUNT_PASSWORD))));
		edit6_BoiteVocale->setText( a->getAccountDetail(*(new QString(ACCOUNT_MAILBOX))));
	}
}


void ConfigurationDialog::saveAccount(QListWidgetItem * item)
{
	Account * a = accountList->getAccountByItem(item);
	if(! a)
	{
		cout << "Sauvegarde d'un compte inexistant\n";
	}
	else
	{
		a->setAccountDetail(ACCOUNT_ALIAS, edit1_Alias->text());
		a->setAccountDetail(ACCOUNT_TYPE, getIndexProtocole(edit2_Protocole->currentIndex()));
		a->setAccountDetail(ACCOUNT_HOSTNAME, edit3_Serveur->text());
		a->setAccountDetail(ACCOUNT_USERNAME, edit4_Usager->text());
		a->setAccountDetail(ACCOUNT_PASSWORD, edit5_Mdp->text());
		a->setAccountDetail(ACCOUNT_MAILBOX, edit6_BoiteVocale->text());
	}
}



void ConfigurationDialog::on_edit1_Alias_textChanged(const QString & text)
{
	listWidgetComptes->currentItem()->setText(text);
}
