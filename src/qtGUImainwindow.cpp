/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *                                                                             
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                             
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                             
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


////////////////////////////////////////////////////////////////////////////////
// QtGUIMainWindow Implementation                                             //
////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>

#include <qbitmap.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qevent.h>
#include <qinputdialog.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qregexp.h>
#include <qsettings.h>
#include <qspinbox.h>
#include <qtimer.h>
#include <qtooltip.h>

#include "audiodrivers.h"
#include "configuration.h"
#include "configurationpanelui.h"
#include "global.h"
#include "jpushbutton.h"
#include "manager.h"
#include "numerickeypadtools.h"
#include "skin.h"
#include "qtGUImainwindow.h"

#define QCHAR_TO_STRIP	"-"
#define REG_EXPR		"(-|\\(|\\)| )"
	

///////////////////////////////////////////////////////////////////////////////
// Tray Icon implementation
///////////////////////////////////////////////////////////////////////////////
MyTrayIcon::MyTrayIcon(const QPixmap &icon, const QString &tooltip, 
		QPopupMenu *mypop, QObject *parent, const char *name)
		: TrayIcon (icon, tooltip, mypop, parent, name)
{
	menu = mypop;
}

void
MyTrayIcon::mousePressEvent (QMouseEvent *e) 
{
	switch ( e->button() ) {
		case RightButton:
			menu->popup( e->globalPos() );
			e->accept();
			break;
			
		case LeftButton:
			emit clickedLeft();
			break;
			
		case MidButton:
			break;
			
		default:
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// QtGUIMainWindow implementation
///////////////////////////////////////////////////////////////////////////////

/**
 * Default Constructor
 * Init, Connections
 */
QtGUIMainWindow::QtGUIMainWindow (QWidget *parent, const char *name, WFlags f, 
									Manager *mngr) 
									: TransQWidget (parent, name, f) {

	// Create configuration panel
	panel = new ConfigurationPanel (0, 0, false);

	// Address book dialog
	phonebook = new PhoneBook (0, 0, false);

	// URL input dialog
	urlinput = new URL_Input (this);
	
	// For managing 
	this->callmanager = mngr;

	// For DTMF
	key = new DTMF ();
	buf = new short[SIZEBUF];

	// Load file configuration skin
	QString skinfilename(Skin::getPath(QString(SKINDIR), setPathSkin(),
				QString(FILE_INI)));
	ExistingDF.SetFileName(skinfilename);
	ExistingDF.Load(skinfilename);
	
	// Initialisations
	this->initButtons();
	this->initBlinkTimer();

 	// Initialisation variables for ringing and message
	msgVar = false;
	b_dialtone = false;
	
	// Load background image phone
	setbgPixmap (new QPixmap (Skin::getPath(QString(SKINDIR), 
											setPathSkin(),
											QString(PIXMAP_PHONE))));
	
	// Transform pixmap to QImage
	setSourceImage ();

	this->setMaximumSize (getSourceImage().width(), getSourceImage().height());
	this->setGeometry (MAIN_INITIAL_POSITION, 
					   MAIN_INITIAL_POSITION, 
					   getSourceImage().width(),
					   getSourceImage().height());
		
	// Calculate just one time the transparency mask bit to bit
	transparencyMask ();

	// By default, keyboard mapping mode is numerical mode
	this->setMode(NUM_MODE);
	
	// Connect blinkTimer signals to blink slot
    connect(blinkTimer, SIGNAL(timeout()),this, SLOT(blinkMessageSlot()));
	connect (blinkTimer, SIGNAL(timeout()), this, SLOT(blinkRingSlot()) );
	connect (blinkTimer, SIGNAL(timeout()), this, SLOT(blinkLineSlot()));
	
	// Line pixmaps initialisation
	for (int i = 0; i < NUMBER_OF_LINES; i++) {
		for (int j = 0; j < NUMBER_OF_STATES; j++) {
	 		TabLinePixmap[i][j] = QPixmap(Skin::getPath(QString(SKINDIR),
												setPathSkin(), 
												QString(PIXMAP_LINE(i, j))));
	 	}
	}
	
	currentLineNumber = -1;
	onLine = currentLineNumber;
	callinprogress = false;
	chosenLine = -1;
	choose = false;
	noChoose = false;
	transfer = false;
	
	// Message pixmaps initialisation
	TabMsgPixmap[0] = QPixmap(Skin::getPath(QString(SKINDIR), setPathSkin(), 
						PIXMAP_MESSAGE_OFF));
	TabMsgPixmap[1] = QPixmap(Skin::getPath(QString(SKINDIR), setPathSkin(), 
						PIXMAP_MESSAGE_ON));
	
	// Create new display and numeric keypad
	lcd = new MyDisplay(this, 0, this);
	keypad = new NumericKeypad (this, NULL, Qt::WDestructiveClose |
                    					Qt::WStyle_Customize |
                    					Qt::WStyle_NoBorder);

	this->first = true;
	
	// Move 
	setMainLCD ();

	// Connect to append url in display
	connect (urlinput->buttonOK, SIGNAL(clicked()), this, SLOT(stripSlot()));

	// Connect to apply settings
	connect (panel->buttonApply, SIGNAL(clicked()), this, SLOT(applySlot()));
	
	// Handle keyboard events
	// Connect for clicked numeric keypad button 
	connect ((QObject*)keypad->key0, SIGNAL(clicked()), this, 
			SLOT(pressedKey0()));
	connect ((QObject*)keypad->key1, SIGNAL(clicked()), this, 
			SLOT(pressedKey1()));
	connect ((QObject*)keypad->key2, SIGNAL(clicked()), this, 
			SLOT(pressedKey2()));
	connect ((QObject*)keypad->key3, SIGNAL(clicked()), this, 
			SLOT(pressedKey3()));
	connect ((QObject*)keypad->key4, SIGNAL(clicked()), this, 
			SLOT(pressedKey4()));
	connect ((QObject*)keypad->key5, SIGNAL(clicked()), this, 
			SLOT(pressedKey5()));
	connect ((QObject*)keypad->key6, SIGNAL(clicked()), this, 
			SLOT(pressedKey6()));
	connect ((QObject*)keypad->key7, SIGNAL(clicked()), this, 
			SLOT(pressedKey7()));
	connect ((QObject*)keypad->key8, SIGNAL(clicked()), this, 
			SLOT(pressedKey8()));
	connect ((QObject*)keypad->key9, SIGNAL(clicked()), this, 
			SLOT(pressedKey9()));
	connect ((QObject*)keypad->keyStar, SIGNAL(clicked()), this, 
			SLOT(pressedKeyStar()));
	connect ((QObject*)keypad->keyHash, SIGNAL(clicked()), this, 
			SLOT(pressedKeyHash()));
	connect ((QObject*)keypad->keyClose, SIGNAL(clicked()), this, 
			SLOT(dtmfKeypad()));

	// Connections for the lines 
	connect (callmanager->phLines[0]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line0()));
	connect (callmanager->phLines[1]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line1()));
	connect (callmanager->phLines[2]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line2()));
	connect (callmanager->phLines[3]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line3()));
	connect (callmanager->phLines[4]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line4()));
	connect (callmanager->phLines[5]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line5()));

	// Misc 
	connect (phoneKey_msg, SIGNAL(clicked()), this, SLOT(button_msg()));
	connect (phoneKey_transf, SIGNAL(clicked()), this, SLOT(button_transfer()));
	connect (phoneKey_conf, SIGNAL(clicked()), this, SLOT(button_conf()));
	connect (dial_button, SIGNAL(clicked()), this, SLOT(dial()));
	connect (mute_button, SIGNAL(clicked()), this, SLOT(button_mute()));
	connect (hangup_button, SIGNAL(clicked()), this, SLOT(hangupLine()));
	connect (configuration_button,SIGNAL(clicked()),this,SLOT(configuration()));
	connect (addr_book_button, SIGNAL(clicked()), this,SLOT(addressBook()));
	connect (dtmf_button, SIGNAL(clicked()), this, SLOT(dtmfKeypad()));

	// Connect to reduce
	if (Config::getb(QString("Preferences/Options.checkedTray"))) {
		connect (reduce_button, SIGNAL(clicked()), this, SLOT(clickHandle()));
	} else {
		connect (reduce_button, SIGNAL(clicked()), this, SLOT(showMinimized()));
	}
	// Connect to quit with keyboard
	connect (this, SIGNAL(keyPressed(int)), this, SLOT(quitApplication()));
   	// Connect to quit with quit button
	connect (quit_button, SIGNAL(clicked()), this, SLOT(quitApplication()));
	
	// To register when program is launched
	if (Config::getb(QString("Preferences/Options.autoregister")) and
			panel->password->text() == "") { 
		configuration ();
	}

	// Change window title and Icon.
	this->setCaption(PROGNAME);
	this->setIcon(QPixmap(Skin::getPathPixmap(QString(PIXDIR), 
				QString(SFLPHONE_LOGO))));

	// Init ringing state (not ringing)
	//this->ring(false);

	// Show the GUI
	this->show();	

	mypop = new QPopupMenu(this);
	mypop->insertItem ("Quit", qApp, SLOT(quit()));

	trayicon = new MyTrayIcon(QPixmap(
				Skin::getPathPixmap(QString(PIXDIR), QString(TRAY_ICON))), 
				NULL, mypop, parent, name);
	trayicon->show();
	connect(trayicon, SIGNAL(clickedLeft()), this, SLOT(clickHandle()));
	
}

