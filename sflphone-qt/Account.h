#ifndef HEADER_ACCOUNT
#define HEADER_ACCOUNT



#include <QtGui>
#include "metatypes.h"

/** @enum account_state_t 
  * This enum have all the states an account can take.
  */
typedef enum
{
  /** Invalid state */
   ACCOUNT_STATE_INVALID = 0,
   /** The account is registered  */
   ACCOUNT_STATE_REGISTERED,   
   /** The account is not registered */
   ACCOUNT_STATE_UNREGISTERED,   
   /** The account is trying to register */
   ACCOUNT_STATE_TRYING, 
   /** Error state. The account is not registered */
   ACCOUNT_STATE_ERROR,
   /** An authentification error occured. Wrong password or wrong username. The account is not registered */
   ACCOUNT_STATE_ERROR_AUTH,
   /** The network is unreachable. The account is not registered */
   ACCOUNT_STATE_ERROR_NETWORK,
   /** Host is unreachable. The account is not registered */
   ACCOUNT_STATE_ERROR_HOST,
   /** Stun server configuration error. The account is not registered */
   ACCOUNT_STATE_ERROR_CONF_STUN,
   /** Stun server is not existing. The account is not registered */
   ACCOUNT_STATE_ERROR_EXIST_STUN

} account_state_t;

const QString account_state_name(account_state_t & s);

class Account{
	
private:

	QString * accountId;
	MapStringString * accountDetails;
	account_state_t * state;
	QListWidgetItem * item;

public:
	
	//Constructors
	Account(QListWidgetItem & _item, QString & alias);
	//Account(QString & _accountId, MapStringString & _accountDetails, account_state_t & _state);
	Account(QString & _accountId);
	~Account();
	
	//Getters
	QString & getAccountId();
	MapStringString & getAccountDetails();
	account_state_t & getState();
	QListWidgetItem & getItem();
	QString getStateName();
	QString getAccountDetail(QString & param);
	//QString getAccountDetail(std::string param);
	
	//Setters
	//void setAccountId(QString id);
	void setAccountDetails(MapStringString m);
	//void setState(account_state_t s);
	void setAccountDetail(QString param, QString val);
	
	//Operators
	bool operator==(const Account&)const;
	
};



#endif