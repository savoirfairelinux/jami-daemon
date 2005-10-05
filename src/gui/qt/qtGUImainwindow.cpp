/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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


#include <stdio.h>

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qevent.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qregexp.h>
#include <qspinbox.h>
#include <qtimer.h> 
#include <qtooltip.h>

#include "../../audio/audiolayer.h"
#include "../../audio/dtmf.h"
#include "../../audio/ringbuffer.h"
#include "../../error.h"
#include "../../global.h"
#include "../../manager.h"
#include "../../user_cfg.h"
#include "skin.h"
#include "configurationpanelui.h"
#include "jpushbutton.h"
#include "mydisplay.h"
#include "numerickeypad.h"
#include "numerickeypadtools.h"
#include "point.h"
#include "phoneline.h"
#include "qtGUImainwindow.h"
#include "vector.h"
#include "volumecontrol.h"

#define QCHAR_TO_STRIP	"-"
#define REG_EXPR		"(-|\\(|\\)| )"
	
using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Tray Icon implementation
///////////////////////////////////////////////////////////////////////////////
MyTrayIcon::MyTrayIcon(const QPixmap &icon, const QString &tooltip, 
		QPopupMenu *_mypop, QObject *parent, const char *name)
		: TrayIcon (icon, tooltip, _mypop, parent, name)
{
	menu = _mypop;
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
QtGUIMainWindow::QtGUIMainWindow (QWidget *parent, 
				  const char *name, 
				  WFlags f)
  : TransQWidget (parent, name, f), 
    GuiFramework() {	
  for (int i = 0; i < NUMBER_OF_LINES; i++) {
		phLines[i] = new PhoneLine();
		phLines[i]->setState(FREE);
		_TabIncomingCalls[i] = -1;
    }
	
	// Create configuration_panel
	_panel = new ConfigurationPanel (0, 0, false);
										
	// URL input dialog
	_urlinput = new URL_Input (this);
	

	// For DTMF
    _key = new DTMF ();
    _buf = new int16[SIZEBUF];

	// Create new display and numeric _keypad
	_lcd = new MyDisplay(this, 0, this);
	_keypad = new NumericKeypad (this, NULL, Qt::WDestructiveClose |
                    					Qt::WStyle_Customize |
                    					Qt::WStyle_NoBorder);
	_currentLine = -1;
	_chosenLine = -1;
	_prevLine = -1;
 	_first = true;
	_chooseLine = false;
	_transfer = false;
	_dialtone = false;
	_msgVar = false;
	_apply = false;

	// Initialisation of all that concern the skin
	initSkin();
	
	this->initBlinkTimer();

	
	// By default, keyboard mapping mode is numerical mode
	this->setMode(NUM_MODE);
	
	// Move 
	setMainLCD ();
	
	// Change window title and Icon.
	this->setCaption(PROGNAME);
	this->setIcon(QPixmap(Skin::getPathPixmap(QString(PIXDIR), 
				QString(SFLPHONE_LOGO))));

	// Show the GUI
	this->show();	

	// Handle the tray icon system menu
	_mypop = new QPopupMenu(this);
	_mypop->insertItem ("Compose", _urlinput, SLOT(show()));
	_mypop->insertItem ("Setup", this, SLOT(configuration()));
	_mypop->insertItem ("Quit", qApp, SLOT(quit()));

	_trayicon = new MyTrayIcon(QPixmap(
				Skin::getPathPixmap(QString(PIXDIR), QString(TRAY_ICON))), 
				NULL, _mypop, parent, name);
	_trayicon->show();
	
	// Connect to handle _trayicon
	connect(_trayicon, SIGNAL(clickedLeft()), this, SLOT(clickHandle()));
	// Connect _blinkTimer signals to blink slot
    connect(_blinkTimer, SIGNAL(timeout()),this, SLOT(blinkMessageSlot()));
	connect (_blinkTimer, SIGNAL(timeout()), this, SLOT(blinkRingSlot()) );
	connect (_blinkTimer, SIGNAL(timeout()), this, SLOT(blinkLineSlot()));
	// Connect to append url in display
	connect (_urlinput->buttonOK, SIGNAL(clicked()), this, SLOT(stripSlot()));
	// Connect to save settings
	connect (_panel->buttonSave, SIGNAL(clicked()), this, SLOT(save()));
	// Connect to apply skin
	connect (_panel->buttonApplySkin, SIGNAL(clicked()), this,SLOT(applySkin()));
	// Connect to register manually
	connect (_panel->Register, SIGNAL(clicked()), this, SLOT(registerSlot()));
}

/**
 * Destructor
 */
QtGUIMainWindow::~QtGUIMainWindow(void) {
	deleteButtons();
	delete	_panel;
	delete  _blinkTimer;
	delete  _keypad;
	delete	_lcd;
	delete  _urlinput;
	delete 	_mypop;
	delete 	_trayicon;
	delete  pt;
	delete 	_key;
	delete[] _buf;
	delete[] phLines;
}

void
QtGUIMainWindow::deleteButtons (void) {
	delete  phoneKey_transf;
	delete  phoneKey_msg;
	delete  phoneKey_conf;
	delete  reduce_button;
	delete  quit_button;
	delete  addr_book_button;
	delete  configuration_button;
	delete  hangup_button;
	delete  dial_button;
	delete  mute_button;
	delete	dtmf_button;
	delete  vol_mic;
    delete  vol_spkr;
    delete  micVolVector;
    delete  spkrVolVector;

	for (int j = 0; j < NUMBER_OF_LINES; j++) {
        delete phLines[j]->button();
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
	_blinkTimer = new QTimer(this);
	_blinkTimer->start(500);
}

/**
 * Init variable with skin choice
 */
QString
QtGUIMainWindow::setPathSkin (void) {
  std::string skinChoice = Manager::instance().getConfigString("Preferences",
"Themes.skinChoice");
  if ( skinChoice.empty() ) {
    skinChoice = "metal";
    Manager::instance().setConfig("Preferences", "Themes.skinChoice",
skinChoice);
  }
	return QString(skinChoice);
}

/**
 * Init variable with ring choice
 */
std::string
QtGUIMainWindow::ringFile(void) {
	return Manager::instance().getConfigString(AUDIO, RING_CHOICE);
}

/**
 * Get whole path for rings
 */
std::string
QtGUIMainWindow::getRingtoneFile (void) {
	string ringFilename(Skin::getPathRing(string(RINGDIR), ringFile()));
	return ringFilename;
}

void
QtGUIMainWindow::initSkin (void) {
	// Load file configuration skin
	string skinfilename(Skin::getPath(QString(SKINDIR), setPathSkin(),
				QString(FILE_INI)));
	
	if (!_apply) {
		this->pt = new Point(skinfilename);	
	} else {
		// If click on apply button
		delete pt;
		deleteButtons();
		this->pt = new Point(skinfilename);	
	}
	// Initialisation of the buttons
	initSpkrVolumePosition();
    initMicVolumePosition();
	initButtons();
	initVolume();
	// Connections of the buttons
	connections();
	
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

	// Line pixmaps initialisation
	for (int i = 0; i < NUMBER_OF_LINES; i++) {
		for (int j = 0; j < NUMBER_OF_STATES; j++) {
	 		TabLinePixmap[i][j] = QPixmap(Skin::getPath(QString(SKINDIR),
												setPathSkin(), 
												QString(PIXMAP_LINE(i, j))));
	 	}
	}
	// Message pixmaps initialisation
	TabMsgPixmap[0] = QPixmap(Skin::getPath(QString(SKINDIR), setPathSkin(), 
						PIXMAP_MESSAGE_OFF));
	TabMsgPixmap[1] = QPixmap(Skin::getPath(QString(SKINDIR), setPathSkin(), 
						PIXMAP_MESSAGE_ON));
}

void
QtGUIMainWindow::initSpkrVolumePosition (void) {
/*
    if (pt->getDirection(VOL_SPKR) == VERTICAL) {
        vol_spkr_x = Manager::("Audio", "Volume.speakers_x",
                pt->getX(VOL_SPKR));
        vol_spkr_y = Config::get("Audio", "Volume.speakers_y",
                pt->getVariation(VOL_SPKR));
    } else if (pt->getDirection(VOL_SPKR) == HORIZONTAL) {
        vol_spkr_x = Config::get("Audio", "Volume.speakers_x",
                pt->getX(VOL_SPKR) + pt->getVariation(VOL_SPKR));
        vol_spkr_y = Config::get("Audio", "Volume.speakers_y",
                pt->getY(VOL_SPKR));
    }
 // TODO: will have to calculate the volume with pourcentage
*/
}
                                                                                
void
QtGUIMainWindow::initMicVolumePosition (void) {
/*
  // TODO: will have to calculate the volume with pourcetage
    if (pt->getDirection(VOL_MIC) == VERTICAL) {
        vol_mic_x = Config::get("Audio", "Volume.micro_x", pt->getX(VOL_MIC));
        vol_mic_y = Config::get("Audio", "Volume.micro_y",
                pt->getVariation(VOL_MIC));
    } else if (pt->getDirection(VOL_MIC) == HORIZONTAL) {
        vol_mic_x = Config::get("Audio", "Volume.micro_x",
                pt->getX(VOL_MIC) + pt->getVariation(VOL_MIC));
        vol_mic_y = Config::get("Audio", "Volume.micro_y", pt->getY(VOL_MIC));
    }
*/
}

void
QtGUIMainWindow::initVolume (void) 
{
    // TODO: the manager already know the volume...
    //Manager::instance().setSpkrVolume(spkrVolVector->Y() - vol_spkr_y);
    //Manager::instance().setMicroVolume(micVolVector->Y() - vol_mic_y);
}

/**
 * Inits all phonekey buttons.
 * Create new QPushButtons, set up tool tip, disable focus, set button geometry
 * set palette.
 */
void
QtGUIMainWindow::initButtons (void) {
	// Buttons initialisation
	phoneKey_msg= new JPushButton(this, NULL, VOICEMAIL);
	phoneKey_transf = new JPushButton(this, NULL, TRANSFER);
	phoneKey_conf = new JPushButton(this, NULL, CONFERENCE);
	reduce_button = new JPushButton(this, NULL, MINIMIZE);
	quit_button = new JPushButton(this, NULL, CLOSE);
	addr_book_button = new JPushButton(this, NULL, DIRECTORY);
	configuration_button = new JPushButton(this, NULL, SETUP);
	hangup_button = new JPushButton(this, NULL, HANGUP);
	dial_button = new JPushButton(this, NULL, CONNECT);
	mute_button = new JPushButton(this, NULL, MUTE);
	dtmf_button = new JPushButton(this, NULL, DTMF_SHOW);

	// Set tooltip buttons
	QToolTip::add(reduce_button, tr("Minimize window"));
	QToolTip::add(quit_button, tr("Close window (Ctrl+Q)"));
	QToolTip::add(phoneKey_msg, tr("Get your message"));
	QToolTip::add(phoneKey_transf, tr("Call transfer (Ctrl+T)"));
	QToolTip::add(phoneKey_conf, tr("Conference"));
	QToolTip::add(addr_book_button, tr("Address book"));
	QToolTip::add(configuration_button, tr("Configuration tools (Ctrl+C)"));
	QToolTip::add(hangup_button, tr("Hangup (Esc)"));
	QToolTip::add(dial_button, tr("Dial (Enter)"));
	QToolTip::add(mute_button, tr("Mute (Ctrl+M)"));
	QToolTip::add(dtmf_button, tr("Show DTMF _keypad (Ctrl+D)"));

	// Buttons position
	phoneKey_msg->move (pt->getX(VOICEMAIL), pt->getY(VOICEMAIL));
	phoneKey_transf->move (pt->getX(TRANSFER), pt->getY(TRANSFER));
	phoneKey_conf->move (pt->getX(CONFERENCE), pt->getY(CONFERENCE));
	reduce_button->move (pt->getX(MINIMIZE), pt->getY(MINIMIZE));
	addr_book_button->move (pt->getX(DIRECTORY), pt->getY(DIRECTORY));
	quit_button->move (pt->getX(CLOSE), pt->getY(CLOSE));
	configuration_button->move (pt->getX(SETUP), pt->getY(SETUP));
	hangup_button->move (pt->getX(HANGUP), pt->getY(HANGUP));
	dial_button->move (pt->getX(CONNECT), pt->getY(CONNECT));
	mute_button->move (pt->getX(MUTE), pt->getY(MUTE));
	dtmf_button->move (pt->getX(DTMF_SHOW), pt->getY(DTMF_SHOW));
	
	// Loop for line buttons
	// Initialisation, set no focus, set geometry, set palette, pixmap
	for (int j = 0; j < NUMBER_OF_LINES; j++) {
		QString lnum;
   
		lnum = "l" + lnum.setNum (j + 1);
		phLines[j]->setButton(new JPushButton(
					this, NULL, lnum.ascii()));
		phLines[j]->button()->move (pt->getX(lnum),pt->getY(lnum));
	}
     
	// Set pixmaps volume
    micVolVector = new Vector(this, VOL_MIC, pt);
    spkrVolVector = new Vector(this, VOL_SPKR, pt);
                                                                               
    vol_mic = new VolumeControl(this, NULL, VOLUME, micVolVector);
    vol_spkr = new VolumeControl(this, NULL, VOLUME, spkrVolVector);
    vol_mic->move(vol_mic_x, vol_mic_y);
    vol_spkr->move(vol_spkr_x, vol_spkr_y);
}

void
QtGUIMainWindow::connections (void) {
	// Connect for clicked numeric _keypad button 
	connect ((QObject*)_keypad->key0, SIGNAL(clicked()), this, 
			SLOT(pressedKey0()));
	connect ((QObject*)_keypad->key1, SIGNAL(clicked()), this, 
			SLOT(pressedKey1()));
	connect ((QObject*)_keypad->key2, SIGNAL(clicked()), this, 
			SLOT(pressedKey2()));
	connect ((QObject*)_keypad->key3, SIGNAL(clicked()), this, 
			SLOT(pressedKey3()));
	connect ((QObject*)_keypad->key4, SIGNAL(clicked()), this, 
			SLOT(pressedKey4()));
	connect ((QObject*)_keypad->key5, SIGNAL(clicked()), this, 
			SLOT(pressedKey5()));
	connect ((QObject*)_keypad->key6, SIGNAL(clicked()), this, 
			SLOT(pressedKey6()));
	connect ((QObject*)_keypad->key7, SIGNAL(clicked()), this, 
			SLOT(pressedKey7()));
	connect ((QObject*)_keypad->key8, SIGNAL(clicked()), this, 
			SLOT(pressedKey8()));
	connect ((QObject*)_keypad->key9, SIGNAL(clicked()), this, 
			SLOT(pressedKey9()));
	connect ((QObject*)_keypad->keyStar, SIGNAL(clicked()), this, 
			SLOT(pressedKeyStar()));
	connect ((QObject*)_keypad->keyHash, SIGNAL(clicked()), this, 
			SLOT(pressedKeyHash()));
	connect ((QObject*)_keypad->keyClose, SIGNAL(clicked()), this, 
			SLOT(dtmfKeypad()));

	// Connections for the lines 
	connect (phLines[0]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line0()));
	connect (phLines[1]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line1()));
	connect (phLines[2]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line2()));
	connect (phLines[3]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line3()));
	connect (phLines[4]->button(), SIGNAL(clicked()), this, 
			SLOT(button_line4()));
	connect (phLines[5]->button(), SIGNAL(clicked()), this, 
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
	connect (reduce_button, SIGNAL(clicked()), this, SLOT(reduceHandle()));
	// Connect to quit with keyboard
	connect (this, SIGNAL(keyPressed(int)), this, SLOT(qt_quitApplication()));
   	// Connect to quit with quit button
	connect (quit_button, SIGNAL(clicked()), this, SLOT(qt_quitApplication()));

	// Connections for volume control
    connect(vol_spkr, SIGNAL(setVolumeValue(int)), this,
            SLOT(volumeSpkrChanged(int)));
    connect(vol_mic, SIGNAL(setVolumeValue(int)), this,
            SLOT(volumeMicChanged(int)));

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

short 
QtGUIMainWindow::isThereIncomingCall (void)
{
	for (int i = 0; i < NUMBER_OF_LINES; i++) {
		if (_TabIncomingCalls[i] > 0) {
			return _TabIncomingCalls[i];
		}
	}
	
	return -1;
}

short 
QtGUIMainWindow::isIncomingCall (int line)
{
	if (_TabIncomingCalls[line] > 0) {
		return 	_TabIncomingCalls[line];
	} else {
		return -1;
	}
}

int
QtGUIMainWindow::id2line (short id)
{
	int i;
	for (i = 0; i < NUMBER_OF_LINES; i++) {
		if (phLines[i]->getCallId() == id) {
			return i;
		}
	}
	if (i == NUMBER_OF_LINES) {
		_debug("Id is not attributed to a phoneline\n");
		return -1;
	}
	return -1;
}

short
QtGUIMainWindow::line2id (int line)
{
	short i;
	if (line != -1) {
		i = phLines[line]->getCallId();
	} else {
		return -1;
	}
	
	if (i > 0) {
		return i;
	} else {
		return -1;
	}
}

void 
QtGUIMainWindow::changeLineStatePixmap (int line, line_state state)
{
	// Set free-status for current line
    phLines[line]->setState(state);
    // Set free-pixmap
	if (state == ONHOLD) {
		// Because for the state of line pixmap there are just 2 states 
		// (FREE and BUSY), so we associate ONHOLD to BUSY
		state = BUSY;
	}
   	phLines[line]->button()->setPixmap(TabLinePixmap[line][state]);
}

int
QtGUIMainWindow::busyLineNumber (void)
{
	int temp = -1;
    for (int i = 0; i < NUMBER_OF_LINES; i++) {
        if (phLines[i]->isBusy() and i != _currentLine) {
            temp = i;
        }
    }
    return temp;
}

int
QtGUIMainWindow::putOnHoldBusyLine (int line)
{
	if (line != -1 and !phLines[line]->getbRinging()) {
		if (!getCall(line2id(line))->isRinging() and !getCall(line2id(line))->isProgressing()) {
			// Occurs when newly off-hook line replaces another one.
			_debug("On hold line %d [id=%d]\n", line, line2id(line));
			if (qt_onHoldCall(line2id(line)) != 1) {
				return -1;
			}
		} 
		changeLineStatePixmap(line, ONHOLD);
		return 1;
	} 
	return 0;
}

void
QtGUIMainWindow::dialtone (bool var) {
  if (var) {
    Manager::instance().playTone();
  }
}

void
QtGUIMainWindow::callIsRinging(int id, int line, int busyLine)
{
	changeLineStatePixmap(line, BUSY);
	putOnHoldBusyLine(busyLine);
	displayContext(id);
	if (getChooseLine()) {
		// If a free line is off-hook, set this line to free state
		setChooseLine(false);
		changeLineStatePixmap(getChosenLine(), FREE);
		dialtone(false);
	}
}

void
QtGUIMainWindow::callIsProgressing (int id, int line, int busyLine)
{
	// Same function of callIsRinging
	callIsRinging(id, line, busyLine);
}

int
QtGUIMainWindow::callIsBusy (Call* call, int id, int line, int busyLine)
{
	if(call->isAnswered() and getPrevLine() != line) {
	// If the current line is not the line which is answered
		_debug("CASE 3 Call %d already answered\n", id);
		changeLineStatePixmap(line, BUSY);
		putOnHoldBusyLine(busyLine);
		if (getChooseLine()) {
			// If a free line is off-hook, set this line to free state
			changeLineStatePixmap(getChosenLine(), FREE);
			dialtone(false);
		}
		peerAnsweredCall(id);
		displayContext(id);
	} else {
	// If call is busy, put this call on hold
		_debug("CASE 4 Put Call %d on-hold\n", id);
		changeLineStatePixmap(line, ONHOLD);
		displayStatus(ONHOLD_STATUS);
		if (qt_onHoldCall(id) != 1) {
		  Manager::instance().displayErrorText(id, "On-hold call failed !\n");
		  return -1;
		}
	}
	return 1;
}

int
QtGUIMainWindow::callIsOnHold(int id, int line, int busyLine)
{
	changeLineStatePixmap(line, BUSY);
	if (putOnHoldBusyLine(busyLine) == -1) {
		Manager::instance().displayErrorText(id, "Off-hold call failed !\n");
		return -1;
	}
	if (getChooseLine()) {
		// If a free line is off-hook, set this line to free state
		setChooseLine(false);
		dialtone(false);
		if (busyLine == -1) {
			changeLineStatePixmap(getChosenLine(), FREE);
		}
	}		
	_lcd->setInFunction(true);
	if (qt_offHoldCall(id) != 1) {
		Manager::instance().displayErrorText(id, "Off-hold call failed !\n");
		return -1;
	}

	displayContext(id);
	_lcd->setLenToShift(0);
	return 1;
}

int
QtGUIMainWindow::callIsIncoming (int id, int line, int busyLine)
{
	_TabIncomingCalls[line] = -1;
	changeLineStatePixmap(line, BUSY);
	putOnHoldBusyLine(busyLine);
	if (qt_answerCall(id) != 1) {
		Manager::instance().displayErrorText(id, "Answered call failed !\n");
		return -1;
	}
	return 1;
}

void
QtGUIMainWindow::clickOnFreeLine(int line, int busyLine)
{
	phLines[line]->button()->setPixmap(TabLinePixmap[line][BUSY]);
	displayStatus(ENTER_NUMBER_STATUS);
	setChooseLine(true);
	setChosenLine(line);

	if (!Manager::instance().getbCongestion()) {
		putOnHoldBusyLine(busyLine);
	} else {
		// When a new line is off-hook -> hangup the previous line 
		// which runs congestion tone 
		changeLineStatePixmap(busyLine, FREE);
		_lcd->clear(QString(ENTER_NUMBER_STATUS));
		Manager::instance().congestion(false);
		phLines[busyLine]->setCallId(0);
	}
	if (getPrevLine() != -1 and getPrevLine() != line 
			and phLines[getPrevLine()]->isFree()) {
		changeLineStatePixmap(getPrevLine(), FREE);
	}

	setPrevLine(line);	

	_lcd->setInFunction(false);
	_lcd->clearBuffer();
	dialtone(true);
}

////////////////////////////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////////////////////////////
Call* 
QtGUIMainWindow::getCall (short id)
{
	return Manager::instance().getCall(id);
}

int 
QtGUIMainWindow::associateCall2Line (short id)
{
	int i;

	if (getChooseLine()) {
		i = getCurrentLine();
		phLines[i]->setState(BUSY);
		phLines[i]->setCallId(id);
		return i;
	} else {
		for (i = 0 ; i < NUMBER_OF_LINES; i++) {
			if (phLines[i]->isFree()) {
				phLines[i]->setState(BUSY);
				phLines[i]->setCallId(id);
				return i;
			}
		}
		if (i == NUMBER_OF_LINES) {
			displayError("All the lines are busy");
			return -1;
		}
		return -1;
	}
}

PhoneLine* 
QtGUIMainWindow::getPhoneLine (short id)
{
	int i;
	for (i = 0; i < NUMBER_OF_LINES; i++) {
		if (phLines[i]->getCallId() == id) {
			return phLines[i];
		} 
	}
	if (i == NUMBER_OF_LINES) {
		_debug("getPhoneLine(id): Id %d is not attributed to a phoneline\n", id);
		return NULL;
	}
	return NULL;
}

int 
QtGUIMainWindow::getCurrentLine (void)
{
	return _currentLine;
}

void 
QtGUIMainWindow::setCurrentLine (int current)
{
	_currentLine = current;
}

bool
QtGUIMainWindow::isCurrentId (short id)
{
	if (line2id(getCurrentLine()) == id) {
		return true;
	} else {
		return false;
	}
}

int 
QtGUIMainWindow::getElapse (void)
{
	int line = getCurrentLine();
	return (phLines[line]->timer->elapsed() / 1000);
}

/////////////////////////////////////////////////////////////////////////////
// Reimplementation of virtual functions
/////////////////////////////////////////////////////////////////////////////

int 
QtGUIMainWindow::incomingCall (short id)
{
	int i;
	// Associate call id with a phoneline i.
	i = associateCall2Line(id);
	if (i >= 0) {
		_TabIncomingCalls[i] = id;
		_debug("Phoneline %d associated to id %d\n", i, id);
		if (getPhoneLine(id) != NULL) {
			_lcd->setInFunction(false);
			// Set boolean to true to blink pixmap to notify the ringing line 
			getPhoneLine(id)->setbRinging(true);
			// Set the status to the phoneline
			getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
		}
		
		// To store information about stop scrolling text
		_lcd->resetForScrolling (false);
		phLines[i]->setStopScrolling(false);
		// Set scrolling mode
		phLines[i]->setScrolling(true);
		_lcd->setIsScrolling(true);
	}
	
	return i;
}
	
void 
QtGUIMainWindow::peerAnsweredCall (short id)
{
	dialtone(false);
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
	// Afficher call-timer
	startCallTimer(id);
	Manager::instance().displayStatus(CONNECTED_STATUS);
	setChooseLine(false);
}

int 
QtGUIMainWindow::peerRingingCall (short id)
{
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
	return 1;
}

	
int 
QtGUIMainWindow::peerHungupCall (short id)
{
	int line = id2line(id);

	// To store information about scrolling text
	_lcd->resetForScrolling (true);
	phLines[line]->setStopScrolling(true);
	// Unset scrolling mode
	_lcd->setIsScrolling(false);
	phLines[getCurrentLine()]->setScrolling(false);

	if (line == getCurrentLine() or getCurrentLine() == -1) {
		stopCallTimer(id);
		Manager::instance().displayStatus(HANGUP_STATUS);
		setCurrentLine(-1);
	} else {
		// Stop the call timer when hang up
    	if (getPhoneLine(id)->timer != NULL) {
        	getPhoneLine(id)->stopTimer();
    	}
		getPhoneLine(id)->first = true;
	}
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
	changeLineStatePixmap(line, FREE);
	getPhoneLine(id)->setbRinging(false);
	getPhoneLine(id)->setCallId(0);
	setChooseLine(false);
	_TabIncomingCalls[line] = -1;
	
	return 1;
}

void 
QtGUIMainWindow::displayTextMessage (short id, const string& message)
{
	(void)id; // To remove warning message of unused parameter
	_lcd->clearBuffer();
	_lcd->appendText(message);
}
	
void 
QtGUIMainWindow::displayErrorText (short id, const string& message)
{
	_lcd->clearBuffer();
	_lcd->appendText(message);

	// Set scrolling mode
	_lcd->setIsScrolling(true);
	phLines[id2line(id)]->setScrolling(true);
}

void 
QtGUIMainWindow::displayError (const string& error)
{
	_lcd->clearBuffer();
	_lcd->appendText(error);
}
	
void 
QtGUIMainWindow::displayStatus (const string& status)
{
	if (status.compare(HANGUP_STATUS) == 0 or 
			status.compare(TRANSFER_STATUS) == 0) {
		_lcd->clearBuffer();
	}
	_lcd->setStatus(status);
}

void 
QtGUIMainWindow::displayContext (short id)
{
	displayStatus(getCall(id)->getStatus());
	
	// To fetch information about scrolling text according to the context 
	// of the phoneline.
	_lcd->setIsScrolling(phLines[id2line(id)]->scrolling());
	_lcd->resetForScrolling(phLines[id2line(id)]->stopScrolling());
	
	if (getCall(id)->isIncomingType()) {
		displayTextMessage (id, getCall(id)->getCallerIdName());
	} else if (getCall(id)->isOutgoingType()) {
		displayTextMessage (id, getCall(id)->getCallerIdNumber());
	} else {
		_debug("No call with id %d\n", id);
	}
}

void 
QtGUIMainWindow::setup (void) 
{
	configuration();
}

int
QtGUIMainWindow::selectedCall (void) 
{
	int id = -1;
	if (getChooseLine()) {
		id = line2id(getChosenLine());
	}
	return id;
}

////////////////////////////////////////////////////////////////////////////
// IP-phone user actions
////////////////////////////////////////////////////////////////////////////

int 
QtGUIMainWindow::qt_outgoingCall (void)
{
	int id;
	int line = -1;

	
	if (_lcd->getTextBuffer() == NULL) {
		Manager::instance().displayStatus(ENTER_NUMBER_STATUS);
		return -1;
	}
	const string to(_lcd->getTextBuffer().ascii());
	if (to.empty()) {
		Manager::instance().displayStatus(ENTER_NUMBER_STATUS);
		return -1;
	} 
	
	id = outgoingCall(to);
	if (id > 0) {	
	        line = associateCall2Line(id);
		if(line == -1) {
		  _debug("Call %d -> there's no available lines\n", id);
		  return -1;
		}
		_debug("Call %d -> line %d\n", id, line);
		

		// To store information about stop scrolling text
		_lcd->resetForScrolling (false);
		phLines[line]->setStopScrolling(false);

		setCurrentLine(line);
		displayStatus(TRYING_STATUS);
		Manager::instance().getCall(id)->setCallerIdNumber(to);
		changeLineStatePixmap(line, BUSY);
	} 
	
	return line;
}	
	
int 
QtGUIMainWindow::qt_hangupCall (short id)
{
	int i;
	i = hangupCall(id);
	stopCallTimer(id);	
	displayStatus(HANGUP_STATUS);
	setCurrentLine(-1);
	return i;
}	
	
int 
QtGUIMainWindow::qt_cancelCall (short id)
{
	int i;
	i = cancelCall(id);
	displayStatus(HANGUP_STATUS);
	setCurrentLine(-1);
	return i;
}

int 
QtGUIMainWindow::qt_answerCall (short id)
{
	int i;
	i = answerCall(id);
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
	startCallTimer(id);	
	displayStatus(CONNECTED_STATUS);
	getPhoneLine(id)->setbRinging(false);
	return i;	
}	
	
int 
QtGUIMainWindow::qt_onHoldCall (short id)
{
	int i;
	i = onHoldCall(id);
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
	return i;	
}	
	
int 
QtGUIMainWindow::qt_offHoldCall (short id)
{
	int i;
	i = offHoldCall(id);
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
	displayStatus(CONNECTED_STATUS);
	return i;	
}	
	
int 
QtGUIMainWindow::qt_transferCall (short id)
{
	int i;
	
	if (id != -1) {
		const string to(_lcd->getTextBuffer().ascii());;
		_debug("qt_transferCall: Transfer call %d to %s number\n", id, to.data());
		i = transferCall(id, to);
		getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
		return i;	
	} else {
		return 0;
	}
}	
	
void 
QtGUIMainWindow::qt_muteOn (short id)
{
	muteOn(id);
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));	
	displayStatus(MUTE_ON_STATUS);
}	
	
