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

	QString getAndIncCallId();
	int size();


};


#endif