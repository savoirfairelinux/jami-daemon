#include "ConfigDialog.h"

class IOption{

	virtual bool operator== (const IOption & other) = 0 ;

	virtual ConfigurationDialog & operator<< ( ConfigurationDialog & cd, const IOption & o) = 0;

	virtual DaemonInterface & operator<< ( DaemonInterface & d, const IOption & o) = 0;

	virtual ConfigurationDialog & operator>> ( ConfigurationDialog & cd, IOption & o) = 0;

	virtual DaemonInterface & operator>> ( DaemonInterface & d, IOption & o) = 0;

}