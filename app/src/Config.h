// User-editable settings loaded from a real Lua config file
// (shinto::configLuaPath(), normally ~/.config/shinto/config.lua) via an
// embedded Lua interpreter. Currently just the search engine, but unlike
// ThemeLoader's flat colors.toml reader this one runs actual Lua, so a
// config file can compute a value however it likes (env vars via os.getenv,
// conditionals, etc.) as long as it ends up setting the right globals.
#pragma once

#include <QString>

namespace shinto {

struct ShintoConfig {
  // URL template used for the omnibox's search fallback (whatever wasn't
  // recognized as a bare URL -- see HistoryStore::toUrl). "%s" is replaced
  // with the percent-encoded query. Overridden by config.lua setting the
  // global `search_engine`, e.g.:
  //   search_engine = "https://www.google.com/search?q=%s"
  QString searchEngineUrl = QStringLiteral("https://duckduckgo.com/?q=%s");
};

// Executes configLuaPath() (if it exists) and reads back recognized
// globals. A missing file, a Lua syntax/runtime error, or a `search_engine`
// that isn't a string or has no "%s" placeholder all fall back to
// ShintoConfig's defaults, with a qWarning for the latter two -- a broken
// user config should never stop Shinto from starting.
ShintoConfig loadConfig();

}  // namespace shinto
