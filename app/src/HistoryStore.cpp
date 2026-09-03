#include "HistoryStore.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include "Shinto.h"

namespace shinto {

namespace {

bool looksLikeMatch(const QString &haystackLower, const QString &needleLower) {
  return needleLower.isEmpty() || haystackLower.contains(needleLower);
}

}  // namespace

HistoryStore::HistoryStore(QObject *parent)
    : QObject(parent), net_(new QNetworkAccessManager(this)) {}

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

QString HistoryStore::prettyUrl(const QString &urlStr) {
  QUrl u(urlStr);
  if (!u.isValid() || u.host().isEmpty()) return urlStr;
  QString host = u.host();
  if (host.startsWith(QStringLiteral("www."))) host = host.mid(4);
  QString path = u.path();
  if (u.hasQuery()) path += "?" + u.query();
  if (u.hasFragment()) path += "#" + u.fragment();
  if (path.isEmpty() || path == QStringLiteral("/")) return host;
  return host + path;
}

QVector<HistoryStore::Suggestion> HistoryStore::localMatches(const QString &query,
                                                              int limit) const {
  QVector<Suggestion> out;
  QSet<QString> seen;
  const QString qn = query.trimmed().toLower();

  auto add = [&](const Suggestion &s) {
    if (s.url.isEmpty() || isInternalUrl(s.url) || seen.contains(s.url)) return;
    seen.insert(s.url);
    out.push_back(s);
  };

  // Typed entries, most recent first, matched against the typed label, the
  // raw URL, or the "pretty" URL -- mirrors typedMatches() in background.js.
  {
    QSqlQuery sql(db_);
    sql.exec(QStringLiteral("SELECT url, q FROM typed ORDER BY t DESC"));
    while (out.size() < limit && sql.next()) {
      const QString url = sql.value(0).toString();
      const QString label = sql.value(1).toString();
      if (!looksLikeMatch(label.toLower(), qn) && !looksLikeMatch(url.toLower(), qn) &&
          !looksLikeMatch(prettyUrl(url).toLower(), qn)) {
        continue;
      }
      add({url, label.isEmpty() ? prettyUrl(url) : label, QStringLiteral("typed")});
    }
  }

  // Visited history, ranked by visit count, within the last 180 days --
  // mirrors chrome.history.search()'s options in completeQuery().
  {
    const qint64 cutoff =
        QDateTime::currentMSecsSinceEpoch() - qint64(180) * 24 * 60 * 60 * 1000;
    QSqlQuery sql(db_);
    sql.prepare(QStringLiteral(
        "SELECT url, title FROM visited"
        " WHERE last_visit >= :cutoff"
        "  AND (title LIKE :like COLLATE NOCASE OR url LIKE :like COLLATE NOCASE)"
        " ORDER BY visit_count DESC LIMIT 24"));
    sql.bindValue(":cutoff", cutoff);
    sql.bindValue(":like", "%" + query.trimmed() + "%");
    sql.exec();
    while (out.size() < limit && sql.next()) {
      const QString url = sql.value(0).toString();
      add({url, prettyUrl(url), QStringLiteral("history")});
    }
  }

  return out;
}

void HistoryStore::requestCompletions(int requestId, const QString &query) {
  const QString trimmed = query.trimmed();
  QVector<Suggestion> local = localMatches(trimmed, kCompleteShow);

  if (trimmed.size() < 2 || local.size() >= kCompleteShow) {
    emit completionsReady(requestId, local);
    return;
  }

  QUrl endpoint(QStringLiteral("https://suggestqueries.google.com/complete/search"));
  QUrlQuery params;
  params.addQueryItem("client", "firefox");
  params.addQueryItem("q", trimmed);
  endpoint.setQuery(params);

  QNetworkReply *reply = net_->get(QNetworkRequest(endpoint));
  QTimer::singleShot(700, reply, &QNetworkReply::abort);

  connect(reply, &QNetworkReply::finished, this, [this, reply, requestId, local]() mutable {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
      if (doc.isArray() && doc.array().size() > 1 && doc.array()[1].isArray()) {
        QSet<QString> seen;
        for (const auto &s : local) seen.insert(s.url);
        for (const QJsonValue &v : doc.array()[1].toArray()) {
          if (local.size() >= kCompleteShow) break;
          const QString phrase = v.toString();
          const QString url = toUrl(phrase);
          if (url.isEmpty() || seen.contains(url)) continue;
          seen.insert(url);
          local.push_back({url, phrase, QStringLiteral("search")});
        }
      }
    }
    emit completionsReady(requestId, local);
  });
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
