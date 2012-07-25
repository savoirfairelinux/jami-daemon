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
   id: historyTab
   width: parent.width

   property int selectedItem: -1
    property string callNumber: ""

   Component {
      id: historyDelegate
      Rectangle {
         width:parent.parent.width
         height: 50
         color: "#1a000000"
         radius: 5
         id:historyItemRect
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
               Text { text: '<b>Name:</b> ' + peerName;color:theme.textColor }
               Text { text: '<b>Number:</b> ' + peerNumber;color:theme.textColor  }
               Text { text: '<b>Date:</b>' + date;color:theme.textColor }
            }
         }

         MouseArea {
            anchors.fill: parent
            onClicked: {
               selectedItem = id
               callNumber = peerNumber
            }
         }
         states: [
            State {
               name: "selected"
               when: (id==selectedItem)
               PropertyChanges {target: historyItemRect; color: theme.highlightColor}
            }
         ]
      }
   }

   Image {
      source: "plasmapackage:/images/history.svgz";
      width:parent.width
      fillMode: Image.PreserveAspectFit
      height:parent.height-50
      sourceSize.width: parent.width
      sourceSize.height: parent.width
   }

   anchors {
      top: tabs.bottom;
      left: parent.left;
      right: parent.right;
      bottom: parent.bottom;
   }

   PlasmaCore.DataSource {
      id: historySource
      engine: "sflphone"
      interval: 5000
      connectedSources: ["history"]
      onDataChanged: {
         plasmoid.busy = false
      }
   }

   PlasmaCore.SortFilterModel {
      id: dateFilter
      filterRole: "date"
      sortRole: "date"
      sortOrder: "DescendingOrder"
      //filterRegExp: toolbarFrame.searchQuery
      sourceModel: PlasmaCore.DataModel {
         dataSource: historySource
         keyRoleFilter: "[\\d]*"
      }
   }

   PlasmaCore.SortFilterModel {
      id: nameFilter
      filterRole: "peerName"
      sortRole: "peerName"
      sortOrder: "AscendingOrder"
      //filterRegExp: toolbarFrame.searchQuery
      sourceModel: PlasmaCore.DataModel {
         dataSource: historySource
         keyRoleFilter: "[\\d]*"
      }
   }

   PlasmaCore.SortFilterModel {
      id: lengthFilter
      filterRole: "length"
      sortRole: "length"
      sortOrder: "AscendingOrder"
      //filterRegExp: toolbarFrame.searchQuery
      sourceModel: PlasmaCore.DataModel {
         dataSource: historySource
         keyRoleFilter: "[\\d]*"
      }
   }

   ListView {
      id: callList
      width:parent.width
      height:parent.height-80
      model: dateFilter
      anchors.top:sortTab.bottom
      delegate: historyDelegate
      focus: true
      section {
            property: "section"
            criteria: ViewSection.FullString
            delegate: ListSectiondelegate {}
      }
   }

   Plasma.TabBar {
      id:sortTab
      width:parent.width
      height:30
      /*Repeater {
         model:5
         Plasma.TabButton {
            text: Array("Name","Organisation","Recent","Department","Group")[index];
            onPressedChanged: {

            }
         }
      }*/
      Plasma.TabButton {
         text: "Date";
         onPressedChanged: {
            console.log("Date pressed")
            callList.section.property = "section"
            callList.model = dateFilter
         }
      }
      Plasma.TabButton {
         text: "Name";
         onPressedChanged: {
            console.log("Name pressed")
            callList.section.property = "peerName"
            callList.model = nameFilter
         }
      }
      Plasma.TabButton {
         text: "Popularity";
         onPressedChanged: {
            console.log("Popularity pressed")
         }
      }
      Plasma.TabButton {
         text: "Length";
         onPressedChanged: {
            console.log("Length pressed")
            callList.section.property = ""
            callList.model = lengthFilter
         }
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
            id:clearButton
            width:parent.width
            height:parent.height
            iconSource: "edit-clear"
            onClicked: {
               //call("112")
            }
         }
         Plasma.Button {
            id:callButton
            width:parent.width
            height:parent.height
            visible:false
            iconSource: "call-start"
            onClicked: {
               if (callNumber != "") {
                  console.log("Calling: "+callNumber)
                  call(callNumber)
               }
            }
         }
      }
      
      states: [
         State {
            name: "itemSelected"
            when: (selectedItem != -1)
            PropertyChanges {
               target: clearButton;
               width:parent.width/2
            }
            PropertyChanges {
               target: callButton
               visible:true
               width:parent.width/2
            }
         },
         State {
            name: "noItem"
            when: (selectedItem == -1)
            PropertyChanges {target: clearButton;width:parent.width}
            PropertyChanges {target: callButton;visible:false}
         }
      ]
   }
}
