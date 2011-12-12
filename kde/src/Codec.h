/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/
#ifndef CODEC_H
#define CODEC_H

#include <QObject>

//Qt
class QString;

/**
   @author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class Codec : public QObject
{
Q_OBJECT
private:
   QString payload   ;
   QString name      ;
   QString frequency ;
   QString bitrate   ;
   QString bandwidth ;
   bool enabled;

public:
   Codec(int payload, bool enabled);
    
   QString getPayload   () const;
   QString getName      () const;
   QString getFrequency () const;
   QString getBitrate   () const;
   QString getBandwidth () const;
   bool isEnabled       () const;
   
   void setPayload   ( QString payload   );
   void setName      ( QString name      );
   void setFrequency ( QString frequency );
   void setBitrate   ( QString bitrate   );
   void setBandwidth ( QString bandwidth );
   void setEnabled   ( bool enabled      );
   
   Codec & operator=(const Codec&);
};

#endif