void 
QtGUIMainWindow::qt_muteOff (short id)
{
	muteOff(id);
	getPhoneLine(id)->setStatus(QString(getCall(id)->getStatus()));
	displayStatus(CONNECTED_STATUS);
}	
	
int 
QtGUIMainWindow::qt_refuseCall (short id)
{
	int i;
	i = refuseCall(id);
	displayStatus(HANGUP_STATUS);
	getPhoneLine(id)->setbRinging(false);
	_TabIncomingCalls[id2line(id)] = -1;
	setCurrentLine(-1);
	return i;	
}	
	
///////////////////////////////////////////////////////////////////////////////
// Public Methods implementations                                            //
///////////////////////////////////////////////////////////////////////////////

	
/**
 * Initializes LCD display, and move it at the configuration file position.
 */
void
QtGUIMainWindow::setMainLCD (void) {
	// Screen initialisation
	this->_lcd->move (pt->getX(SCREEN), pt->getY(SCREEN));
}


int
QtGUIMainWindow::toggleLine (int line) 
{
	int id;
	int busyLine;
	int ret = 1;
	
	Call* call;

	if (line == -1) {
		return -1;
	}
	
	setCurrentLine(line);
	busyLine = busyLineNumber();
	setTransfer(false);
	
	id = line2id(line);
	if (id > 0) {
	// If the call-id already exists
		call = getCall(id);
		if (call == NULL) {
		// Check if the call exists
			return -1;
		} else if (call->isRinging()) {
			// If call is ringing
			_debug("CASE 1: Call %d is ringing\n", id);
			callIsRinging(id, line, busyLine);
		} else if (!call->isIncomingType() and call->isProgressing()) {
			// If call is progressing
			_debug("CASE 2: Call %d is progressing\n", id);
			callIsProgressing (id, line, busyLine);
		} else if (call->isBusy()) {
			ret = callIsBusy (call, id, line, busyLine);
		} else if (call->isOnHold()) {
			// If call is on hold, put this call on busy state
			_debug("CASE 5: Put Call %d off-hold\n", id);
			ret = callIsOnHold(id, line, busyLine);
		} else if (call->isIncomingType()) {
		// If incoming call occurs
			_debug("CASE 6: Answer call %d\n", id);
			ret = callIsIncoming (id, line, busyLine);
		} else {
			_debug("Others cases to handle\n");
			ret = -1;
		}	
		setPrevLine(line);
	} else {
	// If just click on free line
		_debug("CASE 7: New line off-hook\n");
		clickOnFreeLine(line, busyLine);
	}
	return ret;
}

