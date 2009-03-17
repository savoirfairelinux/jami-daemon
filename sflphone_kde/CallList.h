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

	Call * operator[](const QListWidgetItem * item);
	Call * operator[](const QString & callId);
	void remove(Call * call);

	QListWidgetItem * addDialingCall();
	QListWidgetItem * addIncomingCall(const QString & callId, const QString & from, const QString & account);

	QString getAndIncCallId();
	int size();


};


#endif