#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QtGui>
#include "metatypes.h"

const QString account_state_name(QString & s);

class Account{
	
private:

	QString * accountId;
	MapStringString * accountDetails;
	QListWidgetItem * item;
	QWidget * itemWidget;

	Account();

public:
	
	//Constructors
	static Account * buildExistingAccountFromId(QString _accountId);
	static Account * buildNewAccountFromAlias(QString alias);
	
	~Account();
	
	//Getters
	bool isNew();
	bool isChecked();
	QString & getAccountId();
	MapStringString & getAccountDetails();
	QListWidgetItem * getItem();
	QWidget * getItemWidget();
	QString getStateName(QString & state);
	QColor getStateColor();
	QString getStateColorName();
	QString getAccountDetail(QString & param);
	
	//Setters
	void setItemText(QString text);
	void initAccountItem();
	void setAccountId(QString id);
	void setAccountDetails(MapStringString m);
	void setAccountDetail(QString param, QString val);
	
	//Operators
	bool operator==(const Account&)const;
	
	
};



#endif