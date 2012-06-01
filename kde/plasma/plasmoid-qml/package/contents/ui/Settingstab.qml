import QtQuick 1.1
import org.kde.plasma.components 0.1 as Plasma
import org.kde.plasma.core       0.1 as PlasmaCore

Plasma.Page {
   id:settingsTab
   
   property string defaultaccount: "IP2IP"
   
   anchors {
      top: tabs.bottom;
      left: parent.left;
      right: parent.right;
      bottom: parent.bottom;
   }

   Image {
      source: "plasmapackage:/images/configure.svgz";
      width:parent.width
      fillMode: Image.PreserveAspectFit
      height:parent.height-50
      sourceSize.width: parent.width
      sourceSize.height: parent.width
   }

   PlasmaCore.DataSource {
      id: accountSource
      engine: "sflphone"
      interval: 250
      connectedSources: ["accounts"]
      onDataChanged: {
         plasmoid.busy = false
      }
   }

   PlasmaCore.DataSource {
      id: infoSource
      engine: "sflphone"
      interval: 250
      connectedSources: ["info"]
      onDataChanged: {
         plasmoid.busy = false
         defaultaccount = infoSource.data["info"]["Current_account"]
      }
   }

   PlasmaCore.SortFilterModel {
      id: accountFilter
      filterRole: "alias"
      sortRole: "alias"
      sortOrder: "AscendingOrder"
      sourceModel: PlasmaCore.DataModel {
         dataSource: accountSource
         keyRoleFilter: "[\\d]*"
      }
   }
   
   Column {
      width:parent.width
      spacing:3

      Plasma.Label {
         text: "Default account"
      }

      Rectangle {
         height:1
         width:parent.width -30
         color:theme.textColor
         anchors.bottomMargin:10
      }

      Repeater {
         model: accountFilter
         Plasma.RadioButton {
            id:checkbox2
            text:alias
            width:parent.width
            onCheckedChanged: {
               if (checkbox2.checked == false && defaultaccount != id) {
                  return
               }
               if (defaultaccount == id) {
                  checkbox2.checked = true
               }
               defaultaccount = id;
            }
            states: [
               State {
                  name: "selected"
                  when: (id==defaultaccount)
                  PropertyChanges {target: checkbox2;checked:true}
               },
               State {
                  name: "unselected"
                  when: (id!=defaultaccount)
                  PropertyChanges {target: checkbox2;checked:false}
               }
            ]
         }
      }
      
      Item {
         height:15
         width:1
         anchors.bottomMargin:10
      }
      
      Plasma.Label {
         text: "Bookmark"
      }
      
      Rectangle {
         height:1
         width:parent.width -30
         color:theme.textColor
         anchors.bottomMargin:10
      }
      
      Plasma.CheckBox {
         text:"Show popular as bookmark"
         width:parent.width
      }
      
      Item {
         height:15
         width:1
         anchors.bottomMargin:10
      }
      
      Plasma.Label {
         text: "Advanced"
      }
      
      Rectangle {
         height:1
         width:parent.width -30
         color:theme.textColor
         anchors.bottomMargin:10
      }
      
      Item {
         height:5
         width:1
         anchors.bottomMargin:10
      }
      
      Plasma.Button {
         text:"Configure sflphone"
         anchors.margins : 10
         width:parent.width
      }
   }
}