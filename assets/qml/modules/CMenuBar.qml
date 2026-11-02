import QtQuick
import QtQuick.Controls.Material

MenuBar {
    id: menu
    font.pointSize: sm.sFont

    Menu {
        title: qsTr("&Файл")
        font.pointSize: sm.sFont
        width: sm.menuItemWidth
        Action { text: qsTr("Відкрити спектр")
            onTriggered: { fileDialog.open() }
        }
        Action { text: qsTr("Зберегти спектр") }
        Action { text: qsTr("Зберегти CSV") }
    }
    Menu {
        title: qsTr("&Логи")
        font.pointSize: sm.sFont
        width: sm.menuItemWidth

        Action { text: qsTr("Відкрити Інфо")
            onTriggered: { fileDialogLog.open(); openFileType = 0; }
        }
        Action { text: qsTr("Відкрити Сессії")
            onTriggered: { fileDialogLog.open(); openFileType = 1; }
        }
        Action { text: qsTr("Відкрити Девайси")
            onTriggered: { fileDialogLog.open(); openFileType = 2; }
        }
    }
    Menu {
        title: qsTr("Прилади")
        font.pointSize: sm.sFont
        width: sm.menuItemWidth

        Action { text: qsTr("Перепідключити")
            onTriggered: {
                var command = ["start_research"]
                dsContext.receive_data_from_QML(command)
                console.log(command)
            }
        }

        Action { text: qsTr("Зберегти темп. як зроблену")
            onTriggered: {
                var command = ["completed_temp"]
                sender.sendCommand(dsContext, command);
            }
        }

        MenuSeparator {}

        Action { text: qsTr("Відновити сессію")
            onTriggered: {
                fileDialogSession.open()
            }
        }
        Action { text: qsTr("Додати прилад до сесії")
            onTriggered: {
                fileDialogAddToSession.open()
            }
        }

        MenuSeparator {}

        Action { text: qsTr("Заархівувати дані")
            onTriggered: {
                var command = ["create_archive"]
                sender.sendCommand(dsContext, command);
            }
        }
    }
    Menu {
        title: qsTr("Помилки")
        font.pointSize: sm.sFont
        width: sm.menuItemWidth

        Action { text: qsTr("Зупинити таймер помилок")
            onTriggered: {
                var command = ["error_timer_stop"]
                dsContext.receive_data_from_QML(command)
                console.log(command)
            }
        }
    }
}
