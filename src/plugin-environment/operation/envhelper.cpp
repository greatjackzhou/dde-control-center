#include "envhelper.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <memory>

// helper 绝对路径由 CMake 以 DDE_ENV_HELPER_PATH 宏单点传入；未定义时退回
// DDE libexec 安装位（与插件 CMakeLists 的 install 目标一致）。
#ifndef DDE_ENV_HELPER_PATH
#define DDE_ENV_HELPER_PATH "/usr/libexec/dde-control-center/dde-env-write"
#endif

namespace DdeEnvHelper {

const QString &path()
{
    static const QString kPath = QStringLiteral(DDE_ENV_HELPER_PATH);
    return kPath;
}

void runAsync(const QStringList &argsAfterHelper, std::function<void(bool)> onDone)
{
    // 堆上 QProcess，完成后 deleteLater；不阻塞调用线程（GUI）。
    auto *process = new QProcess;
    auto called = std::make_shared<bool>(false);

    // 保证 onDone 恰调用一次（errorOccurred 与 finished 可能竞争）。
    auto finishOnce = [process, called, onDone](bool ok) {
        if (*called) {
            return;
        }
        *called = true;
        process->deleteLater();
        if (onDone) {
            onDone(ok);
        }
    };

    QStringList args;
    args << path() << argsAfterHelper;

    QObject::connect(process,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     process,
                     [finishOnce, process](int code, QProcess::ExitStatus) {
                         if (code == 0) {
                             finishOnce(true);
                             return;
                         }
                         const QString err =
                             QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                         if (code == 126) {
                             qWarning() << "polkit authorization cancelled or denied (code 126)";
                         } else if (code == 127) {
                             qWarning() << "env helper not executable (code 127):" << path();
                         } else {
                             qWarning() << "env helper failed (code" << code << "):" << err;
                         }
                         finishOnce(false);
                     });

    QObject::connect(process, &QProcess::errorOccurred, process,
                     [finishOnce](QProcess::ProcessError error) {
                         if (error == QProcess::FailedToStart) {
                             qWarning() << "pkexec failed to start env helper";
                             finishOnce(false);
                         }
                     });

    process->start(QStringLiteral("pkexec"), args);
}

void runWithContentAsync(const QByteArray &content,
                         const QStringList &modeArgs,
                         std::function<void(bool)> onDone)
{
    // 内容经临时文件传递给 root 侧 helper；helper 结束后由回调方删除临时文件。
    auto *tmpFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/dde-env-XXXXXX"));
    tmpFile->setAutoRemove(false);
    if (!tmpFile->open()) {
        qWarning() << "Failed to create temp file:" << tmpFile->errorString();
        delete tmpFile;
        if (onDone) {
            onDone(false);
        }
        return;
    }
    tmpFile->write(content);
    // 必须 flush 并关闭，root 侧 helper 进程才能读到完整内容
    tmpFile->flush();
    tmpFile->close();
    const QString tmpPath = tmpFile->fileName();
    delete tmpFile;

    QStringList args = modeArgs;
    args << tmpPath;
    runAsync(args, [tmpPath, onDone](bool ok) {
        QFile::remove(tmpPath);
        if (onDone) {
            onDone(ok);
        }
    });
}

} // namespace DdeEnvHelper
