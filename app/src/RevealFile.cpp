#include "RevealFile.h"

#include <QFileInfo>
#include <QProcess>
#include <QUrl>

namespace shinto {

void revealInFileManager(const QString &path) {
  const QString uri = QUrl::fromLocalFile(path).toString();
  // A tracked (not startDetached) QProcess, since the fallback below needs
  // to see whether ShowItems actually succeeded -- dbus-send's own exit
  // code is meaningful with --print-reply (confirmed: 0 only once the
  // destination actually replies to the method call, not just on
  // successful dispatch).
  auto *proc = new QProcess();
  QObject::connect(proc, &QProcess::finished, proc,
                    [proc, path](int exitCode, QProcess::ExitStatus status) {
                      if (exitCode != 0 || status != QProcess::NormalExit) {
                        QProcess::startDetached(QStringLiteral("xdg-open"),
                                                 {QFileInfo(path).absolutePath()});
                      }
                      proc->deleteLater();
                    });
  proc->start(QStringLiteral("dbus-send"),
              {QStringLiteral("--session"), QStringLiteral("--print-reply"),
               QStringLiteral("--dest=org.freedesktop.FileManager1"),
               QStringLiteral("/org/freedesktop/FileManager1"),
               QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
               QStringLiteral("array:string:%1").arg(uri), QStringLiteral("string:")});
}

}  // namespace shinto
