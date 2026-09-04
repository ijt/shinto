#include "Notify.h"

#include <QProcess>

namespace shinto {

void notify(const QString &title, const QString &body, bool critical) {
  QProcess::startDetached(
      QStringLiteral("notify-send"),
      {QStringLiteral("-u"), critical ? QStringLiteral("critical") : QStringLiteral("normal"),
       QStringLiteral("-a"), QStringLiteral("Shinto"), title, body});
}

}  // namespace shinto
