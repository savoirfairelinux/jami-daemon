#include "Call.h"


Call::Call(call_state startState, QString callId)
{
	this->automate = new Automate(startState);
	this->id = callId;
	this->item = new QListWidgetItem("");
}

Call::~Call()
{
	delete item;
	delete automate;
}
	
Call * Call::buildDialingCall(QString callId)
{
	Call * call = new Call(CALL_STATE_DIALING, callId);
	return call;
}

QListWidgetItem * Call::getItem()
{
	return item;
}

call_state Call::getState() const
{
	return automate->getCurrentState();
}

call_state Call::action(call_action action, QString number)
{
	return automate->action(action, id, number);
}

QString Call::getCallId()
{
	return id;
}
