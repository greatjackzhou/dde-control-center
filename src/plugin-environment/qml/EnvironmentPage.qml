import org.deepin.dcc 1.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import org.deepin.dtk 1.0 as D

DccObject {
    id: root
    pageType: DccObject.Item
    page: Item {
        implicitWidth: 800
        implicitHeight: contentColumn.implicitHeight + 40

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 20
            spacing: 12
            width: parent.width - 48

            // ========== 用户环境变量区域 ==========
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: qsTr("User Variables for %1").arg(dccData.userName)
                    font.pixelSize: D.DTK.fontManager.t6.pixelSize
                    font.bold: true
                    color: D.DTK.palette.windowText
                }
                Text {
                    text: qsTr("These variables apply only to your user account (~/.pam_environment)")
                    font.pixelSize: D.DTK.fontManager.t8.pixelSize
                    color: D.DTK.palette.windowText
                    opacity: 0.6
                }
            }

            VariableListView {
                id: userListView
                Layout.fillWidth: true
                Layout.preferredHeight: 240
                model: dccData.userModel
                isSystem: false

                onAddRequested: {
                    addEditDialog.mode = "add"
                    addEditDialog.isSystem = false
                    addEditDialog.open()
                }
                onEditRequested: (key, value) => {
                    if (key === "PATH") {
                        pathEditDialog.isSystem = false
                        pathEditDialog.open()
                    } else {
                        addEditDialog.mode = "edit"
                        addEditDialog.isSystem = false
                        addEditDialog.originalKey = key
                        addEditDialog.varKey = key
                        addEditDialog.varValue = value
                        addEditDialog.open()
                    }
                }
                onDeleteRequested: (key) => {
                    confirmDeleteDialog.varKey = key
                    confirmDeleteDialog.isSystem = false
                    confirmDeleteDialog.open()
                }
            }

            // ========== 系统环境变量区域 ==========
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: qsTr("System Variables")
                    font.pixelSize: D.DTK.fontManager.t6.pixelSize
                    font.bold: true
                    color: D.DTK.palette.windowText
                }
                Text {
                    text: qsTr("These variables apply to all users (/etc/environment)")
                    font.pixelSize: D.DTK.fontManager.t8.pixelSize
                    color: D.DTK.palette.windowText
                    opacity: 0.6
                }
            }

            VariableListView {
                id: systemListView
                Layout.fillWidth: true
                Layout.preferredHeight: 240
                model: dccData.systemModel
                isSystem: true

                onAddRequested: {
                    addEditDialog.mode = "add"
                    addEditDialog.isSystem = true
                    addEditDialog.open()
                }
                onEditRequested: (key, value) => {
                    if (key === "PATH") {
                        pathEditDialog.isSystem = true
                        pathEditDialog.open()
                    } else {
                        addEditDialog.mode = "edit"
                        addEditDialog.isSystem = true
                        addEditDialog.originalKey = key
                        addEditDialog.varKey = key
                        addEditDialog.varValue = value
                        addEditDialog.open()
                    }
                }
                onDeleteRequested: (key) => {
                    confirmDeleteDialog.varKey = key
                    confirmDeleteDialog.isSystem = true
                    confirmDeleteDialog.open()
                }
            }

            // 生效说明：变更后自动同步运行时环境，无需注销
            Text {
                Layout.fillWidth: true
                text: qsTr("Changes are synced automatically without logging out. Newly launched programs take effect immediately; already running programs need to be restarted.")
                wrapMode: Text.WordWrap
                font.pixelSize: D.DTK.fontManager.t8.pixelSize
                color: D.DTK.palette.windowText
                opacity: 0.6
            }

            // 操作结果提示：成功绿色、失败红色，短暂展示后淡出消失
            Text {
                id: resultTip
                property bool isError: false
                readonly property bool isLightTheme: D.DTK.palette.window.lightness > 0.5
                text: ""
                visible: opacity > 0
                opacity: text.length > 0 ? 1.0 : 0.0
                color: isError
                    ? (isLightTheme ? "#c0392b" : "#ff8787")
                    : (isLightTheme ? "#2e9e5b" : "#67d78a")
                font.pixelSize: D.DTK.fontManager.t8.pixelSize
                Layout.fillWidth: true
                wrapMode: Text.WordWrap

                Behavior on opacity {
                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                }

                Timer {
                    id: tipTimer
                    interval: resultTip.isError ? 5000 : 3000
                    onTriggered: resultTip.text = ""
                }
            }
        }

        // 新增/编辑对话框
        AddEditDialog {
            id: addEditDialog
        }

        // PATH 编辑对话框
        PathEditDialog {
            id: pathEditDialog
        }

        // 删除环境变量二次确认：避免误点一键直接删除持久化数据
        D.Dialog {
            id: confirmDeleteDialog
            property string varKey: ""
            property bool isSystem: false

            title: qsTr("Delete Variable?")
            width: 400
            height: 190
            implicitWidth: 400
            implicitHeight: 190
            modal: true
            anchors.centerIn: Overlay.overlay
            padding: 0
            margins: 0

            // 半透明角色色预合成，保证弹窗背景不透明（同 AddEditDialog）
            function opaqueOver(color, over) {
                var a = over.a
                return Qt.rgba(over.r * a + color.r * (1 - a),
                               over.g * a + color.g * (1 - a),
                               over.b * a + color.b * (1 - a), 1.0)
            }

            background: Rectangle {
                radius: 12
                color: confirmDeleteDialog.opaqueOver(D.DTK.palette.window, D.DTK.palette.window)
            }

            // 内边距挂子项 Layout.margins：Popup 会接管 header 的 y/width，RowLayout 无自带 padding
            header: RowLayout {
                spacing: 4

                Text {
                    Layout.leftMargin: 20
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                    text: confirmDeleteDialog.title
                    font.pixelSize: D.DTK.fontManager.t5.pixelSize
                    font.bold: true
                    color: D.DTK.palette.windowText
                    elide: Text.ElideRight
                }

                Item { Layout.fillWidth: true }

                D.Button {
                    flat: true
                    text: "✕"
                    font.pixelSize: D.DTK.fontManager.t6.pixelSize
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    Layout.rightMargin: 14
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                    padding: 0
                    onClicked: confirmDeleteDialog.close()
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 4
                anchors.bottomMargin: 10
                spacing: 6

                Text {
                    text: qsTr("Are you sure you want to delete environment variable \"%1\"? This action cannot be undone.").arg(confirmDeleteDialog.varKey)
                    font.pixelSize: D.DTK.fontManager.t7.pixelSize
                    color: D.DTK.palette.windowText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }

            footer: RowLayout {
                spacing: 10

                Item {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                }

                D.Button {
                    text: qsTr("Cancel")
                    flat: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 12
                    onClicked: confirmDeleteDialog.close()
                }

                D.WarningButton {
                    text: qsTr("Delete")
                    Layout.rightMargin: 16
                    Layout.topMargin: 4
                    Layout.bottomMargin: 12
                    onClicked: {
                        if (confirmDeleteDialog.isSystem)
                            dccData.removeSystemVar(confirmDeleteDialog.varKey)
                        else
                            dccData.removeUserVar(confirmDeleteDialog.varKey)
                        confirmDeleteDialog.close()
                    }
                }
            }
        }

        // 监听 C++ 操作结果
        Connections {
            target: dccData
            function onOperationResult(success, message) {
                resultTip.text = message
                resultTip.isError = !success
                tipTimer.restart()
            }
        }
    }
}
