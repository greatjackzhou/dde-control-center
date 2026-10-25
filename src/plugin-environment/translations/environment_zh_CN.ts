<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>EnvironmentModule</name>
    <message>
        <source>Added successfully</source>
        <translation>添加成功</translation>
    </message>
    <message>
        <source>Failed to add: variable already exists</source>
        <translation>添加失败：变量已存在</translation>
    </message>
    <message>
        <source>Modified successfully</source>
        <translation>修改成功</translation>
    </message>
    <message>
        <source>Failed to modify</source>
        <translation>修改失败</translation>
    </message>
    <message>
        <source>Deleted successfully</source>
        <translation>删除成功</translation>
    </message>
    <message>
        <source>Failed to delete</source>
        <translation>删除失败</translation>
    </message>
    <message>
        <source>Failed to add: variable already exists or permission denied</source>
        <translation>添加失败：变量已存在或权限不足</translation>
    </message>
    <message>
        <source>Failed to modify or permission denied</source>
        <translation>修改失败或权限不足</translation>
    </message>
    <message>
        <source>Failed to delete or permission denied</source>
        <translation>删除失败或权限不足</translation>
    </message>
    <message>
        <source>PATH updated</source>
        <translation>PATH 已更新</translation>
    </message>
    <message>
        <source>Failed to update PATH</source>
        <translation>PATH 更新失败</translation>
    </message>
</context>
<context>
    <name>Environment</name>
    <message>
        <source>Environment Variables</source>
        <translation>环境变量</translation>
    </message>
    <message>
        <source>Manage user and system environment variables</source>
        <translation>管理用户与系统的环境变量</translation>
    </message>
</context>
<context>
    <name>EnvironmentPage</name>
    <message>
        <source>Environment Variables</source>
        <translation>环境变量</translation>
    </message>
    <message>
        <source>User Variables for %1</source>
        <translation>%1 的用户环境变量</translation>
    </message>
    <message>
        <source>These variables apply only to your user account (~/.config/environment.d/dde-control-center.conf)</source>
        <translation type="unfinished">这些变量仅适用于当前用户账户 (~/.config/environment.d/dde-control-center.conf)</translation>
    </message>
    <message>
        <source>System Variables</source>
        <translation>系统环境变量</translation>
    </message>
    <message>
        <source>These variables apply to all users (/etc/environment)</source>
        <translation>这些变量适用于所有用户 (/etc/environment)</translation>
    </message>
    <message>
        <source>Changes are synced without logging out. For PATH, newly opened terminals take effect right away, but programs that are already running and the graphical session need to be restarted, or you need to log out and back in.</source>
        <translation>修改会自动同步，无需注销。对于 PATH，新打开的终端会立即生效；但已在运行的程序与图形会话需要重启，或注销并重新登录后才生效。</translation>
    </message>
    <message>
        <source>Delete Variable?</source>
        <translation>确认删除环境变量？</translation>
    </message>
    <message>
        <source>Are you sure you want to delete environment variable &quot;%1&quot;? This action cannot be undone.</source>
        <translation>确定要删除环境变量“%1”吗？此操作不可撤销。</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
</context>
<context>
    <name>VariableListView</name>
    <message>
        <source>Variable Name</source>
        <translation>变量名</translation>
    </message>
    <message>
        <source>Variable Value</source>
        <translation>变量值</translation>
    </message>
    <message>
        <source>New</source>
        <translation>新建</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>No environment variables</source>
        <translation>暂无环境变量</translation>
    </message>
</context>
<context>
    <name>AddEditDialog</name>
    <message>
        <source>New Variable</source>
        <translation>新建变量</translation>
    </message>
    <message>
        <source>Edit Variable</source>
        <translation>编辑变量</translation>
    </message>
    <message>
        <source>Variable name:</source>
        <translation>变量名：</translation>
    </message>
    <message>
        <source>Variable value:</source>
        <translation>变量值：</translation>
    </message>
    <message>
        <source>e.g. JAVA_HOME</source>
        <translation>例如 JAVA_HOME</translation>
    </message>
    <message>
        <source>e.g. /usr/lib/jvm/java-17</source>
        <translation>例如 /usr/lib/jvm/java-17</translation>
    </message>
    <message>
        <source>Variable name cannot be empty</source>
        <translation>变量名不能为空</translation>
    </message>
    <message>
        <source>Invalid name: must start with letter or underscore, contain only letters, digits, and underscores</source>
        <translation>名称无效：必须以字母或下划线开头，只能包含字母、数字和下划线</translation>
    </message>
    <message>
        <source>Operation failed, variable may already exist</source>
        <translation>操作失败，变量可能已存在</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <source>OK</source>
        <translation>确定</translation>
    </message>
</context>
<context>
    <name>PathEditDialog</name>
    <message>
        <source>Edit PATH</source>
        <translation>编辑 PATH</translation>
    </message>
    <message>
        <source>New</source>
        <translation>新建</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Move Up</source>
        <translation>上移</translation>
    </message>
    <message>
        <source>Move Down</source>
        <translation>下移</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <source>Add Path</source>
        <translation>添加路径</translation>
    </message>
    <message>
        <source>Edit Path</source>
        <translation>编辑路径</translation>
    </message>
    <message>
        <source>Path:</source>
        <translation>路径：</translation>
    </message>
    <message>
        <source>/usr/local/bin</source>
        <translation>/usr/local/bin</translation>
    </message>
    <message>
        <source>Browse...</source>
        <translation>浏览...</translation>
    </message>
    <message>
        <source>Select Directory</source>
        <translation>选择目录</translation>
    </message>
    <message>
        <source>Path cannot be empty</source>
        <translation>路径不能为空</translation>
    </message>
    <message>
        <source>Path already exists</source>
        <translation>该路径已存在</translation>
    </message>
    <message>
        <source>No path entries</source>
        <translation>暂无路径条目</translation>
    </message>
    <message>
        <source>Click New to add path entries</source>
        <translation>点击“新建”按钮添加路径条目</translation>
    </message>
    <message>
        <source>%1 items</source>
        <translation>%1 个条目</translation>
    </message>
    <message>
        <source>Save failed, please retry or check permissions</source>
        <translation>保存失败，请重试或检查权限</translation>
    </message>
    <message>
        <source>Discard changes?</source>
        <translation>是否放弃更改？</translation>
    </message>
    <message>
        <source>You have unsaved changes to PATH. Discard them?</source>
        <translation>对 PATH 的修改尚未保存，确定要放弃这些更改吗？</translation>
    </message>
    <message>
        <source>Keep Editing</source>
        <translation>继续编辑</translation>
    </message>
    <message>
        <source>Discard</source>
        <translation>放弃更改</translation>
    </message>
    <message>
        <source>This is not an absolute path and may not work as expected</source>
        <translation>这不是绝对路径，可能不会按预期生效</translation>
    </message>
</context>
</TS>
