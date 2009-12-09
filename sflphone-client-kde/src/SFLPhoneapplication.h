#ifndef SFLPHONEAPPLICATION_H
#define SFLPHONEAPPLICATION_H


#include <KApplication>


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
    // Return true if quit was selected
    //bool         quitSelected() const;
    // Tell the application that quit was selected
    //void         setQuitSelected(bool quitSelected);

 // private slots:
 //void         slotAboutToQuit();
 //   void         slotLastWindowClosed();

  private:  // private methods
    void         initializeMainWindow();
    void         initializePaths();

  private:
    // Reference to the sflphone window
    SFLPhone       *sflphoneWindow_;
    // True when quit was selected
    //bool         quitSelected_;
};

#endif // SFLPHONEAPPLICATION_H
