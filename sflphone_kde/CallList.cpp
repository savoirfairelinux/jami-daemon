#include "CallList.h"

CallList::CallList()
{
	callIdCpt = 0;
	calls = new QVector<Call *>();
}


Call * CallList::operator[](QListWidgetItem * item)
{
	for(int i = 0 ; i < size() ; i++)
	{
		if ((*calls)[i]->getItem() == item)
		{
			return (*calls)[i];
		}
	}
	return NULL;
}

QString CallList::getAndIncCallId()
{
	QString res = QString::number(callIdCpt);
	
	return res;
}

int CallList::size()
{
	return calls->size();
}

QListWidgetItem * CallList::addDialingCall()
{
	Call * call = Call::buildDialingCall(getAndIncCallId());
	calls->append(call);
	return call->getItem();
}