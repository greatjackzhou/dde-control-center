import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

DccObject {
    name: "environment"
    parentName: "system"
    displayName: qsTr("Environment Variables")
    description: qsTr("Manage user and system environment variables")
    // DccObject 元数据在模块创建阶段求值，此处无窗口/调色板上下文（D.DTK.palette 尚不可用），
    // 不能做依赖调色板的动态计算；icon 交由框架经 QIcon::fromTheme 解析静态主题图标名。
    icon: "environment_icon"
    weight: 200
}
