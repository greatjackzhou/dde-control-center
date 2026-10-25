import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import org.deepin.dtk 1.0 as D

D.Dialog {
    id: root

    property string mode: "add"   // "add" or "edit"
    property bool isSystem: false
    property string originalKey: ""
    property string varKey: ""
    property string varValue: ""
    // 标记本对话框已发起一次异步写操作，等待 operationResult 决定关闭或报错
    property bool pendingOperation: false

    title: mode === "add" ? qsTr("New Variable") : qsTr("Edit Variable")
    width: 420
    // 错误提示出现时对话框平滑增高，避免文案被裁切
    height: 260 + (errorLabel.visible ? errorLabel.implicitHeight + 8 : 0)
    // 样式的隐式尺寸与阴影留白会顶掉显式尺寸、裁切背景，这里统一归零自管
    implicitWidth: 420
    implicitHeight: 260
    modal: true
    anchors.centerIn: Overlay.overlay
    padding: 0
    margins: 0

    Behavior on height {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    // 调色板角色色可能带 alpha（DTK 的 window/base 为半透明），预合成到纯色上保证不透明
    function opaqueOver(color, over) {
        var a = over.a
        return Qt.rgba(over.r * a + color.r * (1 - a),
                       over.g * a + color.g * (1 - a),
                       over.b * a + color.b * (1 - a), 1.0)
    }

    // 默认 FloatingPanel 背景是半透明+窗口内模糊，模糊失效时整窗发虚，改为不透明圆角底
    background: Rectangle {
        radius: 12
        color: root.opaqueOver(D.DTK.palette.window, D.DTK.palette.window)
    }

    // 标题栏追加右上角关闭按钮（默认 Dialog 无叉可点）
    // 注意：Popup 布局会强制 header 的 y/width，在 header 上写 x/y 会把关闭按钮
    // 顶出右缘；RowLayout 自身无 padding，内边距只能挂到各子项的 Layout.margins
    header: RowLayout {
        spacing: 4

        Text {
            Layout.leftMargin: 20
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            text: root.title
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
            onClicked: root.close()
        }
    }

    onOpened: {
        keyField.text = varKey
        valueField.text = varValue
        keyField.forceActiveFocus()
        // 编辑模式下全选变量名，便于直接改名
        if (mode === "edit")
            keyField.selectAll()
        errorLabel.text = ""
    }

    onRejected: {
        clearFields()
    }

    function clearFields() {
        varKey = ""
        varValue = ""
        originalKey = ""
        mode = "add"
    }

    // 关闭时清除发起标记，避免残留的 operationResult 误触后续关闭
    onClosed: pendingOperation = false

    function doAccept() {
        var key = keyField.text.trim()
        var value = valueField.text.trim()

        // 验证
        if (key.length === 0) {
            errorLabel.text = qsTr("Variable name cannot be empty")
            return
        }
        if (!dccData.isValidVarName(key)) {
            errorLabel.text = qsTr("Invalid name: must start with letter or underscore, contain only letters, digits, and underscores")
            return
        }
        // 存储格式对含空格/冒号的值用双引号包裹且无法转义，值内出现 " 会使
        // 写读往返与 pam_env/systemd 解析损坏，故直接拒绝
        if (value.indexOf('"') >= 0) {
            errorLabel.text = qsTr("Variable value cannot contain double quotes (\")")
            return
        }

        // 发起异步操作：结果经 dccData.operationResult 回传，由下方 Connections 处理
        pendingOperation = true
        if (mode === "add") {
            if (isSystem)
                dccData.addSystemVar(key, value)
            else
                dccData.addUserVar(key, value)
        } else {
            if (isSystem)
                dccData.editSystemVar(originalKey, key, value)
            else
                dccData.editUserVar(originalKey, key, value)
        }
    }

    // 监听 C++ 异步操作结果：成功则清空并关闭，失败就地提示（弹窗不关闭，便于重试）
    Connections {
        target: dccData
        function onOperationResult(success, message) {
            if (!root.pendingOperation)
                return
            root.pendingOperation = false
            if (success) {
                root.clearFields()
                root.close()
            } else {
                errorLabel.text = message
            }
        }
    }

    // contentItem 已由 Popup 布局排在 header 下方，这里只需常规内边距
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 4
        anchors.bottomMargin: 10
        spacing: 12

        // 变量名
        RowLayout {
            spacing: 10
            Text {
                text: qsTr("Variable name:")
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                horizontalAlignment: Text.AlignRight
                font.pixelSize: D.DTK.fontManager.t6.pixelSize
                color: D.DTK.palette.windowText
            }
            D.TextField {
                id: keyField
                Layout.fillWidth: true
                placeholderText: qsTr("e.g. JAVA_HOME")
                onTextChanged: errorLabel.text = ""
                onAccepted: valueField.forceActiveFocus()
            }
        }

        // 变量值
        RowLayout {
            spacing: 10
            Text {
                text: qsTr("Variable value:")
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                horizontalAlignment: Text.AlignRight
                font.pixelSize: D.DTK.fontManager.t6.pixelSize
                color: D.DTK.palette.windowText
            }
            D.TextField {
                id: valueField
                Layout.fillWidth: true
                placeholderText: qsTr("e.g. /usr/lib/jvm/java-17")
                onTextChanged: errorLabel.text = ""
                onAccepted: root.doAccept()
            }
        }

        // 错误提示
        Text {
            id: errorLabel
            text: ""
            color: D.DTK.palette.window.lightness > 0.5 ? "#c0392b" : "#ff8787"
            font.pixelSize: D.DTK.fontManager.t8.pixelSize
            visible: text.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 110
            wrapMode: Text.Wrap
        }
    }

    // 底部按钮统一放入 footer，与 DTK 原生对话框按钮区一致
    // Layout.margins 对布局根项无效，左右内缩避开圆角需挂到子项上
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
            onClicked: {
                root.clearFields()
                root.close()
            }
        }

        D.RecommandButton {
            text: qsTr("OK")
            Layout.rightMargin: 16
            Layout.topMargin: 4
            Layout.bottomMargin: 12
            enabled: !dccData.saving
            onClicked: root.doAccept()
        }
    }
}
