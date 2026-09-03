// Typed-URL + visited-history log, and the omnibox's URL-or-search
// heuristic. There's no suggestion UI reading this back right now (the
// omnibox is a plain input, no dropdown) -- recording still happens so the
// data is there if a completion UI comes back later, but nothing queries
// it yet.
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

namespace shinto {

class HistoryStore : public QObject {
  Q_OBJECT

 public:
  explicit HistoryStore(QObject *parent = nullptr);

  // Opens (creating if needed) the SQLite store at shinto::historyDbPath().
  // Returns false (and logs) if the SQLite driver / file can't be opened.
  bool open();

  // Records an omnibox submission. `url` is what was navigated to; `q` is
  // the raw text the user typed.
  void recordTyped(const QString &q, const QString &url);

  // Records a page visit, called from BrowserWindow on urlChanged/
  // titleChanged. Internal/gate-ish URLs are filtered out.
  void recordVisit(const QString &url, const QString &title);

  // Heuristic URL-or-search resolution: has a scheme -> as-is; "//" -> https:;
  // "localhost[:port]/..." or an IPv4 host -> http://; a bare "word.word"
  // with no spaces -> https://; otherwise -> a DuckDuckGo search. Mirrors
  // toUrl()/completeToUrl() from the old extension/background.js verbatim.
  static QString toUrl(const QString &raw);

 private:
  static bool isInternalUrl(const QString &url);
  void trimTyped();

  QSqlDatabase db_;
  static constexpr int kTypedMax = 300;
};

}  // namespace shinto
