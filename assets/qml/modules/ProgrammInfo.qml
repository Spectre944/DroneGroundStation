import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material

import "buttons"

Item {
    id: root

    property int containerItems: 5
    property int containerWidth: 50

    width: containerItems * containerWidth
    z: 10

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        anchors.rightMargin: 10
        clip: true

        // IconImage {
        //     Layout.fillHeight: true
        //     Layout.preferredWidth: containerWidth
        //     source: "qrc:/icons/svg/bs5/bell-fill.svg"
        // }

        // IconButtonFlat {
        //     Layout.fillHeight: true
        //     Layout.preferredWidth: containerWidth
        //     icon.source: "qrc:/icons/svg/bs5/play-fill.svg"
        // }

        // IconButtonFlat {
        //     Layout.fillHeight: true
        //     Layout.preferredWidth: containerWidth
        //     icon.source: "qrc:/icons/svg/bs5/bell-fill.svg"
        // }


        Rectangle {
            id: content
            Layout.fillHeight: true
            Layout.fillWidth: true
            radius: 20
            color: sm.accentColor

            RowLayout {
                anchors.fill: parent
                anchors.margins: 0

                IconImage {
                    Layout.fillHeight: true
                    Layout.preferredWidth: containerWidth
                    source: "qrc:/icons/svg/bs5/headset.svg"
                }

                IconImage {
                    Layout.fillHeight: true
                    Layout.preferredWidth: containerWidth
                    source: "qrc:/icons/svg/bs5/exclamation-triangle.svg"
                }

                IconImage {
                    Layout.fillHeight: true
                    Layout.preferredWidth: containerWidth
                    source: "qrc:/icons/svg/bs5/exclamation-triangle.svg"
                }
            }
        }

    }







}
