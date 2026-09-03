// Shared constants and small path helpers used across the Shinto app.
// Mirrors the paths the old bash `shinto` script/control-server.py used, so a
// migrating install lands in familiar places.
#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

namespace shinto {

// Fixed Wayland app_id / QGuiApplication name. Every Shinto window uses this
// same id (see hypr.lua), unlike the old Chromium --app windows whose app_id
// was derived from the loaded URL.
inline const char *kAppId = "shinto";

// ~/.local/share/shinto (respects XDG_DATA_HOME).
inline QString dataHome() {
  QString base = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
  if (base.isEmpty()) {
    base = QDir::homePath() + "/.local/share";
  }
  QDir dir(base + "/shinto");
  dir.mkpath(".");
  return dir.absolutePath();
}

// Where QWebEngineProfile keeps cookies/localStorage/cache. Deliberately a
// new subpath, not the old Chromium --user-data-dir, since the on-disk
// formats aren't compatible.
inline QString webEngineStoragePath() { return dataHome() + "/profile/webengine"; }

// SQLite typed/visited history store, independent of the WebEngine profile.
inline QString historyDbPath() { return dataHome() + "/history.sqlite"; }

// Omarchy's per-theme color file.
inline QString colorsTomlPath() {
  return QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml";
}

// $XDG_CONFIG_HOME/shinto/config.lua (~/.config/shinto/config.lua by
// default) -- the user-editable Lua config file (search engine, etc; see
// Config.h). Unlike colorsTomlPath() this one is Shinto's own, not
// Omarchy-managed, and it's fine for it not to exist yet.
inline QString configLuaPath() {
  QString base = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
  if (base.isEmpty()) {
    base = QDir::homePath() + "/.config";
  }
  return base + "/shinto/config.lua";
}

// $XDG_RUNTIME_DIR/shinto.sock — the singleton handoff socket.
inline QString singletonSocketPath() {
  QString runtime = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
  if (runtime.isEmpty()) {
    runtime = QDir::tempPath();
  }
  return runtime + "/shinto.sock";
}

}  // namespace shinto
