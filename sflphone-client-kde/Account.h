#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QtCore/QString>
#include <QtGui/QListWidgetItem>
#include <QtGui/QColor>

#include "metatypes.h"
#include "AccountItemWidget.h"

const QString account_state_name(QString & s);

class Account{
	
private:

	QString * accountId;
	MapStringString * accountDetails;
	QListWidgetItem * item;
	AccountItemWidget * itemWidget;

	Account();

public:
	
	//Constructors
	static Account * buildExistingAccountFromId(QString _accountId);
	static Account * buildNewAccountFromAlias(QString alias);
	
	~Account();
	
	//Getters
	bool isNew() const;
	bool isChecked() const;
	QString & getAccountId();
	MapStringString & getAccountDetails() const;
	QListWidgetItem * getItem();
	QListWidgetItem * renewItem();
	AccountItemWidget * getItemWidget();
	QString getStateName(QString & state);
	QColor getStateColor();
	QString getStateColorName();
	QString getAccountDetail(QString param) const;
	QString getAlias();
	
	//Setters
	void initAccountItem();
	void setAccountId(QString id);
	void setAccountDetails(MapStringString m);
	void setAccountDetail(QString param, QString val);
	
	//Operators
	bool operator==(const Account&)const;
	
	
};



#endif