/**
 * Destructor
 */
QtGUIMainWindow::~QtGUIMainWindow(void) {
	delete  phoneKey_transf;
	delete  phoneKey_msg;
	delete  phoneKey_conf;
	delete  phoneKey_line0;
	delete  phoneKey_line1;
	delete  phoneKey_line2;
	delete  phoneKey_line3;
	delete  reduce_button;
	delete  quit_button;
	delete  addr_book_button;
	delete  configuration_button;
	delete  hangup_button;
	delete  dial_button;
	delete  mute_button;
	delete	dtmf_button;
	delete	vol_mic;
	delete 	vol_spkr;
	delete	panel;
	delete  blinkTimer;
	delete  keypad;
	delete	lcd;
	delete  urlinput;
	delete	callmanager;
	delete 	mypop;
	delete 	trayicon;

	for (int j = 0; j < NUMBER_OF_LINES; j++) {
		delete callmanager->phLines[j]->button();
	}
}



///////////////////////////////////////////////////////////////////////////////
// Private Methods implementations     	                               	     
///////////////////////////////////////////////////////////////////////////////
/**
 * Init and start blink timer for all blinking pixmap, with 500 ms timeout.
 */
void
QtGUIMainWindow::initBlinkTimer(void) {
	blinkTimer = new QTimer(this);
	blinkTimer->start(500);
}

