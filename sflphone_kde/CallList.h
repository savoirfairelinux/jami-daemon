#ifndef CALL_LIST_H
#define CALL_LIST_H

#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtGui/QListWidgetItem>

#include "Call.h"

class CallList
{
private:
	QVector<Call *> * calls;
	int callIdCpt;

public:

	CallList();
	~CallList();

	Call * findCallByItem(const QListWidgetItem * item);
	Call * findCallByHistoryItem(const QListWidgetItem * item);
	Call * operator[](const QListWidgetItem * item);
	Call * operator[](const QString & callId);
	Call * operator[](int ind);

	Call * addDialingCall();
	Call * addIncomingCall(const QString & callId, const QString & from, const QString & account);
	Call * addRingingCall(const QString & callId);

	QString getAndIncCallId();
	int size();


};


#endif