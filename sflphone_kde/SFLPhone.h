#ifndef SFLPHONE_H
#define SFLPHONE_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QListWidgetItem>
#include <QtGui/QKeyEvent>
#include <QErrorMessage>
#include <KXmlGuiWindow>

#include "ui_sflphone_kdeview_base.h"
#include "ConfigDialog.h"
#include "CallList.h"
#include "AccountWizard.h"
#include "Contact.h"
#include "sflphone_kdeview.h"


class ConfigurationDialog;
class sflphone_kdeView;

class SFLPhone : public KMainWindow
{

Q_OBJECT

private:
	sflphone_kdeView * view;



public:
	SFLPhone(QWidget *parent = 0);
	~SFLPhone();
	void setupActions();

};

#endif
 