/**
 * Init variable with skin choice
 */
QString
QtGUIMainWindow::setPathSkin (void) {
/*	QString		 pathskin;
	if (apply) {
		pathskin = panel->SkinChoice->currentText();
		apply = false;
	} else {
		pathskin = Config::get(QString("Preferences/Themes.skinChoice"),
				QString("default"));
	}
	return pathskin;*/
	return Config::get(QString("Preferences/Themes.skinChoice"),
		   QString("metal"));
}

/**
 * Inits all phonekey buttons.
 * Create new QPushButtons, set up tool tip, disable focus, set button geometry
 * set palette.
 */
void
QtGUIMainWindow::initButtons (void) {
	// Buttons initialisation
	phoneKey_msg= new JPushButton(this, NULL, "voicemail");
	phoneKey_transf = new JPushButton(this, NULL, "transfer");
	phoneKey_conf = new JPushButton(this, NULL, "conference");
	reduce_button = new JPushButton(this, NULL, "minimize");
	quit_button = new JPushButton(this, NULL, "close");
	addr_book_button = new JPushButton(this, NULL, "directory");
	configuration_button = new JPushButton(this, NULL, "setup");
	hangup_button = new JPushButton(this, NULL, "hangup");
	dial_button = new JPushButton(this, NULL, "ok");
	mute_button = new JPushButton(this, NULL, "mute");
	dtmf_button = new JPushButton(this, NULL, "dtmf");

	// Set tooltip buttons
	QToolTip::add(reduce_button, tr("Minimize window"));
	QToolTip::add(quit_button, tr("Close window (Ctrl+Q)"));
	QToolTip::add(phoneKey_msg, tr("Get your message"));
	QToolTip::add(phoneKey_transf, tr("Call transfer"));
	QToolTip::add(phoneKey_conf, tr("Conference"));
	QToolTip::add(addr_book_button, tr("Address book"));
	QToolTip::add(configuration_button, tr("Configuration tools (Ctrl+C)"));
	QToolTip::add(hangup_button, tr("Hangup"));
	QToolTip::add(dial_button, tr("Dial"));
	QToolTip::add(mute_button, tr("Mute"));
	QToolTip::add(dtmf_button, tr("Show DTMF keypad"));
	
	// Buttons position
	phoneKey_msg->move (
			ExistingDF.GetInt("msg_x","Positions"), 
			ExistingDF.GetInt("msg_y","Positions"));
	phoneKey_transf->move (
			ExistingDF.GetInt("transf_x","Positions"), 
			ExistingDF.GetInt("transf_y","Positions"));
	phoneKey_conf->move (
			ExistingDF.GetInt("conf_x","Positions"), 
			ExistingDF.GetInt("conf_y","Positions"));
	reduce_button->move (
			ExistingDF.GetInt("reduce_x","Positions"), 
			ExistingDF.GetInt("reduce_y","Positions"));
	addr_book_button->move (
			ExistingDF.GetInt("addr_book_x","Positions"), 
			ExistingDF.GetInt("addr_book_y","Positions"));
	quit_button->move (
			ExistingDF.GetInt("quit_x","Positions"), 
			ExistingDF.GetInt("quit_y","Positions"));		
	configuration_button->move (
			ExistingDF.GetInt("configuration_x","Positions"), 
			ExistingDF.GetInt("configuration_y","Positions"));
	hangup_button->move (
			ExistingDF.GetInt("hangup_x","Positions"), 
			ExistingDF.GetInt("hangup_y","Positions"));
	dial_button->move (
			ExistingDF.GetInt("dial_x","Positions"), 
			ExistingDF.GetInt("dial_y","Positions"));
	mute_button->move (
			ExistingDF.GetInt("mute_x","Positions"), 
			ExistingDF.GetInt("mute_y","Positions"));
	dtmf_button->move (
			ExistingDF.GetInt("dtmf_x","Positions"), 
			ExistingDF.GetInt("dtmf_y","Positions"));
				

	// Loop for line buttons
	//Initialisation, set no focus, set geometry, set palette, pixmap
	for (int j = 0; j < NUMBER_OF_LINES; j++) {
		QString lnum;

		lnum = "l" + lnum.setNum (j + 1);
		callmanager->phLines[j]->setButton(new JPushButton(
					this, NULL, lnum.ascii()));
		callmanager->phLines[j]->button()->move (
					ExistingDF.GetInt(lnum + "_x","Positions"), 
					ExistingDF.GetInt(lnum + "_y","Positions"));
	}

	// Set pixmaps volume TODO:change to thing like slider
	vol_mic = new JPushButton(this, NULL, "volume");
	vol_mic->move(
			ExistingDF.GetInt("vol_mic_x","Positions"), 
			ExistingDF.GetInt("vol_mic_y","Positions"));
	vol_spkr = new JPushButton(this, NULL, "volume");
	vol_spkr->move(
			ExistingDF.GetInt("vol_spkr_x","Positions"), 
			ExistingDF.GetInt("vol_spkr_y","Positions"));
}