/**
 * Actions occur when click on hang off button. 
 * Use to validate incoming and outgoing call, transfer call.
 */
void
QtGUIMainWindow::dial (void) 
{
	short i;
	int line = -1;
	
	if ((i = isThereIncomingCall()) > 0) {
		// If new incoming call 
		line = id2line(i);
		if (line != -1) {
			_TabIncomingCalls[line] = -1;
			toggleLine(line);
		} else {
			return;
		}
	} else if (getTransfer()){
		// If call transfer
		setTransfer(false);
		int id = line2id(getCurrentLine());
		if(qt_transferCall (id) != 1) {
			Manager::instance().displayErrorText(id, "Transfer failed !\n");
		}
	} else {
		// If new outgoing call  
		if (getCurrentLine() < 0 or getChooseLine()) {
			line = qt_outgoingCall();
			if (line == -1) {
				return;
			} else {
				setPrevLine(line);
			}
		}
	}		
}

/**
 * Hangup the current call.
 */
void
QtGUIMainWindow::hangupLine (void) 
{  
	int i;
	int line = getCurrentLine();
	int id = phLines[line]->getCallId();

	setTransfer(false);

	// To store information about stop scrolling text
	_lcd->resetForScrolling (true);
	phLines[line]->setStopScrolling(true);
	// Unset scrolling mode
	_lcd->setIsScrolling(false);
	phLines[getCurrentLine()]->setScrolling(false);

	if (Manager::instance().getbCongestion() and line != -1) {
		// If congestion tone
		if (id > 0 and qt_hangupCall(id)) {
			changeLineStatePixmap(line, FREE);
			_lcd->clear(QString(ENTER_NUMBER_STATUS));
			Manager::instance().congestion(false);
			phLines[line]->setCallId(0);
		} else if (id == 0) {
			changeLineStatePixmap(line, FREE);
		} 
	} else if ((i = isThereIncomingCall()) > 0){
		// To refuse new incoming call 
		_debug("Refuse call %d\n", id);
		if (!qt_refuseCall(i)) {
			Manager::instance().displayErrorText(id, "Refused call failed !\n");
		}
		changeLineStatePixmap(id2line(i), FREE);
		phLines[id2line(i)]->setCallId(0);
	} else if (line >= 0 and id > 0 and getCall(id)->isProgressing()) {
		// If I want to cancel a call before ringing.
		if (!qt_cancelCall(id)) {
			Manager::instance().displayErrorText(id, "Cancelled call failed !\n");
		}
		changeLineStatePixmap(line, FREE);
		phLines[line]->setCallId(0);
		setChooseLine(false);
	} else if (line >= 0 and id > 0) {
		// If hangup current line normally
		_debug("Hangup line %d\n", line);
		if (!qt_hangupCall(id)) {
			Manager::instance().displayErrorText(id, "Hangup call failed !\n");
		}
		changeLineStatePixmap(line, FREE);
		phLines[line]->setCallId(0);
		setChooseLine(false);
	} else if (line >= 0) {
		_debug("Just load free pixmap for the line %d\n", line);
		changeLineStatePixmap(line, FREE);
		dialtone(false);
		setChooseLine(false);
		setCurrentLine(-1);
	} 
}

