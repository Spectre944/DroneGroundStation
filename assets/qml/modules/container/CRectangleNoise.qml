import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import Qt5Compat.GraphicalEffects

Rectangle {
    id: blurRect

    Layout.fillHeight: true
    Layout.preferredWidth: 300

    radius: 10

    color: "white"
    opacity: 0.5

    property int    blurValue:          10
    property real   brightnessValue:    0.1
    property real   toneValue:          0.65
    property real   imgOpacityValue:    0.1

/*
    ColumnLayout {
        anchors.fill: parent

        Slider {
            Layout.fillWidth: true
            id: opacitySlider
            value: 0.5
            from: 0
            to: 1.0
        }

        Slider {
            Layout.fillWidth: true
            id: blurSlider
            value: 0
            from: 0
            to: 200
        }

        Slider {
            Layout.fillWidth: true
            id: brightnesSlider
            value: 0.1
            from: 0
            to: 1.0
        }

        Slider {
            Layout.fillWidth: true
            id: toneSlider
            value: 0.65
            from: 0
            to: 1.0
        }
        Slider {
            Layout.fillWidth: true
            id: imageOpacity
            value: 0.1
            from: 0
            to: 1.0
        }
    }
*/

    // Источник для эффекта размытия
    ShaderEffectSource {
        id: effect_source
        anchors.fill: parent
        visible: false
        sourceItem: parent
    }

    // Эффект размытия
    FastBlur {
        id: fast_blur
        anchors.fill: parent
        source: effect_source
        radius: blurValue
    }

    // Прямоугольник для затемнения и яркости
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(1, 1, 1, brightnessValue)  // Минимальная яркость
        radius: 10
    }

    // Прямоугольник для тонировки
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(1, 1, 1, toneValue)  // Тонировка
        radius: 10
    }

    // Изображение для добавления шума (шероховатости)
    Image {
        anchors.fill: parent
        source: "qrc:/icons/effects/Noise-2.png"
        fillMode: Image.Tile
        opacity: imgOpacityValue
    }
}
