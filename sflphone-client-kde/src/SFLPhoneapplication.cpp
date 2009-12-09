#include "SFLPhoneapplication.h"


#include <KCmdLineArgs>
#include <KIconLoader>
#include <KStandardDirs>
#include <KNotification>
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

// test notif
  KNotification *notification = new KNotification( "contact online" );
  notification->setText( "text" );
  notification->sendEvent();

  // Start remaining initialisation
//  initializePaths();
//  initializeMainWindow();
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

  // We found session data for the Contact List, to restore it
  if( kapp->isSessionRestored() && restoredWindow != -1 )
  {
    sflphoneWindow_->restore( restoredWindow, false );
  }
  else
  {
    if( ! args->isSet( "hidden" ) )
    {
      sflphoneWindow_->show();
    }
  }
}



/**
 * Initialize additional paths
 */
void SFLPhoneApplication::initializePaths()
{
  // Add compile time paths as fallback
  //KGlobal::dirs()       -> addPrefix( SFLPHONE_PREFIX );
  //KIconLoader::global() -> addAppDir( SFLPHONE_PREFIX "/share" );

  // Test whether the prefix is correct.
  if( KGlobal::dirs()->findResource( "appdata", "icons/hi128-apps-sflphone-client-kde.png" ).isNull() )
  {
    kWarning() << "SFLPhone could not find resources in the search paths: "
               << KGlobal::dirs()->findDirs( "appdata", QString::null ).join(", ") << endl;
  }
}


#include "SFLPhoneapplication.moc"
