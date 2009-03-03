#include "CheckState.h"


bool CheckState::operator== (const CheckState & other)
{
	return ( checked == other.checked );
}

ConfigurationDialog & CheckState::operator<< ( ConfigurationDialog & cd, const CheckState & c)
{
	if(inverted)
		checkBox->setCheckState(checked ? Qt::Unchecked : Qt::Checked);
	else
		checkBox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	
	
}

DaemonInterface & CheckState::operator<< (DaemonInterface & d, const CheckState & c)
{

}

ConfigurationDialog & CheckState::operator>> ( ConfigurationDialog & cd, CheckState & c)
{

}

DaemonInterface & CheckState::operator>> ( DaemonInterface & d, CheckState & c)
{

}
