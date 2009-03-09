#ifndef CALL_LIST_H
#define CALL_LIST_H


class CallList
{
private:
	QVector<Call *> * calls

public:
	
	~CallList();

	Call * operator[](QListWidgetItem * item);






};


#endif