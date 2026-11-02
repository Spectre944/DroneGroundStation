import QtQuick
import QtQuick.Controls.Material

Button {
    id: btn
    display: AbstractButton.TextBesideIcon
    icon.source: "qrc:/icons/svg/bs5/gear.svg"
    width: 10
    checkable: true
    autoExclusive: true
    flat: true
}
