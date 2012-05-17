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
