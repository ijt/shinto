#include "Config.h"

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QUrl>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "Shinto.h"

namespace shinto {

namespace {

// Shinto is normally launched from a Hyprland keybinding or systemd, with
// no terminal in sight -- a qWarning alone is invisible in practice, and a
// broken config should be loud, not a silent fallback the user never
// learns about. Best-effort: if notify-send isn't installed or nothing
// implements org.freedesktop.Notifications, this just doesn't pop
// anything; qWarning still covers the terminal-launched case.
void reportConfigError(const QString &message) {
  qWarning() << "shinto:" << message;
  QProcess::startDetached(
      QStringLiteral("notify-send"),
      {QStringLiteral("-u"), QStringLiteral("critical"), QStringLiteral("-a"),
       QStringLiteral("Shinto"), QStringLiteral("Shinto config.lua error"), message});
}

}  // namespace

ShintoConfig loadConfig() {
  ShintoConfig config;
  const QString path = configLuaPath();
  if (!QFile::exists(path)) {
    return config;  // no config.lua yet -- nothing to report
  }

  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dofile(L, path.toLocal8Bit().constData()) != LUA_OK) {
    reportConfigError(QStringLiteral("%1 -- using the default search engine instead")
                           .arg(QString::fromUtf8(lua_tostring(L, -1))));
    lua_close(L);
    return config;
  }

  lua_getglobal(L, "search_engine");
  if (lua_isstring(L, -1)) {
    const QString value = QString::fromUtf8(lua_tostring(L, -1));
    if (value.contains(QStringLiteral("%s"))) {
      config.searchEngineUrl = value;
    } else {
      reportConfigError(QStringLiteral("search_engine (%1) has no %s query placeholder -- "
                                        "using the default search engine instead")
                             .arg(value));
    }
  } else if (!lua_isnil(L, -1)) {
    reportConfigError(QStringLiteral(
        "search_engine must be a string -- using the default search engine instead"));
  }
  lua_pop(L, 1);

  lua_close(L);
  return config;
}

}  // namespace shinto
