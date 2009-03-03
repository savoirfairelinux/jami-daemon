class CheckState:public class IOption
{

private:
	
	void * (*getter());
	void * (*setter());
	bool checked;
	QCheckBox * checkBox;
	bool inverted;

public:

	CheckState(void * (*_getter()), void * (*_setter()), bool _checked, QCheckBox * _checkBox, bool _inverted = false):
		getter(_getter), setter(_setter), checked(_checked), checkBox(_checkBox), inverted(_inverted);

	virtual bool operator== (const CheckState & other);

	virtual ConfigurationDialog & operator<< ( ConfigurationDialog & cd, const CheckState & c);

	virtual DaemonInterface & operator<< (DaemonInterface & d, const CheckState & c);

	virtual ConfigurationDialog & operator>> ( ConfigurationDialog & cd, CheckState & c);

	virtual DaemonInterface & operator>> ( DaemonInterface & d, CheckState & c);


};