/**
 * Stop the blinking message slot and load the message-off button pixmap.
 */
void
QtGUIMainWindow::stopVoiceMessageNotification (void) {
   	_msgVar = false;
    phoneKey_msg->setPixmap(TabMsgPixmap[FREE]);
}

void
QtGUIMainWindow::startVoiceMessageNotification (void) {
   	_msgVar = true;
}

/**
 * Stop the call timer.
 * 
 * @param	line: number of line 
 */
void
QtGUIMainWindow::stopCallTimer (short id) {
	// Stop the call timer when hang up
    if (getPhoneLine(id)->timer != NULL) {
        getPhoneLine(id)->stopTimer();
    }
    // No display call timer, display current hour
	_lcd->setInFunction(false);
    getPhoneLine(id)->first = true;
}

/**
 * Start the call timer.
 * 
 * @param	line: number of line 
 */
void
QtGUIMainWindow::startCallTimer (short id) {
	// Call-timer enable
    _lcd->setInFunction(true);
                                                                                
    // To start the timer for display text just one time
    if (getPhoneLine(id)->first) {
        getPhoneLine(id)->startTimer();
        getPhoneLine(id)->first = false;
    }
} 

///////////////////////////////////////////////////////////////////////////////
// Public slot implementations                                               //
///////////////////////////////////////////////////////////////////////////////
void
QtGUIMainWindow::volumeSpkrChanged (int val) {
	Manager::instance().setSpkrVolume(val);
}

