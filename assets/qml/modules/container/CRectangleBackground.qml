import QtQuick
import Qt5Compat.GraphicalEffects

Rectangle {
    id: backgroundRectangle
    radius: 10
    color: sm.containerColor
    anchors.fill: parent
    anchors.margins: 10
    layer.enabled: true
    layer.effect: DropShadow {
        color: sm.shadowColor
        transparentBorder: true
        horizontalOffset: 2
        verticalOffset: 4
        radius: 4
    }
}
