import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material

import "../modules/buttons"

Item {
    id: root

    Layout.fillHeight: true
    Layout.preferredWidth: 30

    signal changePage(int pageIndex)

    Rectangle {
        id: leftMenuFrame
        anchors.fill: parent
        anchors.topMargin: 4    //Выровнял с высотой страницы SwipeView
        anchors.bottomMargin: 3
        anchors.leftMargin: -15 // Смещение ближе к левому краю
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            LeftMenuButton {
                id: leftMenuBtnDev
                icon.source: "qrc:/icons/svg/bs5/house.svg"
                checked: true
                onClicked: root.changePage(0)
            }

            LeftMenuButton {
                id: leftMenuBtnDatabase
                icon.source: "qrc:/icons/svg/bs5/database.svg"
                onClicked: root.changePage(1)
            }

            LeftMenuButton {
                id: leftMenuBtnReport
                icon.source: "qrc:/icons/svg/bs5/journal-bookmark.svg"
                onClicked: root.changePage(2)
            }

            LeftMenuButton {
                id: leftMenuBtnLog
                icon.source: "qrc:/icons/svg/bs5/terminal.svg"
                onClicked: root.changePage(3)
            }

            Item {
                Layout.fillHeight: true
            }

            LeftMenuButton {
                id: leftMenuBtnSett
                icon.source: "qrc:/icons/svg/bs5/gear.svg"
                onClicked: root.changePage(4)
            }
        }
    }
}