void
QtGUIMainWindow::volumeMicChanged (int val) {
	Manager::instance().setMicroVolume(val);
}

void
QtGUIMainWindow::registerSlot (void) {
	_panel->saveSlot();	
	registerVoIPLink();
}

/**
 * Slot to blink with free and busy pixmaps when line is hold.
 */
void
QtGUIMainWindow::blinkLineSlot (void) {
    static bool isOn = false;
    int state = BUSY;                                                                                 
    if  (!isOn) {
        state = FREE;
    }
                                                                                
    for (int i = 0; i < NUMBER_OF_LINES; i++) {
        // If lines are hold on, set blinking pixmap
        if (phLines[i]->isOnHold()) {
            phLines[i]->button()->setPixmap(TabLinePixmap[i][state]);
        }
    }
    isOn = !isOn;
}

// Dial the voicemail Number automatically when button is clicked
void
QtGUIMainWindow::button_msg (void) {
     stopVoiceMessageNotification();
	 _lcd->clearBuffer();
     _lcd->appendText(Manager::instance().getConfigString(PREFERENCES,
VOICEMAIL_NUM));
	 if (qt_outgoingCall() == -1) {
		 return;
	 }
}

// Allow to enter a phone number to transfer the current call.
// This number is validated by ok-button or typing Enter
void
QtGUIMainWindow::button_transfer (void) {
	int line_num = getCurrentLine();
    if (line_num != -1 and phLines[line_num]->isBusy()
			and !Manager::instance().getbCongestion()) {
		setTransfer(true);
		onHoldCall(line2id(getCurrentLine()));
		displayStatus(TRANSFER_STATUS);
    }
}

