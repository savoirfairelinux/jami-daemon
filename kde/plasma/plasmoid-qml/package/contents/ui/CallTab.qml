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
   anchors.top: tabs.bottom
   width:parent.width
   height:parent.height - tabs.height
   id:callTab

   property string currentCall: ""
   property string currentCallId: ""
   property bool   requestNumberOverlay: false

   Component {
      id: callDelegate
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
               property string callState: ""
               width:48
               fillMode: Image.PreserveAspectFit
               height:48
               sourceSize.width: parent.width
               sourceSize.height: parent.width
               Image {
               property string callState: stateName
               source: "plasmapackage:/images/contact.svg";
               width:48
               fillMode: Image.PreserveAspectFit
               height:48
               sourceSize.width: parent.width
               sourceSize.height: parent.width
               onCallStateChanged: {
                  if (callState == "Ringing (out)") {
                     source="plasmapackage:/images/state/ring.svg";
                  }
                  else if (callState == "Talking") {
                     source="plasmapackage:/images/state/current.svg";
                  }
                  else if (callState == "Hold") {
                     source="plasmapackage:/images/state/hold.svg";
                  }
                  else if (callState == "Busy") {
                     source="plasmapackage:/images/state/busy.svg";
                  }
                  else if (callState == "Failed") {
                     source="plasmapackage:/images/state/fail.svg";
                  }
                  else if (callState == "Ringing (in)") {
                     source="plasmapackage:/images/state/incoming.svg";
                  }
                  else if (callState == "Dialing") {
                     source="plasmapackage:/images/state/dial.svg";
                  }
                  else if (callState == "Transfer") {
                     source="plasmapackage:/images/state/transfert.svg";
                  }
                  else {
                     console.log("Unknow state")
                  }
               }
               Component.onCompleted: {
                  if (callState == "Ringing (out)") {
                     source="plasmapackage:/images/state/ring.svg";
                  }
                  else if (callState == "Talking") {
                     source="plasmapackage:/images/state/current.svg";
                  }
                  else if (callState == "Hold") {
                     source="plasmapackage:/images/state/hold.svg";
                  }
                  else if (callState == "Busy") {
                     source="plasmapackage:/images/state/busy.svg";
                  }
                  else if (callState == "Failed") {
                     source="plasmapackage:/images/state/fail.svg";
                  }
                  else if (callState == "Ringing (in)") {
                     source="plasmapackage:/images/state/incoming.svg";
                  }
                  else if (callState == "Dialing") {
                     source="plasmapackage:/images/state/dial.svg";
                  }
                  else if (callState == "Transfer") {
                     source="plasmapackage:/images/state/transfert.svg";
                  }
                  else {
                     console.log("Unknow state "+callState+" end"+stateName)
                  }
               }
            }
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
               currentCallId = id;
               currentCall   = peerNumber;
            }
         }
         states: [
            State {
               name: "selected"
               when: (id==currentCallId)
               PropertyChanges {target: bookmarkItemRect; color: theme.highlightColor}
            }
         ]
      }
   }

   PlasmaCore.DataSource {
      id: callSource
      engine: "sflphone"
      interval: 250
      connectedSources: ["calls"]
      onDataChanged: {
         console.log("CALL LIST CHANGED")
         plasmoid.busy = false
      }
   }

   /*PlasmaCore.SortFilterModel {
      id: filter
      filterRole: "listPriority"
      sortRole: "listPriority"
      sortOrder: "DescendingOrder"
      sourceModel:PlasmaCore.SortFilterModel {
         filterRole: "peerName"
         sortRole: "peerName"
         sortOrder: "AscendingOrder"
         sourceModel:
      }
   }*/


   ListView {
      id: callList
      model: PlasmaCore.DataModel {
            dataSource: callSource
            keyRoleFilter: "[\\d]*"
         }
      width:parent.width
      height:parent.height-280
      delegate: callDelegate
      //anchors.fill:parent
      focus: true
      section {
            property: "section"
            delegate: ListSectiondelegate {}
      }
      onCountChanged: {
         if (count == 0) {
            currentCall = ""
            currentCallId = ""
         }
      }
   }

   Plasma.TextArea {
      id:display
      width:parent.width
      height:40
      focus:true
      readOnly:true
      anchors {
            top: callList.bottom;
            left: parent.left;
            right: parent.right;
      }

      function addText(text) {
         display.text = display.text+text
      }

      Keys.onPressed: {
         console.log("Key pressed")
         display.addText(event.key)
      }
      Keys.onReturnPressed: {
         console.log("Call")

      }
   }
   
   Image {
      source: "plasmapackage:/images/phone.svg";
      width:parent.width
      fillMode: Image.PreserveAspectFit
      height:parent.height-250
   }

   DialPad {
      anchors {
            top: display.bottom;
            left: parent.left;
            right: parent.right;
      }
      id: dialPad
      width:parent.width
      height:200
   }

   Rectangle {
      width:parent.width
      height:50
      anchors {
         top: dialPad.bottom;
         left: parent.left;
         right: parent.right;
      }
      Row {
         width:parent.width
         height:parent.height
         Plasma.Button {
            id:addContactButton
            width:parent.width/5
            height:parent.height
            iconSource: "list-add-user"
            onClicked: {
               call("112")
            }
         }
         Plasma.Button {
            id:newCallButton
            width:parent.width/6
            height:parent.height
            iconSource: "plasmapackage:/images/state/new_call.svg"
            visible:false
            onClicked: {
               currentCall   = ""
               currentCallId = ""
            }
         }
         Plasma.Button {
            id: buttonCall
            width:parent.width/5 * 3
            height:parent.height
            iconSource: "call-start"
            onClicked: {
               currentCall = display.text
               display.text = ""
               call(currentCall)
            }
         }
         Plasma.Button {
            id:buttonTransfer
            width:parent.width/5
            height:parent.height
            visible:false
            iconSource: "plasmapackage:/images/state/transfert.svg"
            onClicked: {
               if (currentCall != "") {
                  requestNumberOverlay = true
               }
            }
         }
         Plasma.Button {
            id:buttonHangUp
            width:parent.width/5
            height:parent.height
            visible:false
            iconSource: "plasmapackage:/images/state/hang_up.svg"
            onClicked: {
               console.log("Attempt to hangup "+currentCallId)
               if (currentCallId != "") {
                  hangUp(currentCallId)
               }
            }
         }
         Plasma.Button {
            id:buttonHold
            width:parent.width/5
            height:parent.height
            visible:false
            iconSource: "plasmapackage:/images/state/hold.svg"
            onClicked: {
               if (currentCall != "") {
                  hold(currentCallId)
               }
            }
         }
         Plasma.Button {
            id:buttonRecord
            width:parent.width/5
            height:parent.height
            visible:false
            iconSource: "plasmapackage:/images/state/record.svg"
            onClicked: {
               if (currentCall != "") {
                  record(currentCallId)
               }
            }
         }
         Plasma.Button {
            id:buttonClear
            width:parent.width/5
            height:parent.height
            iconSource: "edit-clear-locationbar-rtl"
            onClicked: {
               currentCall = ""
               display.text = ""
            }
         }
      }
   }

   states: [
      State {
         name: "dialing"
         when: (currentCall == "")
         PropertyChanges {
            target: buttonCall
         }
      },
      State {
         name: "active"
         when: (currentCall != "")
         PropertyChanges {
            target: buttonHangUp
            visible:true
            width:parent.width/3
         }
         PropertyChanges {
            target: addContactButton
            visible:false
         }
         PropertyChanges {
            target: newCallButton
            visible:true
         }
         PropertyChanges {
            target: buttonRecord
            visible:true
            width:parent.width/6
         }
         PropertyChanges {
            target: buttonTransfer
            visible:true
            width:parent.width/6
         }
         PropertyChanges {
            target: buttonHold
            visible:true
            width:parent.width/6
         }
         PropertyChanges {
            target: buttonCall
            //width:parent.width/5
            visible:false
         }
         PropertyChanges {
            target: buttonClear
            //width:parent.width/5
            visible:false
         }
      },
      State {
         name: "requestTransferOverlay"
         when: (requestNumberOverlay == true)
      }
   ]
}
