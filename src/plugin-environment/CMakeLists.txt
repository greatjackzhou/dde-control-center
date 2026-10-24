cmake_minimum_required(VERSION 3.23)
set(pluginName environment)

# 依赖（Qt / Dtk / DCC 框架）由 dde-control-center 根 CMakeLists 统一 find_package，
# 并通过全局变量 ${QT_NS}、${DTK_NS}、${DCC_FRAME_Library} 暴露给各插件，
# 插件自身不再重复 find_package。
file(GLOB_RECURSE ${pluginName}_SRCS
    "operation/*.cpp"
    "operation/*.h"
)

add_library(${pluginName} MODULE ${${pluginName}_SRCS})

target_link_libraries(${pluginName} PRIVATE
    ${DCC_FRAME_Library}
    ${QT_NS}::Core
    ${QT_NS}::Gui
    ${QT_NS}::Quick
    ${DTK_NS}::Core
    ${DTK_NS}::Gui
)

# ---- polkit 提权：写入 /etc/environment ----
# helper 的绝对安装路径单点定义：按 DDE 惯例提权 helper 放 libexec/（对齐
# /usr/libexec/<component>/ 既有 polkit helper），policy 与 C++ 调用点均在此变量
# 下派生，多架构布局下三处永远一致。
include(GNUInstallDirs)
set(DDE_ENV_HELPER_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/dde-control-center")
set(DDE_ENV_HELPER_PATH "${DDE_ENV_HELPER_INSTALL_DIR}/dde-env-write")

# HelperPath 供 C++ 侧（envvarstore.cpp）定位 pkexec 目标
target_compile_definitions(${pluginName} PRIVATE DDE_ENV_HELPER_PATH="${DDE_ENV_HELPER_PATH}")

dcc_install_plugin(NAME ${pluginName} TARGET ${pluginName})
dcc_handle_plugin_translation(NAME ${pluginName})

# helper 与 policy 均安装到上方单点定义的绝对路径下；
# policy 里的 exec.path 注解使用 @HELPER_PATH@ 占位符，安装时替换为真实路径。
install(PROGRAMS
    "${CMAKE_CURRENT_SOURCE_DIR}/misc/dde-env-write"
    DESTINATION "${DDE_ENV_HELPER_INSTALL_DIR}"
)

# policy 安装到 /usr/share/polkit-1/actions（前缀为 /usr 时），
# 内容中的 @HELPER_PATH@ 替换为 helper 的真实绝对路径。
install(CODE "
    set(HELPER_PATH \"${DDE_ENV_HELPER_PATH}\")
    configure_file(\"${CMAKE_CURRENT_SOURCE_DIR}/misc/com.deepin.controlcenter.environment.policy\"
                   \"${CMAKE_CURRENT_BINARY_DIR}/com.deepin.controlcenter.environment.policy\"
                   @ONLY)
    file(INSTALL
         \"${CMAKE_CURRENT_BINARY_DIR}/com.deepin.controlcenter.environment.policy\"
         DESTINATION \"${CMAKE_INSTALL_FULL_DATADIR}/polkit-1/actions\")
")

# ---- 模块图标 ----
# DccObject.icon 使用 freedesktop 标准主题图标名（preferences-system），由
# QIcon::fromTheme 在运行时经 deepin-icon-theme 解析，与其它控制中心模块一致；
# 本插件不携带自带 svg。如需专属图标，应在 deepin-icon-theme 另行收录，
# 届时只需把 QML 中的图标名改为对应主题名即可。