void
QtGUIMainWindow::button_conf (void) {
//TODO: This feature is not implemented yet
	QMessageBox::information(this, "Conference",
		"This feature is not implemented yet", QMessageBox::Yes);
}

void
QtGUIMainWindow::button_line0 (void) {
 	toggleLine (0);
	 
}

void
QtGUIMainWindow::button_line1 (void) {
	toggleLine (1);
}

void
QtGUIMainWindow::button_line2 (void) {
	toggleLine (2);
}

void
QtGUIMainWindow::button_line3 (void) {
	toggleLine (3);
}

void
QtGUIMainWindow::button_line4 (void) {
	toggleLine (4);
}

void
QtGUIMainWindow::button_line5 (void) {
	toggleLine (5);
}

void
QtGUIMainWindow::button_mute(void) 
{
   // Disable micro sound
    static bool isOn = true;
	
	int id = line2id(getCurrentLine());
	
	if (id != -1 and Manager::instance().getNumberOfCalls() > 0) {
    // If there is at least a pending call
        if(!isOn) {
			qt_muteOff(id);
        } else {
			qt_muteOn(id);
        }                                                                       
        isOn = !isOn;
    }
}

// Show the setup _panel
void
QtGUIMainWindow::configuration (void) {
	 _panel->show();
}

