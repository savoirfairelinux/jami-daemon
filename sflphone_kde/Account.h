#ifndef HEADER_ACCOUNT
#define HEADER_ACCOUNT

#include <QtGui>
#include "metatypes.h"

const QString account_state_name(QString & s);

class Account{
	
private:

	QString * accountId;
	MapStringString * accountDetails;
	QListWidgetItem * item;

	Account();

public:
	
	//Constructors
	static Account * buildExistingAccountFromId(QString _accountId);
	static Account * buildNewAccountFromAlias(QString alias);
	
	~Account();
	
	//Getters
	bool isNew();
	QString & getAccountId();
	MapStringString & getAccountDetails();
	QListWidgetItem * getItem();
	QString getStateName(QString & state);
	QColor getStateColor();
	QString getStateColorName();
	QString getAccountDetail(QString & param);
	//QString getAccountDetail(std::string param);
	
	//Setters
	void initAccountItem();
	void setAccountId(QString id);
	void setAccountDetails(MapStringString m);
	//void setState(account_state_t s);
	void setAccountDetail(QString param, QString val);
	
	//Operators
	bool operator==(const Account&)const;
	
};



#endif