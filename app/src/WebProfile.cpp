#include "WebProfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include "Notify.h"
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

// QtWebEngine's WebAuthn support is incomplete in a way that hangs pages,
// not just lacks features: PublicKeyCredential.getClientCapabilities()
// (and the older isUserVerifyingPlatformAuthenticatorAvailable() /
// isConditionalMediationAvailable() feature-detection calls) never settle
// -- the promise neither resolves nor rejects, ever -- confirmed as a
// standing QtWebEngine issue via another QtWebEngine-based browser
// (qutebrowser #8930), not something specific to one site. A page that
// awaits one of these before deciding whether to offer passkey/QR-code UI
// or fall back to a password is left permanently stuck (reproduced
// concretely: "Continue with Apple" on x.com hangs on a spinner forever
// after submitting an email, and never offers its usual "sign in via
// iPhone" option either -- both are this).
//
// Races each real call against a timeout rather than unconditionally
// stubbing them out: if a native call actually resolves (fixed in a
// future QtWebEngine, or a real platform authenticator answers), that
// real answer wins; only a call that's actually hung falls back to "no
// WebAuthn here" once the timeout elapses. This can't restore the
// QR-code/hybrid-transport flow itself -- Qt's own docs for
// QWebEngineWebAuthUxRequest list only user verification, resident
// credentials, and failure UX as supported, nothing hybrid-transport-
// shaped -- but it stops the hang, letting a site fall through to
// password auth on its own instead of freezing.
//
// Unlike the scrollbar script above, this runs at DocumentCreation
// (Chromium's document_start), not DocumentReady: it never touches the
// DOM (only static methods on the PublicKeyCredential global, which
// exists before any script runs regardless of parse state), so the
// DocumentReady-only reasoning above doesn't apply -- and it has to be in
// place before the page's own feature-detection call fires, which an
// early <head> inline script or auth SDK may do well before
// DocumentReady.
void installWebAuthnCapabilityShim(QWebEngineProfile *profile) {
  QWebEngineScript script;
  script.setName(QStringLiteral("shinto-webauthn-capability-shim"));
  script.setInjectionPoint(QWebEngineScript::DocumentCreation);
  script.setWorldId(QWebEngineScript::MainWorld);
  script.setRunsOnSubFrames(true);
  script.setSourceCode(QStringLiteral(R"JS(
(function() {
  if (typeof PublicKeyCredential === 'undefined') return;
  var TIMEOUT_MS = 2500; // generous: "hangs forever" -> "waits 2.5s" is
                          // still a huge win, and keeps the window for a
                          // false early answer narrow.

  function shim(name, fallbackValue) {
    // These are STATIC methods on the class itself per the WebAuthn
    // spec, not prototype methods.
    if (typeof PublicKeyCredential[name] !== 'function') return; // absent on this build
    var real = PublicKeyCredential[name].bind(PublicKeyCredential);
    function patched() {
      var args = arguments;
      return new Promise(function(resolve) {
        var settled = false;
        var timer = setTimeout(function() {
          if (settled) return;
          settled = true;
          resolve(fallbackValue);
        }, TIMEOUT_MS);
        var realPromise;
        try { realPromise = real.apply(PublicKeyCredential, args); }
        catch (e) {
          if (!settled) { settled = true; clearTimeout(timer); resolve(fallbackValue); }
          return;
        }
        realPromise.then(function(v) {
          if (settled) return;
          settled = true; clearTimeout(timer); resolve(v);
        }, function() {
          if (settled) return;
          settled = true; clearTimeout(timer); resolve(fallbackValue);
        });
      });
    }
    try {
      Object.defineProperty(PublicKeyCredential, name,
        { value: patched, writable: true, configurable: true, enumerable: false });
    } catch (e) { /* non-configurable on this build: leave native behavior alone */ }
  }

  shim('getClientCapabilities', {
    conditionalCreate: false, conditionalGet: false, hybridTransport: false,
    passkeyPlatformAuthenticator: false, userVerifyingPlatformAuthenticator: false,
    relatedOrigins: false, signalAllAcceptedCredentials: false,
    signalCurrentUserDetails: false, signalUnknownCredential: false
  });
  shim('isUserVerifyingPlatformAuthenticatorAvailable', false);
  shim('isConditionalMediationAvailable', false);
})();
)JS"));
  profile->scripts()->insert(script);
}

// Chromium's own DownloadInterruptReason taxonomy mixes transient
// network/server hiccups in with things retrying can't fix (bad disk,
// blocked file, user cancellation). Reproduced concretely: a real
// multi-hundred-MB download from a CDN that curl pulls without a hitch
// (400MB straight through at a steady 5.8MB/s) still gets killed by
// QtWebEngine's network stack with NetworkFailed every 30-90 seconds on
// this connection -- not this specific site's fault, not GPU/QUIC flags
// (confirmed unaffected by --disable-quic), just a flaky link Chromium's
// downloader is happy to resume if asked. Only the reasons below are worth
// retrying; anything else (a real disk problem, a rejected/blocked file,
// the user hitting cancel) would just fail again identically.
bool isRetryableInterrupt(QWebEngineDownloadRequest::DownloadInterruptReason reason) {
  switch (reason) {
    case QWebEngineDownloadRequest::NetworkFailed:
    case QWebEngineDownloadRequest::NetworkTimeout:
    case QWebEngineDownloadRequest::NetworkDisconnected:
    case QWebEngineDownloadRequest::NetworkServerDown:
    case QWebEngineDownloadRequest::ServerFailed:
    case QWebEngineDownloadRequest::ServerUnreachable:
      return true;
    default:
      return false;
  }
}

// Nothing in Shinto connected to QWebEngineProfile::downloadRequested, and
// an unhandled download just sits forever in the DownloadRequested state --
// nothing is written to disk, and there's no error either, so a user
// clicking a download link (confirmed concretely: a .dmg/.exe/.tar.gz from
// jetbrains.com) sees literally nothing happen and has no way to tell
// whether it worked. Shinto has no download-manager UI to show progress in
// instead, so this accepts every download straight into the platform
// Downloads folder and narrates start/finish/failure via desktop
// notification (see Notify.h) -- the same "no terminal in sight" reasoning
// that already justified one for config errors.
void installDownloadHandler(QWebEngineProfile *profile) {
  QObject::connect(
      profile, &QWebEngineProfile::downloadRequested, profile,
      [](QWebEngineDownloadRequest *download) {
        if (!download) return;

        const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QDir().mkpath(dir);
        download->setDownloadDirectory(dir);

        // QWebEngineDownloadRequest doesn't dedupe filenames the way
        // Chromium's own save-as dialog would -- a second download of the
        // same name would silently clobber the first one without this.
        QString name = download->suggestedFileName();
        if (name.isEmpty()) name = QStringLiteral("download");
        const QFileInfo info(name);
        const QString base = info.completeBaseName();
        const QString ext = info.suffix();
        QString candidate = name;
        for (int n = 1; QFile::exists(dir + QLatin1Char('/') + candidate); ++n) {
          candidate = ext.isEmpty() ? QStringLiteral("%1 (%2)").arg(base).arg(n)
                                     : QStringLiteral("%1 (%2).%3").arg(base).arg(n).arg(ext);
        }
        download->setDownloadFileName(candidate);
        download->accept();

        notify(QStringLiteral("Download started"), candidate);

        // Retries silently (no notification) up to this many times before
        // reporting failure -- observed concretely needing more than one or
        // two on this network for a ~1.2GB file, since each interruption
        // costs nothing but a resume() (the CDN supports Range requests, so
        // this continues rather than restarting from scratch), not a full
        // redownload.
        auto retriesLeft = std::make_shared<int>(8);
        QObject::connect(
            download, &QWebEngineDownloadRequest::stateChanged, download,
            [download, candidate, retriesLeft](QWebEngineDownloadRequest::DownloadState state) {
              switch (state) {
                case QWebEngineDownloadRequest::DownloadCompleted:
                  notify(QStringLiteral("Download complete"), candidate);
                  break;
                case QWebEngineDownloadRequest::DownloadInterrupted: {
                  const auto reason = download->interruptReason();
                  if (isRetryableInterrupt(reason) && *retriesLeft > 0) {
                    --*retriesLeft;
                    // A moment's grace before resuming rather than
                    // hammering the connection back immediately.
                    QPointer<QWebEngineDownloadRequest> guarded(download);
                    QTimer::singleShot(1000, [guarded]() {
                      if (guarded) guarded->resume();
                    });
                    break;
                  }
                  notify(QStringLiteral("Download failed"),
                         candidate + QStringLiteral(": ") + download->interruptReasonString(),
                         /*critical=*/true);
                  break;
                }
                default:
                  break;
              }
            });
      });
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
  installWebAuthnCapabilityShim(profile);
  installDownloadHandler(profile);

  return profile;
}

}  // namespace shinto
