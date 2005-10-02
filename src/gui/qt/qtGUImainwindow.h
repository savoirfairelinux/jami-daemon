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

#ifndef __QT_GUI_MAIN_WINDOW_H__
#define __QT_GUI_MAIN_WINDOW_H__

#include <qimage.h>
#include <qpixmap.h>
#include <qpopupmenu.h>
#include <qwidget.h>

#include "../../skin.h"
#include "../guiframework.h"
#include "configurationpanelui.h"
#include "phoneline.h"
#include "transqwidget.h"
#include "trayicon.h"
#include "url_input.h"

#define	MAIN_INITIAL_POSITION	20

// To type with keyboard what you want to appear in the screen.
#define TEXT_MODE				0
#define NUM_MODE				1

class Call;
class DTMF;
class JPushButton;
class MyDisplay;
class MyTrayIcon;
class NumericKeypad;
class PhoneLine;		
class Point;
class SipVoIPLink;
class URL_Input;
class VolumeControl;
class Vector;
///////////////////////////////////////////////////////////////////////////////
// Tray Icon class
///////////////////////////////////////////////////////////////////////////////
class MyTrayIcon : public TrayIcon
{
	Q_OBJECT
public:
	MyTrayIcon(const QPixmap &, const QString &, QPopupMenu *popup = 0, 
			QObject *parent = 0, const char *name = 0 );
	~MyTrayIcon(){};

	QPopupMenu *menu;

signals:
	void	clickedLeft(void);
protected:	
	void 	mousePressEvent (QMouseEvent *);
};

///////////////////////////////////////////////////////////////////////////////
// GUI main window
///////////////////////////////////////////////////////////////////////////////
class QtGUIMainWindow : public TransQWidget, public GuiFramework {
	Q_OBJECT
public:
	// Default Constructor and destructor
	QtGUIMainWindow	(QWidget* = 0, const char* = 0,WFlags = 0);
	~QtGUIMainWindow(void);
	
	// To build an array of pixmap according to the state
	QPixmap	 TabLinePixmap[NUMBER_OF_LINES][NUMBER_OF_STATES];
	
	// Reimplementation of virtual functions
	virtual int incomingCall (short id);
	virtual void peerAnsweredCall (short id);
	virtual int peerRingingCall (short id);
	virtual int peerHungupCall (short id);
	virtual void displayTextMessage (short id, const std::string& message);
	virtual void displayErrorText (short id, const std::string& message);
	virtual void displayError (const std::string& error);
	virtual void displayStatus (const std::string& status);
	virtual void displayContext (short id);
	virtual std::string getRingtoneFile (void);
	virtual void setup (void);
	virtual void sendMessage(const std::string& code, const std::string& seqId, TokenList& arg) {}
	virtual void sendCallMessage(const std::string& code, 
  const std::string& sequenceId, 
  short id, 
  TokenList arg) {}


		
	/*
	 * Return the id matching to the chosen line
	 */
	virtual int selectedCall (void);

	// Handle IP-phone user actions
	int qt_outgoingCall (void); 	
	int qt_hangupCall (short id);
	int qt_cancelCall (short id);
	int qt_answerCall (short id);
	int qt_onHoldCall (short id);
	int qt_offHoldCall (short id);
	int qt_transferCall (short id);
	void qt_muteOn (short id);
	void qt_muteOff (short id);
	int qt_refuseCall (short id);
	
	/*
	 * Return the call corresponding to the id
	 */
	Call* getCall (short id);
	
	/*
	 * Return the phoneline corresponding to the call-id
	 */
	PhoneLine* getPhoneLine (short id);

	/*
	 * Accessor of the current line
	 */
	int getCurrentLine (void);

	/*
	 * Modifior of the current line
	 */
	void setCurrentLine (int current);

	/*
	 * Check if 'id' is the current id
	 */
	bool isCurrentId (short id);
	
	/*
	 * Return elapse for call-timer
	 */
	int getElapse (void);

	/*
	 * Set the skin path from the setup
	 */ 
	QString  setPathSkin		(void);

