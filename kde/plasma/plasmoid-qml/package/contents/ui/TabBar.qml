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

//Plasma.TagGroup {
Plasma.TabBar {
    id:tabBar
    width:parent.width

    function displayTabs(index) {
        switch (index) {
            case 0:
                contactList.visible = false
                callList.visible    = true
                historyList.visible = false
                bookmarkList.visible= false
                settingsTab.visible = false
                break;
            case 1:
                contactList.visible = true
                callList.visible    = false
                historyList.visible = false
                bookmarkList.visible= false
                settingsTab.visible = false
                break;
            case 2:
                contactList.visible = false
                historyList.visible = true
                callList.visible    = false
                bookmarkList.visible= false
                settingsTab.visible = false
                break;
            case 3:
                contactList.visible = false
                historyList.visible = false
                callList.visible    = false
                bookmarkList.visible= true
                settingsTab.visible = false
                break;
            case 4:
                contactList.visible = false
                callList.visible    = false
                historyList.visible = false
                bookmarkList.visible= false
                settingsTab.visible = true
               break
        }
    }

    //Buggy, awaiting fix upstream, test often
   /*Repeater {
      model:5
      Plasma.TabButton {
         text: Array("New calls","Contacts","History","Bookmarks","Settings")[index]
         onPressedChanged: {
            tabBar.displayTabs(index)
         }
      }
   }*/
   Plasma.TabButton {
      text: "New calls"
      iconSource: "call-start"
      onPressedChanged: {
         tabBar.displayTabs(0)
      }
   }

   Plasma.TabButton {
      text: "Contacts"
      iconSource: "user-identity"
      onPressedChanged: {
         tabBar.displayTabs(1)
      }
   }

   Plasma.TabButton {
      text: "History"
      iconSource: "view-history"
      onPressedChanged: {
         tabBar.displayTabs(2)
      }
   }

   Plasma.TabButton {
      text: "Bookmarks"
      iconSource: "favorites"
      onPressedChanged: {
         tabBar.displayTabs(3)
      }
   }

   Plasma.TabButton {
      iconSource: "configure"
      text:"Configure"
      onPressedChanged: {
         tabBar.displayTabs(4)
      }
   }
}
//}
