#ifndef DDE_ENV_HELPER_H
#define DDE_ENV_HELPER_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <functional>

// 提权 helper 调用统一入口：集中 helper 绝对路径常量与 pkexec 异步调用范式，
// 供 EnvVarStore（写 /etc/environment）与 EnvironmentModule（装 /etc/profile.d）
// 复用，避免两处各自定义 DDE_ENV_HELPER_PATH 回退与退出码判定而失配。
namespace DdeEnvHelper {

// helper 绝对安装路径：由 CMake 以 DDE_ENV_HELPER_PATH 宏单点注入，与 policy 的
// exec.path 注解、install 目标同源；未定义时退回 DDE libexec 安装位。
const QString &path();

// 异步 pkexec 调用：pkexec <path> <argsAfterHelper...>，按退出码判定
// （0 成功；126 用户取消/未授权；127 helper 不可执行；其余为 helper 自身错误），
// 进程结束后恰好回调一次 onDone(ok)。全程非阻塞，不占用 GUI 线程。
void runAsync(const QStringList &argsAfterHelper, std::function<void(bool)> onDone);

// 先把 content 落到临时文件，再异步调用 helper（modeArgs + 临时文件路径），
// helper 结束后删除临时文件并回调。用于需要以文件为源的 environment / profile-d 写入。
void runWithContentAsync(const QByteArray &content,
                         const QStringList &modeArgs,
                         std::function<void(bool)> onDone);

} // namespace DdeEnvHelper

#endif // DDE_ENV_HELPER_H
