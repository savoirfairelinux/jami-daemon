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

Plasma.Page {
    id: bookmarkTab
    height:parent.height-50
    width: parent.width
    
    property int selectedItem: -1

   Component {
      id: contactDelegate
      Rectangle {
         id:bookmarkItemRect
         width:parent.parent.width
         height: 50
         color: "#1a000000"
         radius: 5
         Row {
            spacing: 2
            anchors.margins: 4
            anchors.leftMargin: 50
            Rectangle {
               width: 10
               height: 48
               color: "#AAAAAA"
               border.color: "black"
               border.width: 1
               radius: 10
               anchors.leftMargin: 10
               Column {
                  anchors.centerIn: parent
                  Repeater {
                     model:3
                     Rectangle {
                        width:3
                        height:3
                        radius:2
                        color:"black"
                     }
                  }
               }
            }
            Image {
               source: "plasmapackage:/images/contact.svg";
               width:48
               fillMode: Image.PreserveAspectFit
               height:48
               sourceSize.width: parent.width
               sourceSize.height: parent.width
            }

            Column {
               Text { text: '<b>Name:</b> '   + peerName;color:  theme.textColor }
               Text { text: '<b>Number:</b> ' + peerNumber;color:   theme.textColor }
               //Text { text: '<b>E-Mail:</b>'  + preferredEmail;color: theme.textColor }
            }
         }

         MouseArea {
            anchors.fill: parent
            onClicked: {
               selectedItem = id;
            }
         }
         states: [
            State {
               name: "selected"
               when: (id==selectedItem)
               PropertyChanges {target: bookmarkItemRect; color: theme.highlightColor}
            }
         ]
      }
   }
   
   PlasmaCore.DataSource {
      id: bookmarkSource
      engine: "sflphone"
      interval: 250
      connectedSources: ["bookmark"]
      onDataChanged: {
         plasmoid.busy = false
      }
   }

   PlasmaCore.SortFilterModel {
      id: filter
      filterRole: "listPriority"
      sortRole: "listPriority"
      sortOrder: "DescendingOrder"
      sourceModel:PlasmaCore.SortFilterModel {
         filterRole: "peerName"
         sortRole: "peerName"
         sortOrder: "AscendingOrder"
         sourceModel: PlasmaCore.DataModel {
            dataSource: bookmarkSource
            keyRoleFilter: "[\\d]*"
         }
      }
   }

   anchors {
      top: tabs.bottom;
      left: parent.left;
      right: parent.right;
      bottom: parent.bottom;
   }
   
   Image {
      source: "plasmapackage:/images/favorites.svg";
      width:parent.width
      fillMode: Image.PreserveAspectFit
      height:parent.height-50
      sourceSize.width: parent.width
      sourceSize.height: parent.width
   }

   ListView {
      id: callList
      width:parent.width
      height:parent.height-50
      model: filter
      delegate: contactDelegate
      focus: true
      section {
            property: "section"
            delegate: ListSectiondelegate {}
      }
   }

   Rectangle {
      width:parent.width
      height:50
      anchors {
         top: callList.bottom;
         left: parent.left;
         right: parent.right;
      }
      Row {
         width:parent.width
         height:parent.height
         Plasma.Button {
            width:parent.width
            height:parent.height
            iconSource: "call-start"
            onClicked: {
               call("112")
            }
         }
      }
   }
}
