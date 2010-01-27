#include "SFLPhoneapplication.h"


#include <KCmdLineArgs>
#include <KIconLoader>
#include <KStandardDirs>
#include <KNotification>
#include <KSystemTrayIcon>
#include <KMainWindow>
#include "SFLPhone.h"


/**
 * The application constructor
 */
SFLPhoneApplication::SFLPhoneApplication()
  : KApplication()
  , sflphoneWindow_(0)
//  , quitSelected_(false)
{
  // SFLPhoneApplication is created from main.cpp.
  // It continues the initialisation of the application.

  // Install a message handler, so KMESS_ASSERT won't do a exit(1) or abort()
  // It makes debugging output on Windows disappear, so don't use it there

  // Start remaining initialisation
  initializePaths();
  initializeMainWindow();
}



/**
 * Destructor
 */
SFLPhoneApplication::~SFLPhoneApplication()
{
  // automatically destroyed
  sflphoneWindow_ = 0;
}



/**
 * Return the sflphone window
 */
SFLPhone* SFLPhoneApplication::getSFLPhoneWindow() const
{
  return sflphoneWindow_;
}


/**
 * Initialisation of the main window.
 */
void SFLPhoneApplication::initializeMainWindow()
{
  // Fetch the command line arguments
  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

  // Enable KDE session restore.
  int restoredWindow = -1;
  if( kapp->isSessionRestored() )
  {
    int n = 0;
    while( KMainWindow::canBeRestored( ++n ) )
    {
      if( KMainWindow::classNameOfToplevel( n ) != QLatin1String( "SFLPhone" ) )
      {
        continue;
      }

      restoredWindow = n;
      break;
    }
  }

  // Create the main window and initialize it
  sflphoneWindow_ = new SFLPhone();
  if( ! sflphoneWindow_->initialize() )
  {
    exit(1);
    return;
  }

  // Initialize KApplication
  //setTopWidget( sflphoneWindow_ );
  sflphoneWindow_->show();
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

  // Test whether the prefix is correct.
  if( KGlobal::dirs()->findResource( "appdata", "icons/hi128-apps-sflphone-client-kde.png" ).isNull() )
  {
    kWarning() << "SFLPhone could not find resources in the search paths: "
               << KGlobal::dirs()->findDirs( "appdata", QString::null ).join(", ") << endl;
  }
}


#include "SFLPhoneapplication.moc"
