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

#include "configuration.h"
#include "configurationpanelui.h"
#include "dtmf.h"
#include "jpushbutton.h"
#include "manager.h"
#include "mydisplay.h"
#include "numerickeypad.h"
#include "phonebookui.h"
#include "point.h"
#include "skin.h"
#include "sip.h"
#include "transqwidget.h"
#include "trayicon.h"
#include "url_inputui.h"
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

class QtGUIMainWindow : public TransQWidget {
	Q_OBJECT
public:
	// Default Constructor and destructor
	QtGUIMainWindow	(QWidget* = 0, const char* = 0,WFlags = 0,Manager * = NULL);
	~QtGUIMainWindow(void);

	// Public member variables
	NumericKeypad		*keypad;
	MyDisplay			*lcd;
	QTimer 				*blinkTimer;
	PhoneBook		   	*phonebook;
	URL_Input		   	*urlinput;
	ConfigurationPanel 	*panel;
	QSettings 			 settings;
	Manager				*callmanager;
		
	JPushButton			*phoneKey_msg;
	JPushButton			*phoneKey_transf;
	JPushButton			*phoneKey_conf;
	JPushButton			*phoneKey_line0;
	JPushButton			*phoneKey_line1;
	JPushButton			*phoneKey_line2;
	JPushButton			*phoneKey_line3;
	JPushButton			*reduce_button;
	JPushButton			*quit_button;
	JPushButton			*addr_book_button;
	JPushButton			*configuration_button;
	JPushButton			*hangup_button;
	JPushButton			*dial_button;
	JPushButton			*mute_button;
	JPushButton			*dtmf_button;
	VolumeControl		*vol_mic;
	VolumeControl		*vol_spkr;

	int 				 currentLineNumber;
	int 				 busyNum;
	bool				 ringVar;
	bool 				 msgVar;
	int			 		 chosenLine;
	bool		 		 choose;
	bool				 noChoose;
	bool		 		 transfer;
	bool				 callinprogress;
	
	QPixmap				 TabLinePixmap[NUMBER_OF_LINES][NUMBER_OF_STATES];

	QPopupMenu 			*mypop;
	MyTrayIcon 			*trayicon;

	Vector				*micVolVector;
	Vector				*spkrVolVector;
	int			vol_mic_x, vol_mic_y;
	int			vol_spkr_x, vol_spkr_y;

	// Public functions
	void 	 setMainLCD		 	(void);
	void 	 toggleLine 	 	(int);
	void 	 stopTimerMessage 	(void);
	QString  setPathSkin		(void);
	QString  ringFile			(void);
	QString	 getRingFile		(void);
	inline 
	void 	 ring				(bool b)	{ this->ringVar = b; }
	inline 
	bool 	 ringing			(void) 		{ return this->ringVar; }
	void	 stopCallTimer 		(int);
	void	 startCallTimer 	(int);
	void	 setFreeStateLine 	(int);
	void	 setCurrentLineNumber(int);
	void	 dialTone			(bool);
	void	 setInactiveLine 	(int);

signals:
	void 	 keyPressed			(int);

public slots:
	void 	 dial				(void);
	void 	 blinkRingSlot		(void);
	void 	 blinkLineSlot		(void);
	void 	 blinkMessageSlot	(void);
	void 	 quitApplication	(void);
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
	// Configuration skin file
	Point				*pt;
	

	// To construct ring rect pixmap
	QImage 		 imageRing;
	QImage 		 imageNoRing;

	QPixmap		 TabMsgPixmap[NUMBER_OF_STATES];
	int 		 modeType;
	DTMF 		*key;
	short   	*buf;
	bool		 apply;

	int 		 onLine;

	// For numeric keypad
	bool first;
	
	bool b_dialtone;

	
	// Private functions
	void 		 setMode		(int);
	bool 		 isInTextMode	(void);
	bool 		 isInNumMode	(void);
	void		 initSkin		(void);
	void 		 initButtons 	(void);
	void 		 initBlinkTimer	(void);
	void		 initSpkrVolumePosition (void);
	void		 initMicVolumePosition (void);
	void		 connections	(void);

	int 		 numLineBusy	(void);

	int			 positionOffsetX		(void);
	void 		 deleteButtons	(void);

};


#endif	// __QT_GUI_MAIN_WIDOW_H__
