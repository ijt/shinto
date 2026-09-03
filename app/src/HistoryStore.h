// Typed-URL + visited-history store and omnibox suggestion ranking.
// Replaces chrome.storage.local's "typed" list, chrome.history.search, the
// Google-suggest fetch, and completeQuery/completeToUrl/toUrl from
// extension/background.js and extension/newtab.js.
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

class QNetworkAccessManager;

namespace shinto {

class HistoryStore : public QObject {
  Q_OBJECT

 public:
  struct Suggestion {
    QString url;
    QString label;
    QString kind;  // "typed" | "history" | "search"
  };

  explicit HistoryStore(QObject *parent = nullptr);

  // Opens (creating if needed) the SQLite store at shinto::historyDbPath().
  // Returns false (and logs) if the SQLite driver / file can't be opened.
  bool open();

  // Records an omnibox submission. `url` is what was navigated to; `q` is
  // the raw text the user typed (used as the suggestion label).
  void recordTyped(const QString &q, const QString &url);

  // Records a page visit, called from BrowserWindow on urlChanged/
  // titleChanged. Internal/gate-ish URLs are filtered out, same as the old
  // isInternalUrl().
  void recordVisit(const QString &url, const QString &title);

  // Async: merges local typed/visited matches (synchronous, SQLite) with a
  // Google-suggest network fetch (asynchronous, best-effort, ~700ms budget),
  // then emits completionsReady(requestId, ...) exactly once. Callers should
  // discard results for any requestId other than their latest.
  void requestCompletions(int requestId, const QString &query);

  // Heuristic URL-or-search resolution: has a scheme -> as-is; "//" -> https:;
  // "localhost[:port]/..." or an IPv4 host -> http://; a bare "word.word"
  // with no spaces -> https://; otherwise -> a DuckDuckGo search. Mirrors
  // toUrl()/completeToUrl() verbatim.
  static QString toUrl(const QString &raw);

 signals:
  void completionsReady(int requestId, QVector<Suggestion> results);

 private:
  static bool isInternalUrl(const QString &url);
  static QString prettyUrl(const QString &url);
  QVector<Suggestion> localMatches(const QString &query, int limit) const;
  void trimTyped();

  QSqlDatabase db_;
  QNetworkAccessManager *net_;
  static constexpr int kTypedMax = 300;
  static constexpr int kCompleteShow = 7;
};

}  // namespace shinto