void
QtGUIMainWindow::addressBook (void) {
	QMessageBox::information(this, "Directory",
		"This feature is not implemented yet", QMessageBox::Yes);
}

// Handle the dtmf-button click
void
QtGUIMainWindow::dtmfKeypad (void) {
	if (_keypad->isVisible()) {
        // Hide _keypad if it's visible.
        _keypad->hide();
    } else {
                                                                                
        if (_first and !getMoved()) {
            // If it's the first time that _keypad is shown.
            // The position is fixed with the main initial position.
            _first = false;
            _keypad->setGeometry (MAIN_INITIAL_POSITION +
                                    this->getSourceImage().width(),
                                MAIN_INITIAL_POSITION,
                                _keypad->getSourceImage().width(),
                                _keypad->getSourceImage().height());
        } else {
            // If main window is moved, we calculate the _keypad new position
            // to fix it with main window
            if (getMoved()) {
                _keypad->setGeometry (positionOffsetX(),
                                getGlobalMouseY() - getMouseY(),
                                _keypad->getSourceImage().width(),
                                _keypad->getSourceImage().height());
            }
            if (_keypad->getMoved()) {
                // If _keypad is moved, it shows at the previous position
                _keypad->setGeometry (
                                _keypad->getGlobalMouseX()-_keypad->getMouseX(),
                                _keypad->getGlobalMouseY()-_keypad->getMouseY(),
                                _keypad->getSourceImage().width(),
                                _keypad->getSourceImage().height());
            }
        }
                                                                                
        // Show _keypad if it's hidden.
        _keypad->show();
    }

}

