import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import Qt5Compat.GraphicalEffects

import "settings"
import "modules"
import "pages"
import "modules/container"

ApplicationWindow  {
    id: app
    width: 1280
    height: 720
    visible: true
    title: qsTr("RCB")

    //Material.theme: Material.Dark
    Material.accent: sm.accentColor

    Settings { id: sm }

    menuBar: CMenuBar { id: menu }

    ProgrammInfo {
        anchors.top: menu.top
        anchors.bottom: menu.bottom
        anchors.right: menu.right
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: sm.backgroundColor

        RowLayout {
            anchors.fill: parent
            spacing: 0

            LeftMenu {
                id: leftMenu

                onChangePage: function(index) {
                    console.log("Page index:", index)
                    swipeView.currentIndex = index
                }

            }

            Rectangle {
                id: appContent
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: "transparent"

                SwipeView {
                    id: swipeView
                    anchors.fill: parent
                    //anchors.margins: 10
                    interactive: true
                    clip: true

                    PageDevice {

                    }

                    PageDatabase {

                    }

                }

            }
        }

        PageIndicator {
            id: indicator

            count: swipeView.count
            currentIndex: swipeView.currentIndex
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: -5
        }
    }

    RowLayout {
        id: overlayLayout
        anchors.fill: parent
        //anchors.margins: 10

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }

        CRectangleNoise {

        }
    }

    Connections { target: smContext }
}
