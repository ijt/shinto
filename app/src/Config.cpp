#include "Config.h"

#include <QDebug>
#include <QFile>
#include <QUrl>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "Shinto.h"

namespace shinto {

ShintoConfig loadConfig() {
  ShintoConfig config;
  const QString path = configLuaPath();
  if (!QFile::exists(path)) {
    return config;  // no config.lua yet -- defaults, nothing to warn about
  }

  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dofile(L, path.toLocal8Bit().constData()) != LUA_OK) {
    qWarning() << "shinto: error running" << path << "--" << lua_tostring(L, -1);
    lua_close(L);
    return config;
  }

  lua_getglobal(L, "search_engine");
  if (lua_isstring(L, -1)) {
    const QString value = QString::fromUtf8(lua_tostring(L, -1));
    if (value.contains(QStringLiteral("%s"))) {
      config.searchEngineUrl = value;
    } else {
      qWarning() << "shinto: config.lua's search_engine has no %s query "
                     "placeholder -- ignoring, keeping the default";
    }
  } else if (!lua_isnil(L, -1)) {
    qWarning() << "shinto: config.lua's search_engine must be a string -- ignoring";
  }
  lua_pop(L, 1);

  lua_close(L);
  return config;
}

}  // namespace shinto