// Get x-position offset, related to the screen size, to show numeric _keypad
int
QtGUIMainWindow::positionOffsetX (void) {
	QRect screenRect;
	int offset;

	// Get the screen geometry
	screenRect = (QApplication::desktop())->screenGeometry (0);
	
	offset = this->getSourceImage().width() - getMouseX() + getGlobalMouseX();
	if (offset + _keypad->getSourceImage().width() > screenRect.right()) {
		return getGlobalMouseX() - (
								getMouseX() + _keypad->getSourceImage().width());
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
                                                                                
    if (_msgVar) {
        phoneKey_msg->setPixmap(TabMsgPixmap[stateMsg]);
    }
    isOn = !isOn;
}


/**
 * Slot when phone is ringing, blink pixmap.
 */
void
QtGUIMainWindow::blinkRingSlot (void) 
{
    static bool isOn = false;
    int state = BUSY;
	int i;

	if (isThereIncomingCall() != -1) {
		// For the line
		if  (!isOn) {
			state = FREE;
		}
		for (i = 0; i < NUMBER_OF_LINES; i++) {
			if (isIncomingCall(i) != -1 and phLines[i]->getbRinging()) {
				phLines[i]->button()->setPixmap(TabLinePixmap[i][state]);
			}
		}
		isOn = !isOn;
	}
}

/**
 * Slot to quit application with or without QMessageBox confirmation
 */
void
QtGUIMainWindow::qt_quitApplication (void) 
{
	// Save volume positions
    // TODO: save position if direction is horizontal
/*
    Config::set("Audio", "Volume.speakers_x",  pt->getX(VOL_SPKR));
    if (vol_spkr->getValue() != 0) {
        Config::set("Audio", "Volume.speakers_y",  pt->getY(VOL_SPKR) -
            vol_spkr->getValue());
    }
    Config::set("Audio", "Volume.micro_x",  pt->getX(VOL_MIC));
    if (vol_mic->getValue() != 0) {
        Config::set("Audio", "Volume.micro_y",  pt->getY(VOL_MIC) -
            vol_mic->getValue());
    }
*/
    // Save current position of the controls volume
    save();
	
	if (Manager::instance().getConfigInt(PREFERENCES, CONFIRM_QUIT)) {
		// If message-box
		if (QMessageBox::question(this, "Confirm quit",
		"Are you sure you want to quit SFLPhone ?",
		 QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {		
			QApplication::exit(0);
			if (!quitApplication()) {
				_debug("Your data didn't be saved\n");
			}
		}
	} else {
		// If no message-box
		QApplication::exit(0);
		if (!quitApplication()) {
			_debug("Your data didn't be saved\n");
		}
	}
}

/**
 * Slot to strip _urlinput.
 */
void
QtGUIMainWindow::stripSlot (void) {
    QRegExp rx(REG_EXPR);
    _lcd->appendText(_urlinput->url->text().remove(rx));
    _urlinput->close();
	dial();
}

/**
 * Slot when numeric _keypad phonekey is pressed, append the key text and setup 
 * the DTMF _keypad .
 *
 * @param	button group id 
 */ 
void
QtGUIMainWindow::pressedKeySlot (int id) {
	char code = 0;
	int callid;
                                                                                
    // Stop dial tone because we started dialing.
    if (_dialtone) {
        dialtone(false);
    }
                                                                                
    switch (id) {
        case KEYPAD_ID_0:       code = '0'; break;
        case KEYPAD_ID_1:       code = '1'; break;
        case KEYPAD_ID_2:       code = '2'; break;
        case KEYPAD_ID_3:       code = '3'; break;
        case KEYPAD_ID_4:       code = '4'; break;
        case KEYPAD_ID_5:       code = '5'; break;
        case KEYPAD_ID_6:       code = '6'; break;
        case KEYPAD_ID_7:       code = '7'; break;
        case KEYPAD_ID_8:       code = '8'; break;
        case KEYPAD_ID_9:       code = '9'; break;
        case KEYPAD_ID_STAR:    code = '*'; break;
        case KEYPAD_ID_HASH:    code = '#'; break;
    }
       
	callid = line2id(getCurrentLine());
    if (callid != -1 and getCall(callid)->isBusy()) {
	// To send DTMF during call, no display of them
        sendDtmf(callid, code); 
    } else if (Manager::instance().isDriverLoaded()
			and Manager::instance().error()->getError() == 0) {
	// To compose, phone number appears in the screen if the driver is loaded
	// and there is no error in configuration setup
		_lcd->appendText (code);

		// Unset scrolling mode
		_lcd->setIsScrolling(false);
		phLines[getCurrentLine()]->setScrolling(false);
	}
	// To generate the dtmf if there is no error in configuration
	if (Manager::instance().error()->getError() == 0) {
    Manager::instance().playDtmf(code);
  }
}

// Save settings in config-file
void 
QtGUIMainWindow::save() {
	saveConfig();
}

void
QtGUIMainWindow::applySkin (void) {
	_apply = true;
    // For skin of the screen
    _lcd->initGraphics();
    setMainLCD();
    // For skin of the gui
    initSkin();
}


// Handle operation to minimize the application
void 
QtGUIMainWindow::reduceHandle (void) {
	if (Manager::instance().getConfigInt(PREFERENCES,
CHECKED_TRAY)) {
        clickHandle();
    } else {
        showMinimized();
    }
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
		_lcd->appendText(QChar(e->key()));
		return;
	   	break;

	case Qt::Key_Backspace:
		_lcd->backspace();
		return;
	   	break;	

	case Qt::Key_Escape:
		hangupLine();
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
	case Qt::Key_F5:
	case Qt::Key_F6:
		this->toggleLine(e->key() - Qt::Key_F1);
		return;
	   	break;
		
	// To clear the screen	
	case Qt::Key_L:
 		if (e->state() == Qt::ControlButton ) {		
			_lcd->clear();
			return;
		}
	   	break;
	// To quit the application
	case Qt::Key_Q :
 		if (e->state() == Qt::ControlButton ) {
			emit keyPressed(e->key());
			return;			
		}			
		break;

	// To open input line
	case Qt::Key_O :
 		if (e->state() == Qt::ControlButton ) {
		 	_urlinput->show();
			return;			
		}			
		break;

	// To show window setup
	case Qt::Key_C :
 		if (e->state() == Qt::ControlButton ) {
		 	configuration();
			return;			
		}			
		break;

	// To show/hide dtmf-keypad
	case Qt::Key_D :
 		if (e->state() == Qt::ControlButton ) {
		 	dtmfKeypad();
			return;			
		}			
		break;

	// To set mode (text/num)
	case Qt::Key_Space:
		if (this->isInNumMode()) {
			this->setMode(TEXT_MODE);
		} else {
			this->setMode(NUM_MODE);
		}
		return;
		break;

	// To put mute on/off the mike sound
	case Qt::Key_M :
	if (e->state() == Qt::ControlButton ) {
		button_mute();
		return;			
	}			
	break;

	// To transfer call
	case Qt::Key_T :
	if (e->state() == Qt::ControlButton ) {
		button_transfer();
		return;			
	}			
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
		// Numeric _keypad
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
	
	// If letter _keypad and numeric mode, display digit.
	else if (QChar(e->key()).isLetter() && this->isInNumMode() ) {
		pressedKeySlot(
			(NumericKeypadTools::keyToNumber(e->key())- Qt::Key_0) - 1);
	}
	
	// If letter _keypad and text mode, display letter.
	else if (QChar(e->key()).isLetter() && this->isInTextMode()) {
		_lcd->appendText(QChar(e->key()).lower());
	}  
}

// EOF
