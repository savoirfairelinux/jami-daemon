import QtQuick 1.1
import org.kde.plasma.components 0.1 as Plasma

Rectangle {
    id:dialPad
    width:parent.width
    height:200
    color:theme.viewBackgroundColor

    signal numbrePressed(string number)
    function getNumber(idx) {
        var nb = new Array("1","2","3","4","5","6","7","8","9","*","0","#")
        return nb[idx]
    }
    Grid {
        columns: 3
        spacing: 3
        width:parent.width
        height:50*4
        Repeater {
            model: 12
            Plasma.Button {
                width:parent.width/3 -1
                height:48
                text:getNumber(index)+ "\n" + Array("","abc","def","ghi","jkl","mno","pqrs","tuv","wxyz","","+","","")[index]
                onClicked: {
                   playDMTF(index)
                   dialPad.numbrePressed(dialPad.getNumber(index))
                }
            }
        }
    }

    Component.onCompleted: {
       dialPad.numbrePressed.connect(display.addText)
    }
}
