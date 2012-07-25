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
   width: 350; height: 650

   color:theme.viewBackgroundColor

   PlasmaCore.DataSource {
      id: dataSource
      engine: "sflphone"
      connectedSources: ["calls","history","contacts"]
      interval: 500
   }

   CallTab {
      id:callList
      visible:true
      onRequestNumberOverlayChanged: {
         if (requestNumberOverlay ==  true) {
            transferOverlay.callId  = callList.currentCallId
            transferOverlay.visible = true
         }
      }
   }
   
   ContactTab {
      id:contactList
      visible:false
      onRequestNumberOverlayChanged: {
         console.log("OVERLAY "+contactList.requestNumberOverlay)
         if (contactList.requestNumberOverlay == true) {
            numberOverlay.contactId = contactList.selectedItem
            numberOverlay.visible=true
         }
      }
   }
   
   Settingstab {
      id:settingsTab
      visible:false
   }
   
   HistoryTab {
      id:historyList
      visible:false
   }
   
   BookmarkTab {
      id:bookmarkList
      visible:false
   }
   
   TabBar {
      id:tabs
      width:parent.width
      height:50
   }
   
   NumberOverlay {
      id:numberOverlay
      visible:false
      onVisibleChanged: {
         if (numberOverlay.visible == false) {
            numberOverlay.contactId = ""
            contactList.reset()
         }
      }
   }

   TransferOverlay {
      id:transferOverlay
      visible:false
      onVisibleChanged: {
         if (transferOverlay.visible == false) {
            callList.requestNumberOverlay = false
         }
      }
   }
   
   function call(number) {
      var service = dataSource.serviceForSource("calls")
      var operation = service.operationDescription("Call")
      operation.AccoutId = settingsTab.defaultaccount
      operation.Number = number
      var job = service.startOperationCall(operation)
   }
   
   function playDMTF(number) {
      var service = dataSource.serviceForSource("calls")
      var operation = service.operationDescription("DMTF")
      operation.str = number
      var job = service.startOperationCall(operation)
   }
   
   function transfer(callId,transferNumber) {
      var service = dataSource.serviceForSource("calls")
      var operation = service.operationDescription("Transfer")
      operation.callid         = callId
      operation.transfernumber = transferNumber
      var job = service.startOperationCall(operation)
   }
   
   function hangUp(callId) {
      var service = dataSource.serviceForSource("calls")
      var operation = service.operationDescription("Hangup")
      operation.callid = callId
      var job = service.startOperationCall(operation)
   }
   
   function hold(callId) {
      var service = dataSource.serviceForSource("calls")
      var operation = service.operationDescription("Hold")
      operation.callid = callId
      var job = service.startOperationCall(operation)
   }

   function record(callId) {
      var service = dataSource.serviceForSource("calls")
      var operation = service.operationDescription("Record")
      operation.callid = callId
      var job = service.startOperationCall(operation)
   }
}
