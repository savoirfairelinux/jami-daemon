/******************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                                 *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>   *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Lesser General Public               *
 *   License as published by the Free Software Foundation; either             *
 *   version 2.1 of the License, or (at your option) any later version.       *
 *                                                                            *
 *   This library is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *   Lesser General Public License for more details.                          *
 *                                                                            *
 *   You should have received a copy of the Lesser GNU General Public License *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 *****************************************************************************/
import QtQuick 1.1
import org.kde.plasma.components 0.1 as Plasma
import org.kde.plasma.core       0.1 as PlasmaCore

Rectangle {
   id:transferOverlay

   property string callId: ""

   color:"#BB000000"
   width:parent.width
   height:parent.height

   MouseArea {
      anchors.fill: parent
      onClicked: {
         parent.visible=false
      }
   }

   Plasma.Button {
      x:parent.x+parent.width-30
      y:5
      height:30
      width:30
      iconSource:"dialog-close"
      onClicked: {
         parent.visible=false
      }
   }

   /*Column {
      width:parent.width
      anchors.centerIn : parent

      spacing:10
      Repeater {
         model: PlasmaCore.DataModel {
            dataSource: contactNumberSource
            keyRoleFilter: "[\\d]*"
         }
         Item {
            width:parent.width
            height:50
            Plasma.Button {
               anchors.fill:parent
               clip:true
               text:"<b>"+type+"</b> ("+number+")"
               onClicked: {
                  call(number);
                  numberOverlay.visible = false
               }
            }
         }
      }
   }*/


   Image {
      source: "plasmapackage:/images/transfertarrow.svg";
      width:100
      fillMode: Image.PreserveAspectFit
      height:60
      y:parent.height+parent.y - 60
      x:parent.width+parent.x-100
      sourceSize.width: 100
      sourceSize.height: 60
   }

   Item {
      anchors.fill: parent
      Column {
         anchors.centerIn : parent
         width:parent.width -20
         height:190
         Plasma.TextArea {
            id:transferText
            width:parent.width
            height:40
            focus:true

            /*Keys.onPressed: {
               console.log("Key pressed")
               text += event.key
            }*/

            Keys.onReturnPressed: {
               console.log("Time to transfer"+transferText.text)
            }
         }

         Item {
            height:100
            width:1
         }
         
         Plasma.Button {
            text: "Transfer"
            //width:parent.width/2
            height:50
            anchors.horizontalCenter: parent.horizontalCenter
            iconSource: "plasmapackage:/images/transfertarrow.svg"
            onClicked: {
               transfer(callId,transferText.text)
               callId = ""
               transferText.text = ""
               transferOverlay.visible = false
            }
         }
      }
   }

   states: [
      State {
         name: "active"
         when: (visible == true)
      },
      State {
         name: "innactive"
         when: (visible == false)
         //PropertyChanges {target: clearButton;width:parent.width}
      }
   ]
}