	/**
	 * Sets the corresponding pixmap button according to its state.
	 * Handle operations between lines (on hold, off hold) when you click on
	 * a line. Manage the different cases which could occur.
	 * 
	 * @param	line: number of the current line
	 */
	int 	 toggleLine 	 	(int);
	
	/*
	 * Functions to handle timer call
	 */
	void	 stopCallTimer 		(short);
	void	 startCallTimer 	(short);

	/*
	 * Stop the blinking-signal when you check your voicemail (not used yet)
	 */
	void 	stopVoiceMessageNotification	(void);
	void 	startVoiceMessageNotification	(void);
  void  sendVoiceNbMessage(const std::string& nb_msg) {}

	/*
	 * Manage if you selected a line before dialing 
	 */
	inline void setChooseLine (bool b) { _chooseLine = b; }
	inline bool getChooseLine (void) { return _chooseLine; }
	
	/*
	 * Store the line you selected before dialing
	 */
	inline void setChosenLine (int line) { _chosenLine = line; }
	inline int getChosenLine (void) { return _chosenLine; }

	/*
	 * Manage if you are in transfer mode
	 */
	inline void setTransfer (bool b) { _transfer = b; }
	inline bool getTransfer (void) { return _transfer; }

signals:
	void 	 keyPressed			(int);

public slots:
	// Slot when you click on ok-button or type Enter
	void 	 dial				(void);
	
	// Notification when you have received incoming call 
	void 	 blinkRingSlot		(void);

	// Notification when you have put on-hold a call
	void 	 blinkLineSlot		(void);

	// Notification when you have received message in your mailbox
	void 	 blinkMessageSlot	(void);

	// Slot to use numeric keypad
	void 	 pressedKeySlot		(int);

	// To remove specified characters before appending the text in url-input 
	void 	 stripSlot			(void);

	// To show the address book (not implemented yet)
	void 	 addressBook 		(void);

	// To show keypad at the specified position
	void	 dtmfKeypad			(void);

	// To show setup windows
	void 	 configuration		(void);

	// Slot to hangup call
	void 	 hangupLine			(void);

	// Slot to quit application
	void 	 qt_quitApplication	(void);
	
	// Manage different buttons in the phone	
	void 	 button_mute		(void);
	void 	 button_line0		(void);
	void 	 button_line1		(void);
	void 	 button_line2		(void);
	void 	 button_line3		(void);
	void 	 button_line4		(void);
	void 	 button_line5		(void);
	void 	 button_msg			(void);
	void 	 button_transfer	(void);
	void 	 button_conf		(void);
	void 	 clickHandle 		(void);
	void	 reduceHandle		(void);
	
	// Handle pressed key of the dtmf keypad
	void 	 pressedKey0		(void);
	void 	 pressedKey1		(void);
	void 	 pressedKey2		(void);
	void 	 pressedKey3		(void);
	void 	 pressedKey4		(void);
	void 	 pressedKey5		(void);
	void 	 pressedKey6		(void);
	void 	 pressedKey7		(void);
	void 	 pressedKey8		(void);
	void 	 pressedKey9		(void);
	void 	 pressedKeyStar		(void); // (*)
	void 	 pressedKeyHash		(void); // (#)

	// Manage volume control
	void	 volumeSpkrChanged		(int);
	void	 volumeMicChanged		(int);

	// To register manually
	void	 registerSlot			(void);
	// To save configuration
	void	 save				(void);
	// To apply selected skin
	void	 applySkin			(void);

protected:
	// To handle the key pressed event
	void 		 keyPressEvent 	(QKeyEvent *);

private:
	NumericKeypad* 		_keypad;
	MyDisplay*			_lcd;
	QTimer*				_blinkTimer;
	URL_Input* 			_urlinput;
	ConfigurationPanel*	_panel;
	QPopupMenu*			_mypop;
	MyTrayIcon*			_trayicon;
	DTMF*				_key;
	PhoneLine*			phLines[NUMBER_OF_LINES];

