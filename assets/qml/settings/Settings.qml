import QtQuick
import QtQuick.Controls.Material

Item {

    //toolTip param 1 sec and 5 sec
    property bool ttEnable: smContext.getValue("ui/tooltip/enabled")
    property real ttDelay: 1000
    property real ttDuration: 5000


    property real fontDelta: smContext.getValue("ui/font/delta") || 2
    property real bFont: mFont + fontDelta
    property real mFont: smContext.getValue("ui/font/size") || 14
    property real sFont: mFont - fontDelta
    property real xsFont: 10

    property var currentTheme: themes.material

    //application colore
    QtObject {
        id: themes
        //readonly property var light: ["#07130f", "#daf1ea", "#b1ecd8", "#325fc8", "#315fc9"]
        //                              text      backgr     primary    second     accent
        readonly property var light: ["#07130f", "#daf1ea", "#b1ecd8", "#325fc8", "#315fc9"]
        readonly property var dark: ["#daf1ea", "#07130f", "#73338e", "#010403", "#a9bdea"]
        readonly property var material: ["#000000", "#dbdcdb", "#ff5722", "#ff9800", "#ff5722"]
        readonly property var test: ["#0c0a0c", "#faf7fa", "#a453af", "#d396db", "#c95bd8"]
    }

    property color foregroundColor: currentTheme[0]
    property color backgroundColor: currentTheme[1]
    property color primaryColor:    currentTheme[2]
    property color secondaryColor:  currentTheme[3]
    property color accentColor:     currentTheme[4]
    property color acceptColor:     "#0e8136"
    property color warningColor:    "#d5d300"
    property color errorColor:      "#d52f00"

    property real darkScale: 2
    property real lightScale: 0.4

    property color containerColor:     Qt.lighter(backgroundColor, darkScale)
    property color subContainerColor:  Qt.darker(containerColor, lightScale)
    property color shadowColor:        Qt.lighter(backgroundColor, lightScale)
    property color spliterColor:       Qt.lighter(backgroundColor, lightScale)
    property color subForegroundColor: Qt.lighter(backgroundColor, lightScale)

}



