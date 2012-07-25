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
    id: contactTab
    height:parent.height-50
    width: parent.width

    property string selectedItem:   "-1"
    property string callNumber:      ""
    property int    selectedPCount: 0
    property bool   requestNumberOverlay: false

    Component {
      id: contactDelegate
      Rectangle {
         id:contactItemRect
         width:parent.parent.width
         height: 50
         color: "#1a000000"
         radius:5
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
               Text { text: '<b>Name:</b> '         + formattedName;color: theme.textColor }
               Text { text: '<b>Organisation:</b> ' + organization ;color: theme.textColor }
               Text { text: '<b>Number:</b>'        + phoneNumber  ;color: theme.textColor }
            }
         }

         MouseArea {
            anchors.fill: parent
            onClicked: {
               selectedItem = uid;
               if (phoneCount > 1) {
                  //requestNumberOverlay = true
                  selectedPCount = phoneCount
                  
               }
               else if (phoneCount == 1) {
                  callNumber = phoneNumber
               }
            }
         }
         states: [
            State {
               name: "selected"
               when: (uid==selectedItem)
               PropertyChanges {target: contactItemRect; color: theme.highlightColor}
            }
         ]
      }
   }
   
   anchors {
      top: tabs.bottom;
      left: parent.left;
      right: parent.right;
      bottom: parent.bottom;
   }
   
   PlasmaCore.DataSource {
      id: contactSource
      engine: "sflphone"
      interval: 250
      connectedSources: ["contacts"]
      onDataChanged: {
         plasmoid.busy = false
      }
   }
   
   PlasmaCore.SortFilterModel {
      id: nameFilter
      filterRole: "formattedName"
      sortRole: "formattedName"
      sortOrder: "AscendingOrder"
      sourceModel: PlasmaCore.DataModel {
         dataSource: contactSource
         keyRoleFilter: "[\\d]*"
      }
   }
   
   PlasmaCore.SortFilterModel {
      id: orgFilter
      filterRole: "organization"
      sortRole: "organization"
      sortOrder: "AscendingOrder"
      sourceModel: nameFilter
   }
   
   PlasmaCore.SortFilterModel {
      id: depFilter
      filterRole: "department"
      sortRole: "department"
      sortOrder: "AscendingOrder"
      sourceModel: nameFilter
   }

   Image {
      source: "plasmapackage:/images/contact.svg";
      width:parent.width
      fillMode: Image.PreserveAspectFit
      height:parent.height-50
      sourceSize.width: parent.width
      sourceSize.height: parent.width
   }
   
   ListView {
      id: callList
      width:parent.width
      height:parent.height-80
      anchors.top: sortTab.bottom
      model: nameFilter
      delegate: contactDelegate
      focus: true
      section {
            criteria: ViewSection.FirstCharacter
            property: "formattedName"
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
         text: "Name";
         onPressedChanged: {
            callList.section.criteria = ViewSection.FirstCharacter
            callList.section.property = "formattedName"
            callList.model = nameFilter
         }
      }
      Plasma.TabButton {
         text: "Organisation";
         onPressedChanged: {
            callList.section.criteria = ViewSection.FullString
            callList.section.property = "organization"
            callList.model = orgFilter
         }
      }
      Plasma.TabButton {
         text: "Recent";
         onPressedChanged: {
            callList.section.criteria = ViewSection.FullString
         }
      }
      Plasma.TabButton {
         text: "Department";
         onPressedChanged: {
            callList.section.criteria = ViewSection.FullString
            callList.section.property = "department"
            callList.model = depFilter
         }
      }
      Plasma.TabButton {
         text: "Group";
         onPressedChanged: {

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
            id:addContact
            width:parent.width
            height:parent.height
            iconSource: "list-add-user"
            onClicked: {
               call("112")
            }
         }
         Plasma.Button {
            id:callContact
            width:parent.width/3
            height:parent.height
            iconSource: "call-start"
            visible:false
            onClicked: {
               if (callNumber != "" && selectedItem != "-1") {
                  call(callNumber)
               }
               else if (selectedItem != "-1" && selectedPCount > 1) {
                  requestNumberOverlay = true
               }
            }
         }
         Plasma.Button {
            id:editContact
            width:parent.width/3
            height:parent.height
            iconSource: "document-edit"
            visible:false
            onClicked: {
               call("112")
            }
         }
      }
      states: [
         State {
            name: "itemSelected"
            when: (selectedItem != -1)
            PropertyChanges {
               target: addContact;
               width:parent.width/3
            }
            PropertyChanges {
               target: callContact
               visible:true
               width:parent.width/3
            }
            PropertyChanges {
               target: editContact
               visible:true
               width:parent.width/3
            }
         },
         State {
            name: "noItem"
            when: (selectedItem == -1)
            /*PropertyChanges { target: clearButton; width:   parent.width  }
            PropertyChanges { target: clearButton; visible: false         }
            PropertyChanges { target: clearButton; visible: false         }*/
         }
      ]
   }

   states: [
      State {
         name: "default"
         when: (requestNumberOverlay == false)
      },
      State {
         name: "requestNumberOverlay"
         when: (requestNumberOverlay == true)
      }
   ]
   onRequestNumberOverlayChanged: {
      if (requestNumberOverlay ==  false)
         reset()
   }

   function reset() {
      selectedItem = "-1"
      callNumber = ""
      selectedPCount = 0
      requestNumberOverlay = false
   }
}
