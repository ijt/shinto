#include "WebProfile.h"

#include <QDir>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include "Shinto.h"

namespace shinto {

namespace {

// Chromium's own --enable-features=OverlayScrollbar (see main.cpp) only
// changes the *browser's* default scrollbar rendering; a site that styles
// its own scrollbars via CSS (::-webkit-scrollbar rules -- common, and
// what DuckDuckGo's results page does) overrides that entirely. Only
// injected CSS can force it site-wide.
//
// This replaces an earlier attempt that injected at DocumentCreation
// (Chromium's document_start) with a documentElement-or-document
// fallback -- at that very early point the parser hasn't necessarily
// created <html>/<head> yet, and appending to `document` itself can
// insert our <style> as the document's root element ahead of the real
// one, corrupting the rest of parsing and blanking the page (reproduced
// on a real navigation). DocumentReady runs after the DOM is fully
// parsed, so document.head is guaranteed to exist for any real HTML
// page; we just skip non-HTML documents outright rather than guess.
void installScrollbarHidingScript(QWebEngineProfile *profile) {
  QWebEngineScript script;
  script.setName(QStringLiteral("shinto-hide-scrollbars"));
  script.setInjectionPoint(QWebEngineScript::DocumentReady);
  script.setWorldId(QWebEngineScript::MainWorld);
  script.setRunsOnSubFrames(true);
  script.setSourceCode(QStringLiteral(
      "(function() {"
      "  if (!document.head) return;"
      "  var s = document.createElement('style');"
      "  s.textContent ="
      "    '*{scrollbar-width:none!important;-ms-overflow-style:none!important;}"
      "     *::-webkit-scrollbar{display:none!important;width:0!important;height:0!important;}';"
      "  document.head.appendChild(s);"
      "})();"));
  profile->scripts()->insert(script);
}

}  // namespace

QWebEngineProfile *createSharedProfile(QObject *parent) {
  const QString storage = webEngineStoragePath();
  QDir().mkpath(storage);

  // Named, persistent (non-off-the-record) profile. QWebEngineProfile picks
  // sane cache/storage subpaths under persistentStoragePath by default; we
  // just point it at our own directory instead of Qt's default location.
  auto *profile = new QWebEngineProfile(QStringLiteral("shinto"), parent);
  profile->setPersistentStoragePath(storage);
  profile->setCachePath(storage + "/cache");
  profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
  profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);

  QWebEngineSettings *settings = profile->settings();
  settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
  settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
  settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);

  installScrollbarHidingScript(profile);

  return profile;
}

}  // namespace shinto
