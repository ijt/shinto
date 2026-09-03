#include "PopularDomains.h"

#include <algorithm>

#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace shinto {

PopularDomains::PopularDomains() {
  QFile file(QStringLiteral(":/shinto/top100k-domains.txt"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "shinto: could not open bundled domain list";
    return;
  }
  QTextStream stream(&file);
  int rank = 0;
  while (!stream.atEnd()) {
    const QString line = stream.readLine().trimmed().toLower();
    if (!line.isEmpty()) {
      domains_.push_back({line, rank});
    }
    ++rank;
  }
  std::sort(domains_.begin(), domains_.end(),
            [](const Entry &a, const Entry &b) { return a.domain < b.domain; });
}

QVector<PopularDomains::Suggestion> PopularDomains::complete(const QString &prefix,
                                                              int limit) const {
  QVector<Suggestion> out;
  const QString p = prefix.trimmed().toLower();
  if (p.size() < 2 || domains_.isEmpty()) return out;

  // [lo, hi) is the contiguous range of domains starting with `p` --
  // `upper` is `p` with its last character bumped by one code point, the
  // standard trick for a prefix range query over a sorted sequence.
  QString upper = p;
  upper[upper.size() - 1] = QChar(upper.at(upper.size() - 1).unicode() + 1);

  const auto lo = std::lower_bound(
      domains_.begin(), domains_.end(), p,
      [](const Entry &e, const QString &v) { return e.domain < v; });
  const auto hi = std::lower_bound(
      domains_.begin(), domains_.end(), upper,
      [](const Entry &e, const QString &v) { return e.domain < v; });

  QVector<Entry> matches(lo, hi);
  std::sort(matches.begin(), matches.end(),
            [](const Entry &a, const Entry &b) { return a.rank < b.rank; });

  const int count = qMin(limit, int(matches.size()));
  for (int i = 0; i < count; ++i) {
    out.push_back({matches[i].domain, QStringLiteral("https://") + matches[i].domain});
  }
  return out;
}

}  // namespace shinto
