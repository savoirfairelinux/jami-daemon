#ifndef SFLPHONEAPPLICATION_H
#define SFLPHONEAPPLICATION_H


#include <KApplication>
#include <QDBusAbstractAdaptor>

class SFLPhone;


class SFLPhoneApplication : public KApplication
{
  Q_OBJECT

  public:
   // Constructor
   SFLPhoneApplication();

   // Destructor
   virtual    ~SFLPhoneApplication();

   // Return the contact list window
   SFLPhone*       getSFLPhoneWindow() const;

  private:  // private methods
    void         initializeMainWindow();
    void         initializePaths();

  private:
    // Reference to the sflphone window
    SFLPhone       *sflphoneWindow_;

  private slots:
     Q_NOREPLY void quit2();
};

#endif // SFLPHONEAPPLICATION_H