	JPushButton*		phoneKey_msg;
	JPushButton*		phoneKey_transf;
	JPushButton*		phoneKey_conf;
	JPushButton*		phoneKey_line0;
	JPushButton*		phoneKey_line1;
	JPushButton*		phoneKey_line2;
	JPushButton*		phoneKey_line3;
	JPushButton*		reduce_button;
	JPushButton*		quit_button;
	JPushButton*		addr_book_button;
	JPushButton*		configuration_button;
	JPushButton*		hangup_button;
	JPushButton*		dial_button;
	JPushButton*		mute_button;
	JPushButton*		dtmf_button;
	int16*				_buf;
	// Configuration skin file
	Point*		pt;

	// For volume buttons
	VolumeControl*		vol_mic;
    VolumeControl*		vol_spkr;
	Vector*		micVolVector;
    Vector*		spkrVolVector;
	int			vol_mic_x, 
				vol_mic_y;
	int			vol_spkr_x, 
				vol_spkr_y;

	// To construct ring rect pixmap
	QImage 		imageRing;
	QImage 		imageNoRing;

	QPixmap		TabMsgPixmap[NUMBER_OF_STATES];
	int 		modeType;
	bool		_apply;

	bool b_dialtone;

	// For numeric keypad
	bool _first;
	
	bool _transfer;
	bool _msgVar;
	
	// For outgoing call, if it occurs on the first free line or on the chosen
	// line by the user
	bool _chooseLine;
	int _chosenLine;
	int _prevLine;

	// Array of incoming calls, contains call-id
	int _TabIncomingCalls[NUMBER_OF_LINES];

	// The current phoneline
	int _currentLine;

	bool _dialtone;
	
	////////////////////
	// Private functions
	////////////////////

	inline void setPrevLine(int line) { _prevLine = line; }
	inline int getPrevLine(void) { return _prevLine; }

	// Set position for screen 
	void setMainLCD		 	(void);

	// Handle num/text mode
	void setMode			(int);
	bool isInTextMode		(void);
	bool isInNumMode		(void);

	// Initialize all the skin 
	void initSkin			(void);
	void initButtons 		(void);
	void initVolume 		(void);
	void initSpkrVolumePosition (void);
	void initMicVolumePosition (void);

	// Delete buttons called in destructor
	void deleteButtons		(void);
	
	// Initialize timer for blinking notification
	void initBlinkTimer		(void);

	// Initialize all the connections (SIGNAL->SLOT) 
	void connections		(void);

	// Set the ringtone file path
	string  ringFile		(void);

	// Set position for numeric keypad
	int  positionOffsetX	(void);

	/*
	 * Associate a phoneline number to a call identifiant
	 * @param	identifiant call
	 * @return	phoneline number
	 */
	int associateCall2Line (short id);
	
	/**
	 * Search the busy line number among all lines and different with
	 * current line number.
	 *
	 * @return  number of busy line
	 */
	int  busyLineNumber		(void);

	/*
	 * Put in state on-hold the line 'line'
	 */
	int putOnHoldBusyLine 	(int line);
	
	/*
	 * Return the id of the first line number if incoming call occurs
	 * Otherwise return -1
	 */
	short isThereIncomingCall	(void);

	/*
	 * Return the id of the line number 'line' if an incoming call occurs
	 * Otherwise return -1
	 */
	short isIncomingCall	(int line);

	/*
	 * Return phoneline number according to the identifiant 'id'
	 * id2line returns -1 if phoneline number is not found
	 */
	int id2line				(short id);
	
	/*
	 * Return id according to the phoneline number 'line'
	 * line2id returns -1 if id is not found
	 */
	short line2id			(int line);

	/*
	 * Change state and pixmap of the line 'line' according to the state 'state'
	 */
	void changeLineStatePixmap (int line, line_state state);

	/*
	 * Handle dial tone
	 */
	void dialtone (bool var);

	/* 
	 * Functions used in toggle function
	 */
	void callIsRinging(int id, int line, int busyLine);
	void callIsProgressing (int id, int line, int busyLine);
	int callIsBusy (Call* call, int id, int line, int busyLine);
	int callIsOnHold(int id, int line, int busyLine);
	int callIsIncoming (int id, int line, int busyLine);
	void clickOnFreeLine(int line, int busyLine);

};


#endif	// __QT_GUI_MAIN_WIDOW_H__
