#include "HistoryStore.h"

#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QUrlQuery>

#include "Shinto.h"

namespace shinto {

HistoryStore::HistoryStore(QObject *parent) : QObject(parent) {}

bool HistoryStore::open() {
  db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("shinto_history"));
  db_.setDatabaseName(historyDbPath());
  if (!db_.open()) {
    qWarning() << "shinto: could not open history db:" << db_.lastError().text();
    return false;
  }
  QSqlQuery q(db_);
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS typed ("
      " url TEXT PRIMARY KEY, q TEXT, t INTEGER, n INTEGER)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS visited ("
      " url TEXT PRIMARY KEY, title TEXT, visit_count INTEGER, last_visit INTEGER)"));
  return true;
}

void HistoryStore::recordTyped(const QString &q, const QString &url) {
  if (url.isEmpty()) return;
  QSqlQuery query(db_);
  query.prepare(QStringLiteral(
      "INSERT INTO typed(url, q, t, n) VALUES(:url, :q, :t, 1)"
      " ON CONFLICT(url) DO UPDATE SET"
      "  q = excluded.q, t = excluded.t, n = typed.n + 1"));
  query.bindValue(":url", url);
  query.bindValue(":q", q.trimmed());
  query.bindValue(":t", QDateTime::currentMSecsSinceEpoch());
  if (!query.exec()) {
    qWarning() << "shinto: recordTyped failed:" << query.lastError().text();
  }
  trimTyped();
}

void HistoryStore::trimTyped() {
  QSqlQuery query(db_);
  query.prepare(QStringLiteral(
      "DELETE FROM typed WHERE url NOT IN"
      " (SELECT url FROM typed ORDER BY t DESC LIMIT :max)"));
  query.bindValue(":max", kTypedMax);
  query.exec();
}

void HistoryStore::recordVisit(const QString &url, const QString &title) {
  if (isInternalUrl(url)) return;
  QSqlQuery query(db_);
  query.prepare(QStringLiteral(
      "INSERT INTO visited(url, title, visit_count, last_visit)"
      " VALUES(:url, :title, 1, :t)"
      " ON CONFLICT(url) DO UPDATE SET"
      "  title = excluded.title, visit_count = visited.visit_count + 1,"
      "  last_visit = excluded.last_visit"));
  query.bindValue(":url", url);
  query.bindValue(":title", title);
  query.bindValue(":t", QDateTime::currentMSecsSinceEpoch());
  if (!query.exec()) {
    qWarning() << "shinto: recordVisit failed:" << query.lastError().text();
  }
}

bool HistoryStore::isInternalUrl(const QString &url) {
  if (url.isEmpty()) return true;
  return url.startsWith(QStringLiteral("chrome://")) ||
         url.startsWith(QStringLiteral("chrome-extension://")) ||
         url.startsWith(QStringLiteral("qrc:")) || url.startsWith(QStringLiteral("about:"));
}

QString HistoryStore::toUrl(const QString &raw) {
  const QString q = raw.trimmed();
  if (q.isEmpty()) return {};

  static const QRegularExpression kScheme(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
  static const QRegularExpression kLocalhost(
      QStringLiteral("^localhost(:\\d+)?(/|$)"), QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression kIpv4(QStringLiteral("^(\\d{1,3}\\.){3}\\d{1,3}(:\\d+)?(/|$)"));
  static const QRegularExpression kBareDomain(QStringLiteral("^\\S+\\.\\S+$"));

  if (kScheme.match(q).hasMatch()) return q;
  if (q.startsWith(QStringLiteral("//"))) return QStringLiteral("https:") + q;
  if (kLocalhost.match(q).hasMatch()) return QStringLiteral("http://") + q;
  if (kIpv4.match(q).hasMatch()) return QStringLiteral("http://") + q;
  if (kBareDomain.match(q).hasMatch() && !q.contains(' ')) return QStringLiteral("https://") + q;

  QUrl search(QStringLiteral("https://duckduckgo.com/"));
  QUrlQuery params;
  params.addQueryItem("q", q);
  search.setQuery(params);
  return search.toString();
}

}  // namespace shinto
