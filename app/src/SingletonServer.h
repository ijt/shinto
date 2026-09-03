// Daemon-side half of the singleton handoff. Listens on
// shinto::singletonSocketPath() for line commands from SingletonClient.
// Replaces control-server.py's /open endpoint and Chromium's own
// SingletonSocket/SingletonLock.
#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace shinto {

class SingletonServer : public QObject {
  Q_OBJECT

 public:
  explicit SingletonServer(QObject *parent = nullptr);

  // Starts listening at shinto::singletonSocketPath(), clearing any stale
  // socket file left by an unclean shutdown first. Returns false on failure
  // (e.g. another daemon is genuinely already running).
  bool listen();

 signals:
  // "OPEN <url>" (url may be empty -> open the empty gate).
  void openRequested(const QString &url);
  // "THEME" -> re-read colors.toml and re-apply to every open window.
  void themeReloadRequested();

 private:
  void handleNewConnection();

  QLocalServer *server_;
};

}  // namespace shinto
