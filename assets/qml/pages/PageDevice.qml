import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material

import "../modules/container"

Page {
    id: root
    background: Rectangle { color: "transparent" }

    CRectangleBackground {
        id: background

        Text {
            id: namea
            anchors.fill: parent
            text: qsTr("-------------------------------------------------------------------------------------------------")
        }
    }

}
