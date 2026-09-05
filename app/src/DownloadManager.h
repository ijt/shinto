// Tracks every download for the daemon's lifetime: accepts each one
// (dedupes the filename, retries transient network interruptions -- logic
// moved here verbatim from WebProfile.cpp's old installDownloadHandler),
// persists id/filename/path/url/size/state to SQLite so the list survives
// a daemon restart, and emits signals so any number of BrowserWindows (a
// bottom-of-window progress bar) and the standalone DownloadsWindow can
// react live without polling.
#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

class QWebEngineDownloadRequest;

namespace shinto {

class DownloadManager : public QObject {
  Q_OBJECT

 public:
  enum class State { InProgress, Completed, Interrupted, Cancelled };

  struct DownloadRecord {
    int id = 0;               // QWebEngineDownloadRequest::id()
    QString filename;         // deduped save name (display name)
    QString path;             // full local path (dir + filename)
    QString url;               // source URL
    qint64 totalBytes = 0;
    qint64 receivedBytes = 0;
    State state = State::InProgress;
    QString interruptReason;   // empty unless state == Interrupted
    qint64 startedAt = 0;       // epoch seconds
    qint64 finishedAt = 0;       // 0 while not finished
  };

  explicit DownloadManager(QObject *parent = nullptr);

  // Opens (creating if needed) the SQLite store at shinto::downloadsDbPath().
  // Returns false (and logs) if the SQLite driver/file can't be opened. Any
  // row still InProgress from a previous daemon run is flipped to
  // Interrupted here -- its real QWebEngineDownloadRequest died with that
  // process, so it can never actually resume.
  bool open();

  // Accepts `download` (dedupes its filename into the platform Downloads
  // folder, calls accept()), tracks it in-memory and in SQLite, and drives
  // it through transient-interruption retries until it finishes for good.
  // Call once per QWebEngineProfile::downloadRequested signal.
  void track(QWebEngineDownloadRequest *download);

  // Newest-started first -- for the downloads list view.
  QVector<DownloadRecord> all() const;

  bool hasActive() const;

  // The most recently *started* still-InProgress download -- for the
  // bottom-bar overlay, which only ever shows one at a time. Returns a
  // default-constructed record (id == 0) if hasActive() is false.
  DownloadRecord latestActive() const;

 signals:
  void downloadAdded(int id);
  // Throttled (~4/sec) -- QWebEngineDownloadRequest::receivedBytesChanged
  // fires far more often than any UI needs to redraw.
  void downloadProgress(int id, qint64 received, qint64 total);
  void downloadStateChanged(int id, State state);

 private:
  // Upserts `record` into both the in-memory cache (records_) and SQLite
  // (best-effort -- a failed write is logged and otherwise ignored, same
  // as HistoryStore: the in-memory state driving live UI matters more
  // than every write reaching disk).
  void persist(const DownloadRecord &record);

  // A default-constructed (id == 0) record if `id` isn't tracked.
  DownloadRecord recordFor(int id) const;

  QSqlDatabase db_;
  QVector<DownloadRecord> records_;
};

}  // namespace shinto
