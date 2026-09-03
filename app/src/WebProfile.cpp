#include "WebProfile.h"

#include <QDir>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include "Shinto.h"

namespace shinto {

namespace {

// Scrollbars are visual clutter against this app's aesthetic (no chrome,
// no boxes) -- content stays scrollable, the track/thumb are just never
// drawn. Injected as a real stylesheet (not a Qt/Chromium flag) so it's
// unconditional and can't get stuck in some other visual state the way
// the OverlayScrollbar feature flag did.
void installScrollbarHidingScript(QWebEngineProfile *profile) {
  QWebEngineScript script;
  script.setName(QStringLiteral("shinto-hide-scrollbars"));
  script.setInjectionPoint(QWebEngineScript::DocumentCreation);
  script.setWorldId(QWebEngineScript::MainWorld);
  script.setRunsOnSubFrames(true);
  script.setSourceCode(QStringLiteral(
      "(function() {"
      "  var s = document.createElement('style');"
      "  s.textContent ="
      "    '*{scrollbar-width:none!important;-ms-overflow-style:none!important;}"
      "     *::-webkit-scrollbar{display:none!important;width:0!important;height:0!important;}';"
      "  (document.documentElement || document).appendChild(s);"
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