/**
 * Returns true if the keyboard mapping returns letters. 
 *
 * @return	bool
 */
bool
QtGUIMainWindow::isInTextMode (void) {
	if (modeType == TEXT_MODE) {
		return true;
	} else {
		return false;
	}
}

/**
 * Returns true if the keyboard mapping returns digits. 
 *
 * @return	bool
 */
bool
QtGUIMainWindow::isInNumMode (void) {
	if (modeType == NUM_MODE) {
		return true;
	} else {
		return false;
	}
}

/**
 * Sets up the keyboard mapping mode.
 */
void
QtGUIMainWindow::setMode (int mode) {
	this->modeType = mode;
}

/**
 * Search the busy line number among all lines and different of current line
 * number.
 *
 * @return	number of busy line
 */
int
QtGUIMainWindow::numLineBusy(void) {
	int temp = -1;
	for (int i = 0; i < NUMBER_OF_LINES; i++) {
		if (callmanager->phLines[i]->isBusy() && i != currentLineNumber) {
			temp = i;	
		} 
	}
	return temp;
}

void
QtGUIMainWindow::stopBlinkingRingPixmap (void) {
	// Initialisation of red bar rectangle region
	QRect rect(ExistingDF.GetInt("ring_x","Positions"), 
			ExistingDF.GetInt("ring_y","Positions"),
			imageRing.width(), imageRing.height());

	// Blit the no-ringing pixmap with update rectangle
	bitBlt (this, ExistingDF.GetInt("ring_x","Positions"), 
			ExistingDF.GetInt("ring_y","Positions"),&imageNoRing, 0, 0,
			imageRing.width(), imageRing.height(), Qt::CopyROP);
	update(rect);
}

void
QtGUIMainWindow::dialTone (bool var) {
	if (this->b_dialtone != var) {
		this->b_dialtone = var;
	}
	callmanager->tonezone = var;
	callmanager->tone->toneHandle(ZT_TONE_DIALTONE); 
	
}
///////////////////////////////////////////////////////////////////////////////
// Public Methods implementations                                            //
///////////////////////////////////////////////////////////////////////////////

/**
 * Initializes LCD display, and move it at the configuration file position.
 *
 * @param	lcd
 */
void
QtGUIMainWindow::setMainLCD (void) {
	// Screen initialisation
	this->lcd->move (ExistingDF.GetInt("display_x","Positions"), 
					ExistingDF.GetInt("display_y","Positions"));
}

void
QtGUIMainWindow::setCurrentLineNumber(int newcur) {
	this->currentLineNumber = newcur;
}

/**
 * Sets the corresponding pixmap button according to its state.
 * Handle operations between lines (on hold, off hold) and dial tone 
 * 
 * @param	num_line: number of the current line
 */
