import QtQuick 1.1
import org.kde.plasma.components 0.1 as Plasma
import org.kde.plasma.core       0.1 as PlasmaCore

Rectangle {
   id: listSectiondelegate
   height:20
   gradient: Gradient {
      GradientStop { position: 1.0; color: theme.backgroundColor }
      GradientStop { position: 0.0; color: theme.buttonBackgroundColor }
   }
   width: parent.width
   //height: childrenRect.height + 4
   radius:2
   Text {
      anchors.left: parent.left
      anchors.verticalCenter: parent.verticalCenter
      anchors.leftMargin : 10
      //font.pixelSize: 16
      font.bold: true
      text: section
      color:theme.buttonTextColor
   }
}