import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import org.deepin.dtk 1.0 as D

Rectangle {
    id: root
    radius: 8
    color: root.cardColor
    border.width: 1
    border.color: D.DTK.palette.midlight

    property var model: null
    property bool isSystem: false
    property int currentIndex: -1

    // 名称列宽随卡片宽度自适应，并限制在合理区间，避免值列被挤没
    readonly property int nameColumnWidth: Math.max(140, Math.min(240, width * 0.34))
    // 卡片内文字色（跟随 base 背景）
    readonly property color contentColor: D.DTK.palette.text

    // 调色板角色色可能带 alpha（DTK 的 window/base/highlight 为半透明），
    // 预合成到纯色上保证不透明，避免选中高亮叠加后发淡看不清
    function opaqueOver(color, over) {
        var a = over.a
        return Qt.rgba(over.r * a + color.r * (1 - a),
                       over.g * a + color.g * (1 - a),
                       over.b * a + color.b * (1 - a), 1.0)
    }

    // 卡片实际背景色：页面 window 与 base 两层半透叠加后的不透明效果色
    readonly property color cardColor: opaqueOver(D.DTK.palette.window, D.DTK.palette.base)

    signal addRequested()
    signal editRequested(string key, string value)
    signal deleteRequested(string key)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // 表头
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 16

            Text {
                Layout.preferredWidth: root.nameColumnWidth
                text: qsTr("Variable Name")
                font.bold: true
                font.pixelSize: D.DTK.fontManager.t7.pixelSize
                color: root.contentColor
                opacity: 0.7
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Variable Value")
                font.bold: true
                font.pixelSize: D.DTK.fontManager.t7.pixelSize
                color: root.contentColor
                elide: Text.ElideRight
                opacity: 0.7
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: root.border.color
        }

        // 变量列表
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            clip: true
            focus: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            currentIndex: root.currentIndex
            spacing: 2

            onCurrentIndexChanged: root.currentIndex = currentIndex

            // 键盘快捷键：Delete 删除、Enter/双击 编辑
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Delete && currentIndex >= 0) {
                    var k = model.keyAt(currentIndex)
                    root.deleteRequested(k)
                    root.currentIndex = -1
                    event.accepted = true
                } else if (event.key === Qt.Key_Return && currentIndex >= 0) {
                    var k = model.keyAt(currentIndex)
                    var v = model.valueAt(currentIndex)
                    root.editRequested(k, v)
                    event.accepted = true
                }
            }

            delegate: ItemDelegate {
                id: delegateRoot
                width: ListView.view.width
                height: 40
                highlighted: ListView.isCurrentItem
                onClicked: listView.currentIndex = index
                onDoubleClicked: root.editRequested(model.key, model.value)

                // 选中行用不透明高亮底，配合 highlightedText 保证任意主题下都清晰可辨
                background: Rectangle {
                    radius: 4
                    color: delegateRoot.highlighted
                        ? root.opaqueOver(root.cardColor, D.DTK.palette.highlight)
                        : "transparent"
                }

                // 悬浮查看完整内容：ToolTip 挂在 ItemDelegate(Control) 上，
                // 裸 Text 没有 hoverEnabled/hovered；仅在文本真被省略时弹出
                ToolTip.delay: 600
                ToolTip.timeout: 8000
                ToolTip.visible: delegateRoot.hovered && (keyText.truncated || valueText.truncated)
                ToolTip.text: (keyText.truncated && !valueText.truncated) ? model.key : model.value

                contentItem: RowLayout {
                    spacing: 16

                    Text {
                        id: keyText
                        Layout.preferredWidth: root.nameColumnWidth - 16
                        text: model.key
                        font.pixelSize: D.DTK.fontManager.t6.pixelSize
                        color: delegateRoot.highlighted
                            ? D.DTK.palette.highlightedText
                            : root.contentColor
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        id: valueText
                        Layout.fillWidth: true
                        text: model.value
                        font.pixelSize: D.DTK.fontManager.t6.pixelSize
                        color: delegateRoot.highlighted
                            ? D.DTK.palette.highlightedText
                            : root.contentColor
                        elide: Text.ElideMiddle
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            ScrollIndicator.vertical: ScrollIndicator { }

            // 空列表提示
            Text {
                anchors.centerIn: parent
                visible: listView.count === 0
                text: qsTr("No environment variables")
                font.pixelSize: D.DTK.fontManager.t7.pixelSize
                color: root.contentColor
                opacity: 0.45
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: root.border.color
        }

        // 操作按钮栏
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 8

            // "新建"为主操作，视觉突出
            D.RecommandButton {
                text: qsTr("New")
                onClicked: root.addRequested()
            }

            D.Button {
                text: qsTr("Edit")
                flat: true
                enabled: listView.currentIndex >= 0
                onClicked: {
                    if (listView.currentIndex >= 0) {
                        var k = model.keyAt(listView.currentIndex)
                        var v = model.valueAt(listView.currentIndex)
                        root.editRequested(k, v)
                    }
                }
            }

            D.Button {
                text: qsTr("Delete")
                flat: true
                enabled: listView.currentIndex >= 0
                onClicked: {
                    if (listView.currentIndex >= 0) {
                        var k = model.keyAt(listView.currentIndex)
                        root.deleteRequested(k)
                        root.currentIndex = -1
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }
    }
}