void
QtGUIMainWindow::toggleLine (int num_line) {
	if ( num_line == -1 ){
		qDebug("Should not arrived !!!!!!!%d\n", num_line);
		return ;
	}
	//Current line number
	currentLineNumber = num_line;
	// Change state when click on line button
	callmanager->phLines[currentLineNumber]->toggleState();
	// If another line is busy
	busyNum = numLineBusy();

	if (callmanager->isNotUsedLine(currentLineNumber) and busyNum == -1) {
		qDebug("GUI: PREMIERE LIGNE occupee %d", currentLineNumber);
		lcd->setStatus("Enter Phone Number:");
		chosenLine = currentLineNumber;
		if (!noChoose) {
			choose = true; 
		}
		callmanager->phLines[currentLineNumber]->setStateLine(BUSY);
	} 

	// Occurs when newly off-hook line replaces another one.
	if (busyNum != currentLineNumber && busyNum != -1) {
		if (callmanager->isNotUsedLine(currentLineNumber)) {
			qDebug("GUI: Prend nouvelle ligne %d", currentLineNumber);
			lcd->clear(QString("Enter Phone Number:"));
			chosenLine = currentLineNumber;
			if (!noChoose) {
				choose = true;
			}	
		}
		// Change state to ONHOLD
		callmanager->phLines[busyNum]->setState(ONHOLD);
		callmanager->phLines[busyNum]->setStateLine(ONHOLD);
		callmanager->actionHandle (busyNum, ONHOLD_CALL);
			
		callmanager->phLines[currentLineNumber]->setState(BUSY);
		qDebug("GUI: state ON-HOLD line busyNum %d", busyNum);
	}

	if (callmanager->phLines[currentLineNumber]->isBusy()) {
		qDebug("GUI: isBusy line %d", currentLineNumber);
		// Change line button pixmap to "line in use" state.
		callmanager->phLines[currentLineNumber]->button()->setPixmap(
				TabLinePixmap[currentLineNumber][BUSY]);
		callmanager->phLines[currentLineNumber]->setState(BUSY);
		
		// Answer new call
		if (callmanager->isRingingLine(currentLineNumber) and 
			callmanager->phLines[currentLineNumber]->getStateLine() != ONHOLD){
			qDebug("GUI: -- Nouvel appel repondu %d --", currentLineNumber);
			callmanager->actionHandle (currentLineNumber, ANSWER_CALL);
			stopBlinkingRingPixmap ();
			callmanager->phLines[currentLineNumber]->setState(BUSY);
			callmanager->phLines[currentLineNumber]->setStateLine(BUSY);
		} 
		else if (callmanager->phLines[currentLineNumber]->getStateLine() 
																== ONHOLD) {
			qDebug("GUI: state OFF-HOLD line %d", currentLineNumber);
			lcd->clear(QString("Connected"));
			callmanager->phLines[currentLineNumber]->setStateLine(OFFHOLD);
			callmanager->actionHandle (currentLineNumber, OFFHOLD_CALL);
			lcd->appendText(callmanager->phLines[currentLineNumber]->text);
		}
	} 
	else if (callmanager->phLines[currentLineNumber]->isOnHold()){
		// Change state to ONHOLD
		callmanager->phLines[currentLineNumber]->setState(ONHOLD);
		callmanager->phLines[currentLineNumber]->setStateLine(ONHOLD);
		// Change status to ONHOLD
		qDebug("GUI: state ON-HOLD line %d", currentLineNumber);
		callmanager->actionHandle (currentLineNumber, ONHOLD_CALL);
	}
}

/**
 * Actions occur when click on hang off button. 
 * Use for validate incoming and outgoing call, transfer call.
 */
void
QtGUIMainWindow::dial (void) {
	int i = 0;
	if (transfer and callmanager->sip->call[currentLineNumber] != NULL
			and currentLineNumber != -1) {
		callmanager->actionHandle (currentLineNumber, TRANSFER_CALL);
		transfer = false;
	} else {
		qDebug("GUI: LINE CURRENT %d", currentLineNumber);
		// If new incoming call
		// Stop blinking ring pixmap and answer.
		if (callmanager->ringing()) {
			stopBlinkingRingPixmap();
			currentLineNumber = callmanager->newCallLineNumber();
			toggleLine (currentLineNumber);
		} else {
			// If new outgoing call
			// For new outgoing call with INVITE SIP request
			i = callmanager->outgoingNewCall();
			if (i == 0) {
				qDebug("Choix de la ligne (yes/no) %d",choose); 
				if (!choose) {
					noChoose = true;
					currentLineNumber = callmanager->findLineNumberNotUsedSIP();
				} else {
					currentLineNumber = chosenLine;
				}
				callmanager->phLines[currentLineNumber]->text = 
								callmanager->bufferTextRender();
				toggleLine (currentLineNumber);
				callinprogress = true;	

				// RingTone
				// TODO: callmanager->ringTone(true);
			}
		}	

		if (i == 0) {
			// Set used-state pixmap 'currentLineNumber' line
			callmanager->phLines[currentLineNumber]->button()->setPixmap(
						TabLinePixmap[currentLineNumber][BUSY]);
			callmanager->phLines[currentLineNumber]->setState(BUSY);
		}
	}
}

/**
 * Hangup the current call.
 */
void
QtGUIMainWindow::hangupLine (void) {
	qDebug("HANGUP: line %d", currentLineNumber);
	// If there is current line opened and state line not onHold
	if (currentLineNumber != -1 and 
			!(callmanager->phLines[currentLineNumber]->isOnHold())) {
		// set free pixmap
		setFreeStateLine (currentLineNumber);
		
		if (callinprogress) {
			callmanager->actionHandle (currentLineNumber, CANCEL_CALL);
		} else {
			// Hang up the call
			this->dialTone(false);
			callmanager->actionHandle (currentLineNumber, CLOSE_CALL);
		}
		lcd->clear(QString("Hung up"));
		setCurrentLineNumber(-1);
	}
	
	choose = false;
	noChoose = false;

	// Just to test when  receive a message
#if 0
	msgVar = true;
#endif
}

/**
 * Stop the blinking message slot and load the message-off button pixmap.
 */
