#ifndef CALL_H
#define CALL_H

#include "Automate.h"
//#include "Account.h"
//typedef call_state;

//class Automate;

class Call
{
private:
	//Account * account;
	QString id;
	QString from;
	QString to;
//	HistoryState * historyState;
	QTime start;
	QTime stop;
	QListWidgetItem * item;
	Automate * automate;

	Call(call_state startState, QString callId);

public:
	
	~Call();
	static Call * buildDialingCall(QString calllId);
	QListWidgetItem * getItem();
	call_state getState() const;
	QString getCallId();
	call_state action(call_action action, QString number = NULL);

};

#endif