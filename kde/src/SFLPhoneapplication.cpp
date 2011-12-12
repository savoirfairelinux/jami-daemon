#include "SFLPhoneapplication.h"


#include <KCmdLineArgs>
#include <KIconLoader>
#include <KStandardDirs>
#include <KNotification>
#include <KSystemTrayIcon>
#include <KMainWindow>
#include "lib/instance_interface_singleton.h"
#include "lib/configurationmanager_interface_singleton.h"
#include "lib/callmanager_interface_singleton.h"
#include "SFLPhone.h"


/**
 * The application constructor
 */
SFLPhoneApplication::SFLPhoneApplication()
  : KApplication()
  //, sflphoneWindow_(0)
{
  // SFLPhoneApplication is created from main.cpp.

  // Start remaining initialisation
  initializePaths();
  initializeMainWindow();
  connect(this,SIGNAL(aboutToQuit()),this,SLOT(quit2()));
}



/**
 * Destructor
 */
SFLPhoneApplication::~SFLPhoneApplication()
{
   // automatically destroyed
   //sflphoneWindow_ = 0;
   disableSessionManagement();
   InstanceInterface& instance              = InstanceInterfaceSingleton::getInstance();
   Q_NOREPLY instance.Unregister(getpid());
   instance.connection().disconnectFromBus(instance.connection().baseService());
}



/**
 * Return the sflphone window
 */
// SFLPhone* SFLPhoneApplication::getSFLPhoneWindow() const
// {
//   return sflphoneWindow_;
// }


/**
 * Initialisation of the main window.
 */
void SFLPhoneApplication::initializeMainWindow()
{
  // Fetch the command line arguments
  //KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

  // Enable KDE session restore.
//   int restoredWindow = -1;
//   if( kapp->isSessionRestored() ) {
//     int n = 0;
//     while( KMainWindow::canBeRestored( ++n ) ) {
//       if( KMainWindow::classNameOfToplevel( n ) != QLatin1String( "SFLPhone" ) ) {
//         continue;
//       }
// 
//       restoredWindow = n;
//       break;
//     }
//   }

  // Create the main window and initialize it
//   sflphoneWindow_ = new SFLPhone();
//   if( ! sflphoneWindow_->initialize() ) {
//     exit(1);
//     return;
//   }

  // Initialize KApplication
  //setTopWidget( sflphoneWindow_ );
//   sflphoneWindow_->show();
}



/**
 * Initialize additional paths
 */
void SFLPhoneApplication::initializePaths()
{
  // Add compile time paths as fallback
  KGlobal::dirs()       -> addPrefix( QString(DATA_INSTALL_DIR) );
  KIconLoader::global() -> addAppDir( QString(DATA_INSTALL_DIR) + "/share" );

  qDebug() << "KGlobal::dirs" << QString(DATA_INSTALL_DIR);
}

Q_NOREPLY void SFLPhoneApplication::quit2()
{
   
}

#include "SFLPhoneapplication.moc"
