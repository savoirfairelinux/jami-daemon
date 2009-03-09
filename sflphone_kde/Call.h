#ifndef CALL_H
#define CALL_H


class Call
{
private:
	Account * account;
	QString id;
	CallStatus * status;
	QString from;
	QString to;
	HistoryState * historyState;
	QTime start;
	QTime stop;
	QListWidgetItem * item;


public:
	
	~Call();








};


#endif