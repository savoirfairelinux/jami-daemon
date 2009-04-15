#include "SFLPhone.h"


#include <KStandardAction>


SFLPhone::SFLPhone(QWidget *parent)
    : KMainWindow(parent),
      view(new sflphone_kdeView(this))
{
	// accept dnd
    setAcceptDrops(true);

    // tell the KXmlGuiWindow that this is indeed the main widget
    setCentralWidget(view);

    // then, setup our actions
    setupActions();

    // add a status bar
//     statusBar()->show();

    // a call to KXmlGuiWindow::setupGUI() populates the GUI
    // with actions, using KXMLGUI.
    // It also applies the saved mainwindow settings, if any, and ask the
    // mainwindow to automatically save settings if changed: window size,
    // toolbar position, icon size, etc.
//     setupGUI();

} 

SFLPhone::~SFLPhone()
{
}

void SFLPhone::setupActions()
{
//     KStandardAction::openNew(this, SLOT(fileNew()), actionCollection());
//     KStandardAction::quit(qApp, SLOT(quit()), actionCollection());

//     KStandardAction::preferences(this, SLOT(optionsPreferences()), actionCollection());

    // custom menu and menu item - the slot is in the class testkde4appfwView
//     KAction *custom = new KAction(KIcon("colorize"), i18n("Swi&tch Colors"), this);
//     actionCollection()->addAction( QLatin1String("switch_action"), custom );
//     connect(custom, SIGNAL(triggered(bool)), view, SLOT(switchColors()));
}


