import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

DccObject {
    EnvironmentPage {
        name: "variables"
        parentName: "environment"
        displayName: qsTr("Environment Variables")
        // 同 Environment.qml：DccObject 阶段无 palette 上下文，使用静态主题图标名。
        icon: "environment_icon"
        weight: 10
    }
}
