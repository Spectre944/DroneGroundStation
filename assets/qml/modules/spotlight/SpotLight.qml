import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: overlay
    width: parent.width
    height: parent.height
    color: "black"
    opacity: 0.7  // затемнение

    Rectangle {
        id: spotlight
        x: spotlightX
        y: spotlightY
        width: spotlightWidth
        height: spotlightHeight
        color: "transparent"
        border.color: "orange"
        border.width: 2
    }

    Text {
        id: instruction
        text: "Click here"
        color: "white"
        x: spotlight.x + spotlight.width / 2 - width / 2
        y: spotlight.y + spotlight.height + 10
    }

    property int spotlightX: 0
    property int spotlightY: 0
    property int spotlightWidth: 100
    property int spotlightHeight: 50
}