void
QtGUIMainWindow::stopTimerMessage (void) {
	msgVar = false;	
	phoneKey_msg->setPixmap(TabMsgPixmap[FREE]);
}

/**
 * Set pixamp of free state.
 */
void
QtGUIMainWindow::setFreeStateLine (int line) {
	// Set free-status for current line
	callmanager->phLines[line]->setState (FREE);
	// Set free-pixmap
	callmanager->phLines[line]->button()->setPixmap( TabLinePixmap[line][FREE]);
}

/**
 * Stop the call timer.
 * 
 * @param	line: number of line 
 */
void
QtGUIMainWindow::stopCallTimer (int line) {
	// Stop the call timer when hang up
	if (callmanager->phLines[line]->timer != NULL) {
		callmanager->phLines[line]->stopTimer();
	}
	// No display call timer, display current hour
	lcd->inFunction = false;
	callmanager->phLines[line]->first = true;
}

/**
 * Start the call timer.
 * 
 * @param	line: number of line 
 */
void
QtGUIMainWindow::startCallTimer (int line) {
	// Call-timer enable
	lcd->inFunction = true;
	
	// To start the timer for display text just one time
	if (callmanager->phLines[line]->first) {
		callmanager->phLines[line]->startTimer();
		callmanager->phLines[line]->first = false;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Public slot implementations                                               //
///////////////////////////////////////////////////////////////////////////////
/**
 * Slot to blink with free and busy pixmaps when line is hold.
 */
void
QtGUIMainWindow::blinkLineSlot (void) {
	static bool isOn = false;
	int state = BUSY;
	
	if	(!isOn) {
		state = FREE;	
	}

	for (int i = 0; i < NUMBER_OF_LINES; i++) {
		// If lines are hold on, set blinking pixmap
		if (callmanager->phLines[i]->isOnHold()) {
			callmanager->phLines[i]->button()->setPixmap(TabLinePixmap[i][state]);
		}
	}
	isOn = !isOn;
}

// Dial the voicemail Number automatically when button is clicked
void
QtGUIMainWindow::button_msg (void) {
	 stopTimerMessage();
	 lcd->clear("Voicemail");
	 lcd->appendText(Config::gets("Preferences/Options.voicemailNumber"));
	 dial();
}

// Allow to enter a phone number to transfer the current call.
// This number is validated by ok-button or typing Enter
void
QtGUIMainWindow::button_transfer (void) {
	if (currentLineNumber != -1) {
		transfer = true;
		callmanager->actionHandle (currentLineNumber, ONHOLD_CALL);
		lcd->clear(QString("Transfer to:"));
	}
}

void
QtGUIMainWindow::button_conf (void) {
//TODO: This feature is not implemented yet
}

void
QtGUIMainWindow::button_line0 (void) {
	this->dialTone(true);
 	toggleLine (0);
	 
}

void
QtGUIMainWindow::button_line1 (void) {
	this->dialTone(true);
	toggleLine (1);
}

void
QtGUIMainWindow::button_line2 (void) {
	this->dialTone(true);
	toggleLine (2);
}

void
QtGUIMainWindow::button_line3 (void) {
	this->dialTone(true);
	toggleLine (3);
}

void
QtGUIMainWindow::button_line4 (void) {
	this->dialTone(true);
	toggleLine (4);
}

void
QtGUIMainWindow::button_line5 (void) {
	this->dialTone(true);
	toggleLine (5);
}

void
QtGUIMainWindow::button_mute(void) {
	// Disable micro sound 
	static bool isOn = true;
	
	if(!isOn) {
		callmanager->mute = false;	
		lcd->clear("Mute off");
	} else { 
		callmanager->mute = true;	
		lcd->clear("Mute on");		
	}
	
	isOn = !isOn;
}

// Show the setup panel
void
QtGUIMainWindow::configuration (void) {
	 panel->show();
}

void
QtGUIMainWindow::addressBook (void) {
//	TODO: phonebook->show();
	QMessageBox::information(this, "Information",
		"This feature is not implemented yet", QMessageBox::Yes);
}

// Handle the dtmf-button click
void
QtGUIMainWindow::dtmfKeypad (void) {
	if (keypad->isVisible()) {
		// Hide keypad if it's visible.
		keypad->hide();
	} else {
		
		if (this->first and !getMoved()) {
			// If it's the first time that keypad is shown.
			// The position is fixed with the main initial position.
			this->first = false;
			keypad->setGeometry (MAIN_INITIAL_POSITION + 
									this->getSourceImage().width(), 
							 	MAIN_INITIAL_POSITION, 
							 	keypad->getSourceImage().width(),
							 	keypad->getSourceImage().height());
		} else {
			// If main window is moved, we calculate the keypad new position 
			// to fix it with main window
			if (getMoved()) {
				keypad->setGeometry (positionOffsetX(), 
							 	getGlobalMouseY() - getMouseY(), 
							 	keypad->getSourceImage().width(),
							 	keypad->getSourceImage().height());
			}
			if (keypad->getMoved()) {
				// If keypad is moved, it shows at the previous position
				keypad->setGeometry (
								keypad->getGlobalMouseX()-keypad->getMouseX(), 
							 	keypad->getGlobalMouseY()-keypad->getMouseY(), 
							 	keypad->getSourceImage().width(),
							 	keypad->getSourceImage().height());
			}
		}

		// Show keypad if it's hidden.		
	 	keypad->show();
	}
}

// Get x-position offset, related to the screen size, to show numeric keypad
int
QtGUIMainWindow::positionOffsetX (void) {
	QRect screenRect;
	int offset;

	// Get the screen geometry
	screenRect = (QApplication::desktop())->screenGeometry (0);
	
	offset = this->getSourceImage().width() - getMouseX() + getGlobalMouseX();
	if (offset + keypad->getSourceImage().width() > screenRect.right()) {
		return getGlobalMouseX() - (
								getMouseX() + keypad->getSourceImage().width());
	} else {
		return offset;
	}
}

/**
 * Slot when receive a message, blink pixmap.
 */
void
QtGUIMainWindow::blinkMessageSlot (void) {
	static bool isOn = false;
	int stateMsg = BUSY;
	
	if(!isOn) {
		stateMsg = FREE;
	}
	
	if (msgVar) {
		phoneKey_msg->setPixmap(TabMsgPixmap[stateMsg]);
	}
	isOn = !isOn;
}


/**
 * Slot when phone is ringing, blink pixmap.
 */
void
QtGUIMainWindow::blinkRingSlot (void) {
	static bool isOn = false;
	int state = BUSY;
	int line = callmanager->sip->notUsedLine;
	if ( line == -1 ) 
		return;

	if (callmanager->ringing()) {
		// For the line
		if	(!isOn) {
			state = FREE;	
		}

		callmanager->phLines[line]->button()->setPixmap(
				TabLinePixmap[line][state]);
		isOn = !isOn;
	}
}


/**
 * Slot to quit application with or without QMessageBox confirmation
 */
void
QtGUIMainWindow::quitApplication (void) {
	bool confirm;
	// Show QMessageBox
	if (apply) {
		confirm = panel->confirmationToQuit->isChecked();
		apply = false;
	} else {
		confirm = Config::get (QString("Preferences/Options.confirmQuit"),true);
	}
	if (confirm) {
		if (QMessageBox::question(this, "Confirm quit",
			"Are you sure you want to quit SFLPhone ?",
			 QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
				QApplication::exit(0);
				callmanager->quitLibrary();
		}
	} 
	// No QMessageBox
	else {
		QApplication::exit(0);
		callmanager->quitLibrary();
		qDebug("QUIT");
	}
}

/**
 * Slot to strip urlinput.
 */
void
QtGUIMainWindow::stripSlot (void) {
	QRegExp rx(REG_EXPR);
	lcd->appendText(urlinput->url->text().remove(rx));
	urlinput->close();
}

/**
 * Slot when numeric keypad phonekey is pressed, append the key text and setup 
 * the DTMF keypad .
 *
 * @param	button group id 
 */ 
void
QtGUIMainWindow::pressedKeySlot (int id) {
	char code = 0;
	int pulselen = 0;
	
	// Stop dial tone
 	if (b_dialtone) {
		this->dialTone(false);
	}

	switch (id) {
		case KEYPAD_ID_0:		code = '0'; break;
		case KEYPAD_ID_1:		code = '1'; break;
		case KEYPAD_ID_2:		code = '2'; break;
		case KEYPAD_ID_3:		code = '3'; break;
		case KEYPAD_ID_4:		code = '4'; break;
		case KEYPAD_ID_5:		code = '5'; break;
		case KEYPAD_ID_6:		code = '6'; break;
		case KEYPAD_ID_7:		code = '7'; break;
		case KEYPAD_ID_8:		code = '8'; break;
		case KEYPAD_ID_9:		code = '9'; break;
		case KEYPAD_ID_STAR:	code = '*'; break;
		case KEYPAD_ID_HASH:	code = '#'; break;
	}

	onLine = currentLineNumber;
	if (onLine != -1) {
		callmanager->dtmf(onLine, code);
	} 
	
	lcd->appendText (code);

	// Handle dtmf
	key->startTone(code);
	key->generateDTMF(buf, SAMPLING_RATE);
	if (apply) {
		pulselen = panel->pulseLength->value();
		apply = false;
	} else {
		pulselen = Config::get(QString("Signalisations/DTMF.pulseLength"), 250);
	}
	callmanager->audiodriver->writeBuffer(buf, pulselen * (OCTETS/1000));
}

// Apply new settings
void 
QtGUIMainWindow::applySlot() {
	apply = true;
	
	panel->SkinChoice->setCurrentItem(panel->SkinChoice->currentItem());    
  	panel->confirmationToQuit->setChecked(
			panel->confirmationToQuit->isChecked());
   	panel->pulseLength->setValue(panel->pulseLength->value());
   	panel->zoneToneChoice->setCurrentItem(panel->zoneToneChoice->currentItem());
}

// Handle mouse left-button click to minimize/maximize the application
void
QtGUIMainWindow::clickHandle (void) {
	if (this->isShown()) {
		hide();
	}
	
	else if (this->isMinimized()) {
		showMaximized();
	}
	
	else {
		show();
	}
}

void
QtGUIMainWindow::pressedKey0 (void) {
	pressedKeySlot (KEYPAD_ID_0);
}

void
QtGUIMainWindow::pressedKey1 (void) {
	pressedKeySlot (KEYPAD_ID_1);
}

void
QtGUIMainWindow::pressedKey2 (void) {
	pressedKeySlot (KEYPAD_ID_2);
}
void
QtGUIMainWindow::pressedKey3 (void) {
	pressedKeySlot (KEYPAD_ID_3);
}
void
QtGUIMainWindow::pressedKey4 (void) {
	pressedKeySlot (KEYPAD_ID_4);
}
void
QtGUIMainWindow::pressedKey5 (void) {
	pressedKeySlot (KEYPAD_ID_5);
}
void
QtGUIMainWindow::pressedKey6 (void) {
	pressedKeySlot (KEYPAD_ID_6);
}
void
QtGUIMainWindow::pressedKey7 (void) {
	pressedKeySlot (KEYPAD_ID_7);
}
void
QtGUIMainWindow::pressedKey8 (void) {
	pressedKeySlot (KEYPAD_ID_8);
}
void
QtGUIMainWindow::pressedKey9 (void) {
	pressedKeySlot (KEYPAD_ID_9);
}
void
QtGUIMainWindow::pressedKeyStar  (void) {
	pressedKeySlot (KEYPAD_ID_STAR);
}
void
QtGUIMainWindow::pressedKeyHash (void) {
	pressedKeySlot (KEYPAD_ID_HASH);
}

///////////////////////////////////////////////////////////////////////////////
// Protected Methods implementations                                         //
///////////////////////////////////////////////////////////////////////////////
/**
 * Reimplementation of keyPressEvent() to handle the keyboard mapping.
 */
void
QtGUIMainWindow::keyPressEvent(QKeyEvent *e) {
	// Misc. key	  
	switch (e->key()) {
	case Qt::Key_At:
	case Qt::Key_Colon:
	case Qt::Key_Period:
	case Qt::Key_Comma:
	case Qt::Key_Plus:
	case Qt::Key_Minus:
	case Qt::Key_Slash:
		lcd->appendText(QChar(e->key()));
		return;
	   	break;

	case Qt::Key_Backspace:
		lcd->backspace();
		return;
	   	break;	

	case Qt::Key_Escape:
		lcd->clear();
		return;
	   	break;
	
	case Qt::Key_Return:
	case Qt::Key_Enter:
		dial();
		return;
		break;	

	case Qt::Key_F1:
	case Qt::Key_F2:
	case Qt::Key_F3:
	case Qt::Key_F4:
		this->toggleLine(e->key() - Qt::Key_F1);
		return;
	   	break;

	case Qt::Key_Q :
 		if (e->state() == Qt::ControlButton ) {
			emit keyPressed(e->key());
			return;			
		}			
		break;
	case Qt::Key_L :
 		if (e->state() == Qt::ControlButton ) {
		 	urlinput->show();
			return;			
		}			
		break;
	case Qt::Key_C :
 		if (e->state() == Qt::ControlButton ) {
		 	configuration();
			return;			
		}			
		break;
	case Qt::Key_Space:
		if (this->isInNumMode()) {
			this->setMode(TEXT_MODE);
		} else {
			this->setMode(NUM_MODE);
		}
		return;
		break;

	case Qt::Key_Alt:
	case Qt::Key_CapsLock:
	case Qt::Key_Shift:
	case Qt::Key_Tab:
	case Qt::Key_Control:
		return;
		break;

	default:
		break;
	 }

	if (QChar(e->key()).isDigit() ) {
		// Numeric keypad
		if (e->key() == Qt::Key_0) {
			pressedKeySlot(KEYPAD_ID_0);
		} else {
			pressedKeySlot(e->key() - Qt::Key_0 - 1);
		}
	}
	
	// Handle * and # too.
	else if ((e->key() == Qt::Key_Asterisk)
							or (e->key() == Qt::Key_NumberSign)) {
		(e->key() == Qt::Key_Asterisk) ? 
			pressedKeySlot(KEYPAD_ID_STAR)
			: pressedKeySlot(KEYPAD_ID_HASH);
	}
	
	// If letter keypad and numeric mode, display digit.
	else if (QChar(e->key()).isLetter() && this->isInNumMode() ) {
		pressedKeySlot(
			(NumericKeypadTools::keyToNumber(e->key())- Qt::Key_0) - 1);
	}
	
	// If letter keypad and text mode, display letter.
	else if (QChar(e->key()).isLetter() && this->isInTextMode()) {
		lcd->appendText(QChar(e->key()).lower());
	}  
}

// EOF
