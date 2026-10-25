import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DStyle

// 独立模态对话框窗口：真正的窗口模态（blocking 主窗口交互），
// 标题栏自带 ✕，窗口尺寸由内容隐式尺寸决定，不会再出现内容被窗口裁切
D.DialogWindow {
    id: root

    property bool isSystem: false
    property var pathEntries: []
    // 打开时的列表快照：关闭时若内容有差异则提示放弃确认
    property string initialSnapshot: ""
    // 标记本对话框已发起一次异步 PATH 保存，等待 operationResult 决定关闭或报错
    property bool pendingPathSave: false

    function currentSnapshot() {
        var entries = []
        for (var i = 0; i < pathListModel.count; i++)
            entries.push(pathListModel.get(i).path)
        return entries.join("\n")
    }

    // 统一关闭入口：有未保存改动时先征求确认，避免误点丢失编辑成果
    function requestClose() {
        if (currentSnapshot() !== initialSnapshot)
            confirmDiscard.open()
        else
            hide()
    }

    // 保持调用方 open()/关闭 的使用习惯：Window 以 visible 切换复用
    function open() { visible = true }

    modality: Qt.ApplicationModal
    // 标题直接放进窗口标题栏（自带 ✕ 仍由 DialogTitleBar 提供）
    header: D.DialogTitleBar {
        title: qsTr("Edit PATH")
        enableInWindowBlendBlur: false
    }
    // 窗口尺寸取内容隐式尺寸并锁死 min=max（标题栏本就禁用缩放），
    // 保证窗口永远不小于一屏内容，也不会被外部 WM 压小
    minimumWidth: dialogContent.implicitWidth + 2 * DStyle.Style.dialogWindow.contentHMargin
    minimumHeight: dialogContent.implicitHeight + DStyle.Style.dialogWindow.titleBarHeight
    maximumWidth: minimumWidth
    maximumHeight: minimumHeight

    onVisibleChanged: {
        if (visible) {
            pathEntries = dccData.getPathEntries(isSystem)
            pathListModel.clear()
            for (var i = 0; i < pathEntries.length; i++) {
                pathListModel.append({path: pathEntries[i]})
            }
            pathListView.currentIndex = -1
            initialSnapshot = pathEntries.join("\n")
            saveError.text = ""
            x = Math.round((Screen.width - width) / 2)
            y = Math.round((Screen.height - height) / 2)
            raise()
            requestActivate()
        } else if (!confirmDiscard.visible && currentSnapshot() !== initialSnapshot) {
            // 标题栏 ✕/WM 关闭时若有未保存改动：DialogWindow 会直接 hide，
            // 这里立即拉回并弹放弃确认；保存/放弃路径都会先同步快照再隐藏，不会循环
            visible = true
            confirmDiscard.open()
        }
    }

    // 调色板角色色可能带 alpha（DTK 的 base/window 为半透明），
    // 预合成到纯色上保证不透明
    function opaqueOver(color, over) {
        var a = over.a
        return Qt.rgba(over.r * a + color.r * (1 - a),
                       over.g * a + color.g * (1 - a),
                       over.b * a + color.b * (1 - a), 1.0)
    }

    // 打开内联编辑框修改指定索引的路径
    function openEditAtIndex(index) {
        editPathDialog.mode = "edit"
        editPathDialog.pathText = pathListModel.get(index).path
        editPathDialog.editIndex = index
        editPathDialog.open()
    }

    // 对话框主体：其隐式尺寸直接决定窗口固定大小（min=max），
    // 标题/列表/操作按钮/底部按钮全部计入，不存在被窗口裁切的可能
    ColumnLayout {
        id: dialogContent
        spacing: 10

        // 路径列表
        Rectangle {
            id: listContainer
            Layout.fillWidth: true
            Layout.preferredWidth: 560
            Layout.topMargin: 10
            // 高度随条目数自适应（窗口是内容驱动定高，条目少时不留大片空白），
            // 封顶后超出部分滚动
            Layout.preferredHeight: Math.max(80, Math.min(220, pathListModel.count * 36 + 10))
            radius: 6
            // base 角色为半透明，预合成到 window 底色上，避免列表发虚
            color: root.opaqueOver(D.DTK.palette.window, D.DTK.palette.base)
            border.width: 1
            border.color: D.DTK.palette.midlight

            ListView {
                id: pathListView
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                focus: true
                // DialogWindow 内容槽只接受 Item，ListModel 内联声明在此
                model: ListModel {
                    id: pathListModel
                    // 列表再次变动后清除旧的保存失败提示，避免误导
                    onRowsInserted: saveError.text = ""
                    onRowsRemoved: saveError.text = ""
                    onCountChanged: saveError.text = ""
                }
                currentIndex: -1

                // 键盘快捷键：Delete 删除、Enter 编辑、Ctrl+Up/Down 移动
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Delete && currentIndex >= 0) {
                        pathListModel.remove(currentIndex)
                        currentIndex = Math.min(currentIndex, pathListModel.count - 1)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Return && currentIndex >= 0) {
                        root.openEditAtIndex(currentIndex)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Up && (event.modifiers & Qt.ControlModifier) && currentIndex > 0) {
                        moveCurrentUp()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Down && (event.modifiers & Qt.ControlModifier) && currentIndex >= 0 && currentIndex < pathListModel.count - 1) {
                        moveCurrentDown()
                        event.accepted = true
                    }
                }

                function moveCurrentUp() {
                    var idx = currentIndex
                    if (idx > 0) {
                        // get() 返回的是绑定在行上的代理对象，remove 后即失效，
                        // 必须先把数据拷成普通对象再 insert，否则插入的是空行
                        var item = { path: pathListModel.get(idx).path }
                        pathListModel.remove(idx)
                        pathListModel.insert(idx - 1, item)
                        currentIndex = idx - 1
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }
                }

                function moveCurrentDown() {
                    var idx = currentIndex
                    if (idx >= 0 && idx < pathListModel.count - 1) {
                        var item = { path: pathListModel.get(idx).path }
                        pathListModel.remove(idx)
                        pathListModel.insert(idx + 1, item)
                        currentIndex = idx + 1
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }
                }

                delegate: ItemDelegate {
                    id: pathDelegate
                    width: pathListView.width
                    height: 36
                    highlighted: ListView.isCurrentItem
                    onClicked: pathListView.currentIndex = index
                    onDoubleClicked: {
                        pathListView.currentIndex = index
                        root.openEditAtIndex(index)
                    }

                    // 选中行用不透明高亮底（预合成避免半透叠加变淡），
                    // 悬停行淡蓝反馈；配合 highlightedText 任意主题下都清晰可辨
                    background: Rectangle {
                        radius: 4
                        color: pathDelegate.highlighted
                            ? root.opaqueOver(listContainer.color, D.DTK.palette.highlight)
                            : (pathDelegate.hovered
                                ? Qt.alpha(D.DTK.palette.highlight, 0.12)
                                : "transparent")
                    }

                    // 长路径悬浮查看完整内容：ToolTip 挂在 ItemDelegate(Control) 上
                    ToolTip.delay: 600
                    ToolTip.timeout: 8000
                    ToolTip.visible: pathDelegate.hovered && pathText.truncated
                    ToolTip.text: model.path

                    contentItem: Text {
                        id: pathText
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        text: model.path
                        font.pixelSize: D.DTK.fontManager.t7.pixelSize
                        color: pathDelegate.highlighted
                            ? D.DTK.palette.highlightedText
                            : D.DTK.palette.text
                        elide: Text.ElideMiddle
                        width: parent.width - 20
                    }
                }

                ScrollBar.vertical: D.ScrollBar { }
            }

            // 空列表占位提示：同时引导主操作入口
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 4
                visible: pathListModel.count === 0
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No path entries")
                    font.pixelSize: D.DTK.fontManager.t8.pixelSize
                    color: D.DTK.palette.text
                    opacity: 0.6
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Click New to add path entries")
                    font.pixelSize: D.DTK.fontManager.t8.pixelSize
                    color: D.DTK.palette.text
                    opacity: 0.4
                }
            }
        }

        // 操作按钮栏：左侧主操作 + 条目计数，右侧紧凑排序箭头组
        // 不设固定宽：由子项隐式宽度撑开，作为整个对话框的宽度基准
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            // "新建"为主操作，视觉突出
            D.RecommandButton {
                text: qsTr("New")
                onClicked: {
                    editPathDialog.mode = "add"
                    editPathDialog.pathText = ""
                    editPathDialog.editIndex = -1
                    editPathDialog.open()
                }
            }

            D.Button {
                text: qsTr("Edit")
                flat: true
                enabled: pathListView.currentIndex >= 0
                onClicked: {
                    if (pathListView.currentIndex >= 0)
                        root.openEditAtIndex(pathListView.currentIndex)
                }
            }

            D.Button {
                text: qsTr("Delete")
                flat: true
                enabled: pathListView.currentIndex >= 0
                onClicked: {
                    if (pathListView.currentIndex >= 0) {
                        pathListModel.remove(pathListView.currentIndex)
                        pathListView.currentIndex = Math.min(pathListView.currentIndex, pathListModel.count - 1)
                    }
                }
            }

            Item { Layout.fillWidth: true }

            D.Button {
                text: qsTr("Move Up")
                flat: true
                // 同时压低 minimum/preferred，否则 DTK 按钮隐式最小宽会撑宽整个窗口
                Layout.minimumWidth: 80
                Layout.preferredWidth: 80
                enabled: pathListView.currentIndex > 0
                onClicked: pathListView.moveCurrentUp()
            }

            D.Button {
                text: qsTr("Move Down")
                flat: true
                Layout.minimumWidth: 80
                Layout.preferredWidth: 80
                enabled: pathListView.currentIndex >= 0 && pathListView.currentIndex < pathListModel.count - 1
                onClicked: pathListView.moveCurrentDown()
            }
        }

        // 保存失败就地提示（弹窗不关闭，便于重试）
        Text {
            id: saveError
            text: ""
            visible: text.length > 0
            color: D.DTK.palette.window.lightness > 0.5 ? "#c0392b" : "#ff8787"
            font.pixelSize: D.DTK.fontManager.t8.pixelSize
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        // 底部确定/取消：并入主体布局，随隐式尺寸一起参与窗口定高
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 560
            Layout.topMargin: 4
            Layout.bottomMargin: 12
            spacing: 10

            Item { Layout.fillWidth: true }

            D.Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: root.requestClose()
            }

            D.RecommandButton {
                text: qsTr("OK")
                enabled: !dccData.saving
                onClicked: {
                    var entries = []
                    for (var i = 0; i < pathListModel.count; i++) {
                        entries.push(pathListModel.get(i).path)
                    }
                    // 发起异步保存：结果经 dccData.operationResult 回传，由下方 Connections 处理
                    root.pendingPathSave = true
                    dccData.setPathEntries(entries, root.isSystem)
                }
            }
        }
    }

    // 监听 C++ 异步 PATH 保存结果：成功先同步快照再隐藏（避免被关闭守卫拉回），失败就地提示
    // DialogWindow 的默认属性是 list<Item>，Connections 不是 Item，必须包一层零尺寸 Item
    Item {
        width: 0
        height: 0

        Connections {
            target: dccData
            function onOperationResult(success, message) {
                if (!root.pendingPathSave)
                    return
                root.pendingPathSave = false
                if (success) {
                    root.initialSnapshot = root.currentSnapshot()
                    root.hide()
                } else {
                    saveError.text = qsTr("Save failed, please retry or check permissions")
                }
            }
        }
    }

    // DialogWindow 的默认属性只接受 Item，子 Popup/FolderDialog 存入这个零尺寸占位项
    Item {
        width: 0
        height: 0

    // 未保存改动放弃确认：继承主对话框的视觉基调（不透明底 + 带叉标题栏）
    D.Dialog {
        id: confirmDiscard
        title: qsTr("Discard changes?")
        width: 380
        height: 170
        implicitWidth: 380
        implicitHeight: 170
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 0
        margins: 0

        background: Rectangle {
            radius: 12
            color: root.opaqueOver(D.DTK.palette.window, D.DTK.palette.window)
        }

        header: RowLayout {
            spacing: 4

            Text {
                Layout.leftMargin: 20
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                text: confirmDiscard.title
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
                onClicked: confirmDiscard.close()
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.topMargin: 4
            anchors.bottomMargin: 10

            Text {
                text: qsTr("You have unsaved changes to PATH. Discard them?")
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
                text: qsTr("Keep Editing")
                flat: true
                Layout.topMargin: 4
                Layout.bottomMargin: 12
                onClicked: confirmDiscard.close()
            }

            D.WarningButton {
                text: qsTr("Discard")
                Layout.rightMargin: 16
                Layout.topMargin: 4
                Layout.bottomMargin: 12
                onClicked: {
                    confirmDiscard.close()
                    // 先同步快照再隐藏，让关闭守卫视为无改动而真正隐藏
                    root.initialSnapshot = root.currentSnapshot()
                    root.hide()
                }
            }
        }
    }

    // 内联路径编辑对话框
    D.Dialog {
        id: editPathDialog
        property string mode: "add"
        property string pathText: ""
        property int editIndex: -1

        title: mode === "add" ? qsTr("Add Path") : qsTr("Edit Path")
        width: 450
        // 显式给定高度并随错误/提示行平滑增高；需容纳 header(≈46) + 输入行 + footer(≈52)，
        // 过小会挤压编辑区导致内容溢出
        height: 180 + (pathError.visible ? pathError.implicitHeight + 6 : 0)
                  + (relativeHint.visible ? relativeHint.implicitHeight + 6 : 0)
        implicitWidth: 450
        implicitHeight: 180
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 0
        margins: 0

        // 同主对话框：不透明背景 + 标题栏关闭按钮
        background: Rectangle {
            radius: 12
            color: root.opaqueOver(D.DTK.palette.window, D.DTK.palette.window)
        }

        header: RowLayout {
            spacing: 4

            Text {
                Layout.leftMargin: 20
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                text: editPathDialog.title
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
                onClicked: editPathDialog.close()
            }
        }

        Behavior on height {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        onOpened: {
            pathInput.text = pathText
            pathError.text = ""
            pathInput.forceActiveFocus()
            // 编辑模式全选原路径，便于直接改名
            if (mode === "edit")
                pathInput.selectAll()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: 4
            anchors.bottomMargin: 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: qsTr("Path:")
                    font.pixelSize: D.DTK.fontManager.t6.pixelSize
                    color: D.DTK.palette.windowText
                }

                D.TextField {
                    id: pathInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("/usr/local/bin")
                    onTextChanged: {
                        pathError.text = ""
                        // 非绝对路径只作橙色软提示，不阻断提交（与 Windows 行为一致）
                        var t = text.trim()
                        relativeHint.text = (t.length > 0 && !t.startsWith("/"))
                            ? qsTr("This is not an absolute path and may not work as expected")
                            : ""
                    }
                    onAccepted: editPathDialog.doConfirm()
                }

                D.Button {
                    text: qsTr("Browse...")
                    onClicked: {
                        // 已输入绝对路径时，从该位置打开目录选择器
                        var cur = pathInput.text.trim()
                        if (cur.length > 0 && cur.startsWith("/"))
                            fileDialog.currentFolder = Qt.url("file://" + cur.substring(0, cur.lastIndexOf("/")))
                        fileDialog.open()
                    }
                }
            }

            // 非绝对路径软提示（橙色，不阻断）
            Text {
                id: relativeHint
                text: ""
                visible: text.length > 0
                color: D.DTK.palette.window.lightness > 0.5 ? "#b45d06" : "#f0b060"
                font.pixelSize: D.DTK.fontManager.t8.pixelSize
                Layout.fillWidth: true
                Layout.leftMargin: 4
                wrapMode: Text.Wrap
            }

            // 输入错误提示
            Text {
                id: pathError
                text: ""
                visible: text.length > 0
                color: D.DTK.palette.window.lightness > 0.5 ? "#c0392b" : "#ff8787"
                font.pixelSize: D.DTK.fontManager.t8.pixelSize
                Layout.fillWidth: true
                Layout.leftMargin: 4
                wrapMode: Text.Wrap
            }
        }

        function doConfirm() {
            // Windows 式 PATH 编辑：每行严格一个路径。':' 是 PATH 的段分隔符，
            // 单个条目内不得出现，否则会与底层 ':' 拼接冲突、重开时被拆成多行。
            var path = pathInput.text.trim()
            if (path.length === 0) {
                pathError.text = qsTr("Path cannot be empty")
                return
            }
            if (path.indexOf(':') >= 0) {
                pathError.text = qsTr("A path entry cannot contain ':'; add one path per line")
                return
            }
            // 存储格式用双引号包裹含空格/冒号的值且无法转义，路径内的 "
            // 会破坏引号配对，导致读写往返与 pam_env/systemd 解析出错
            if (path.indexOf('"') >= 0) {
                pathError.text = qsTr("A path entry cannot contain double quotes (\")")
                return
            }
            for (var i = 0; i < pathListModel.count; i++) {
                if (i !== editIndex && pathListModel.get(i).path === path) {
                    pathError.text = qsTr("Path already exists")
                    return
                }
            }

            if (mode === "add") {
                pathListModel.append({path: path})
                pathListView.currentIndex = pathListModel.count - 1
                pathListView.positionViewAtIndex(pathListView.currentIndex, ListView.End)
            } else if (editIndex >= 0) {
                pathListModel.set(editIndex, {path: path})
            }
            editPathDialog.close()
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
                onClicked: editPathDialog.close()
            }

            D.RecommandButton {
                text: qsTr("OK")
                Layout.rightMargin: 16
                Layout.topMargin: 4
                Layout.bottomMargin: 12
                // 必须限定对象名调用：doConfirm 声明在中间对象 editPathDialog 上，
                // 不在 footer 子树的非限定作用域内，裸调会 ReferenceError 点击无响应
                onClicked: editPathDialog.doConfirm()
            }
        }
    }

    // 目录选择器
    FolderDialog {
        id: fileDialog
        title: qsTr("Select Directory")
        onAccepted: {
            // 去掉尾部斜杠，保持与 PATH 条目风格一致
            var selected = fileDialog.selectedFolder.toString().replace("file://", "")
            if (selected.endsWith("/"))
                selected = selected.slice(0, -1)
            pathInput.text = selected
            // 选完目录后焦点回到输入框，便于继续回车确认
            pathInput.forceActiveFocus()
        }
    }
    }
}
