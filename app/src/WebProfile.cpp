#include "WebProfile.h"

#include <QDir>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include "Shinto.h"

namespace shinto {

namespace {

// Chromium's own --enable-features=OverlayScrollbar (see main.cpp) is
// supposed to give exactly this "hidden, appears while scrolling, fades
// back out" behavior, but produces no visible effect in this build (and
// wouldn't survive a site styling its own scrollbars via CSS anyway, like
// DuckDuckGo's results page does) -- so it's implemented by hand here:
// scrollbars are zero-width by default, and a capture-phase `scroll`
// listener (scroll events don't bubble, but they do fire on ancestors
// during capture) adds a class that makes the scrollbar for whatever
// element is actually scrolling visible, removing it again after a short
// idle period. A WeakMap keyed by element keeps each scrollable region's
// timer independent.
//
// Runs at DocumentReady, not DocumentCreation (Chromium's document_start):
// an earlier attempt injected there with a documentElement-or-document
// fallback, and at that very early point the parser hasn't necessarily
// created <html>/<head> yet -- appending to `document` itself can insert
// our <style> as the document's root element ahead of the real one,
// corrupting the rest of parsing and blanking the page (reproduced on a
// real navigation). DocumentReady runs after the DOM is fully parsed, so
// document.head is guaranteed to exist for any real HTML page; non-HTML
// documents are simply skipped rather than guessed at.
void installScrollbarHidingScript(QWebEngineProfile *profile) {
  QWebEngineScript script;
  script.setName(QStringLiteral("shinto-hide-scrollbars"));
  script.setInjectionPoint(QWebEngineScript::DocumentReady);
  script.setWorldId(QWebEngineScript::MainWorld);
  script.setRunsOnSubFrames(true);
  script.setSourceCode(QStringLiteral(R"JS(
(function() {
  if (!document.head) return;
  var style = document.createElement('style');
  style.textContent =
    '::-webkit-scrollbar{width:0px;height:0px;background:transparent;}' +
    '.shinto-scrolling::-webkit-scrollbar{width:8px!important;height:8px!important;}' +
    '.shinto-scrolling::-webkit-scrollbar-track{background:transparent!important;}' +
    '.shinto-scrolling::-webkit-scrollbar-thumb{background:rgba(255,255,255,.35)!important;border-radius:4px;}';
  document.head.appendChild(style);

  var timers = new WeakMap();
  document.addEventListener('scroll', function(e) {
    var el = e.target === document ? document.documentElement : e.target;
    if (!el || el.nodeType !== 1) return;
    el.classList.add('shinto-scrolling');
    clearTimeout(timers.get(el));
    timers.set(el, setTimeout(function() { el.classList.remove('shinto-scrolling'); }, 700));
  }, { capture: true, passive: true });
})();
)JS"));
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
  // Without this, sites (YouTube) detect no Fullscreen API and gray out
  // the button with "Fullscreen is unavailable". Accepting the matching
  // QWebEnginePage::fullScreenRequested signal is still required in
  // BrowserWindow.
  settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

  installScrollbarHidingScript(profile);

  return profile;
}

}  // namespace shinto
