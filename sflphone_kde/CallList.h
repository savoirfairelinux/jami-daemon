#ifndef CALL_LIST_H
#define CALL_LIST_H

#include "Call.h"

class CallList
{
private:
	QVector<Call *> * calls;
	int callIdCpt;

public:

	CallList();
	~CallList();

	Call * operator[](QListWidgetItem * item);

	QListWidgetItem * addDialingCall();
	QListWidgetItem * addIncomingCall(QString callId, QString from, Account & account);

	QString getAndIncCallId();
	int size();


};


#endif