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

#include <qbitmap.h>
#include <qimage.h>
#include <qdragobject.h>
#include <qevent.h>	
#include <qpixmap.h>
#include <qpopupmenu.h>
#include <qpushbutton.h>
#include <qsettings.h> 
#include <qthread.h>
#include <qwidget.h>

#include "../../call.h"
#include "../../configuration.h"
#include "../../manager.h"
#include "../../sipvoiplink.h"
#include "../../skin.h"
#include "../guiframework.h"
#include "../../audio/dtmf.h"
#include "configurationpanelui.h"
#include "jpushbutton.h"
#include "mydisplay.h"
#include "numerickeypad.h"
#include "point.h"
#include "phoneline.h"
#include "transqwidget.h"
#include "trayicon.h"
#include "url_inputui.h"
#include "vector.h"
#include "volumecontrol.h"

#define	MAIN_INITIAL_POSITION	20
#define TEXT_MODE				0
#define NUM_MODE				1

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
	QtGUIMainWindow	(QWidget* = 0, const char* = 0,WFlags = 0,Manager * = NULL);
	~QtGUIMainWindow(void);
	
	QPixmap	 TabLinePixmap[NUMBER_OF_LINES][NUMBER_OF_STATES];
	
	// Reimplementation of virtual functions
	virtual int incomingCall (short id);
	virtual int peerAnsweredCall (short id);
	virtual int peerRingingCall (short id);
	virtual int peerHungupCall (short id);
	virtual void displayTextMessage (short id, const string& message);
	virtual void displayError (const string& error);
	virtual void displayStatus (const string& status);
	virtual void displayContext (short id);
	virtual string getRingtoneFile (void);
	virtual void setup (void);

	// Handle IP-phone user actions
	int qt_outgoingCall (void); 	
	int qt_hangupCall (short id);
	int qt_answerCall (short id);
	int qt_onHoldCall (short id);
	int qt_offHoldCall (short id);
	int qt_transferCall (short id);
	void qt_muteOn (short id);
	void qt_muteOff (short id);
	int qt_refuseCall (short id);
	int qt_cancelCall (short id);
	
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
	 * Return elapse for call-timer
	 */
	int getElapse (void);

	// Public functions
	void 	 setMainLCD		 	(void);
	QString  setPathSkin		(void);

	/**
	 * Sets the corresponding pixmap button according to its state.
	 * Handle operations between lines (on hold, off hold) when you click on
	 * a line. 
	 * 
	 * @param	line: number of the current line
	 */
	int 	 toggleLine 	 	(int);
	
	/*
	 * Functions to handle timer call
	 */
	void	 stopCallTimer 		(short);
	void	 startCallTimer 	(short);

	void 	 stopTimerMessage 	(void);
	void	 dialTone			(bool);

	inline void setChooseLine (bool b) { _chooseLine = b; }
	inline bool getChooseLine (void) { return _chooseLine; }
	
	inline void setChosenLine (int line) { _chosenLine = line; }
	inline int getChosenLine (void) { return _chosenLine; }

	inline void setTransfer (bool b) { _transfer = b; }
	inline bool getTransfer (void) {return _transfer; }

signals:
	void 	 keyPressed			(int);

public slots:
	void 	 dial				(void);
	void 	 blinkRingSlot		(void);
	void 	 blinkLineSlot		(void);
	void 	 blinkMessageSlot	(void);
	void 	 qt_quitApplication	(void);
	void 	 pressedKeySlot		(int);
	void 	 stripSlot			(void);
	void 	 addressBook 		(void);
	void	 dtmfKeypad			(void);
	void 	 configuration		(void);
	void 	 hangupLine			(void);
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
	void	 save				(void);
	void	 applySkin			(void);
	
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
	void 	 pressedKeyStar		(void);
	void 	 pressedKeyHash		(void);

	void	 volumeSpkrChanged		(int);
	void	 volumeMicChanged		(int);
	void	 registerSlot			(void);

protected:
	// To handle the key pressed event
	void 		 keyPressEvent 	(QKeyEvent *);

private:
	NumericKeypad* 		_keypad;
	MyDisplay*			_lcd;
	QTimer*				_blinkTimer;
	URL_Input* 			_urlinput;
	ConfigurationPanel*	_panel;
	Manager*			_callmanager;
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
	short*				_buf;
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
	inline void setPrevLine(int line) { _prevLine = line; }
	inline int getPrevLine(void) { return _prevLine; }

	// Array of incoming calls
	int _TabIncomingCalls[NUMBER_OF_LINES];

	// The current phoneline
	int _currentLine;

	bool _dialtone;
	
	void setMode			(int);
	bool isInTextMode		(void);
	bool isInNumMode		(void);
	void initSkin			(void);
	void initVolume 		(void);
	void initButtons 		(void);
	void initBlinkTimer	(void);
	void initSpkrVolumePosition (void);
	void initMicVolumePosition (void);
	void connections		(void);
	string  ringFile			(void);

	int  positionOffsetX	(void);
	void deleteButtons		(void);

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

};


#endif	// __QT_GUI_MAIN_WIDOW_H__
