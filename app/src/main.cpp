// CLI entry point and dual-mode dispatch: either hand a request off to an
// already-running Shinto daemon (the common case -- a warm process just
// opens another QMainWindow) or become the daemon. Replaces the bash
// `shinto` script's open_page/ensure_daemon/run_daemon and Chromium's own
// SingletonSocket.
#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QSurfaceFormat>
#include <QWebEngineProfile>

#include "BrowserWindow.h"
#include "HistoryStore.h"
#include "Shinto.h"
#include "SingletonClient.h"
#include "SingletonServer.h"
#include "ThemeLoader.h"
#include "WebProfile.h"

namespace {

QString openCommand(const QString &url) {
  return url.isEmpty() ? QStringLiteral("OPEN") : QStringLiteral("OPEN ") + url;
}

}  // namespace

int main(int argc, char *argv[]) {
  // Required before any QApplication exists for QtWebEngine to share GL
  // contexts correctly. Without it, a BrowserWindow's first QWebEngineView
  // can force Qt to tear down and recreate the window's native surface once
  // it's already been shown -- visible as a startup close+reopen flicker,
  // even though it's the same logical window throughout.
  QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  // The other half of that same requirement: QtWebEngine's example apps
  // (simplebrowser et al.) set an explicit default surface format before
  // any window exists, so the GL surface type a BrowserWindow gets created
  // with the first time is already the one QtWebEngine needs -- instead of
  // Qt discovering only once the page actually starts compositing that it
  // needs a different kind of native surface than the one already handed
  // to the (already-mapped) window.
  {
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
  }

  // Must be set before QtWebEngine's Chromium backend initializes, so this
  // has to happen before anything else touches it.
  {
    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (!flags.isEmpty()) flags += ' ';
    // Thin, auto-hiding scrollbars (shown only while scrolling) for page
    // content.
    flags += "--enable-features=OverlayScrollbar ";
    // This machine's Wayland/DRM GBM+EGL native-buffer path for GPU
    // compositing hits a hard Chromium-side failure (gbm_bo_import
    // returning nullptr, EGL_BAD_MATCH, then a fatal abort) -- reproduced
    // on every run with real (non-offscreen) rendering.
    // --disable-gpu-compositing alone still lets Chromium attempt GPU
    // compositing first and fall back, and that fallback transition tears
    // down and recreates the native surface -- visible as a one-time
    // close+reopen right at startup. --disable-gpu skips GPU compositing
    // attempts entirely, avoiding both the crash and that transition.
    flags += "--disable-gpu";
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
  }
  // Silence a harmless xdg-desktop-portal warning
  // ("qt.qpa.services: Failed to register with host portal ... Connection
  // already associated with an application ID") that fires here but not
  // anything we can act on -- it doesn't affect behavior.
  {
    QByteArray rules = qgetenv("QT_LOGGING_RULES");
    if (!rules.isEmpty()) rules += ';';
    rules += "qt.qpa.services=false";
    qputenv("QT_LOGGING_RULES", rules);
  }

  QStringList args;
  for (int i = 1; i < argc; ++i) {
    args << QString::fromLocal8Bit(argv[i]);
  }
  const bool forceDaemon = args.removeOne(QStringLiteral("--daemon"));
  const QString url = args.isEmpty() ? QString() : args.first();

  qWarning("shinto: [debug] pid=%lld forceDaemon=%d url=%s", (long long)QCoreApplication::applicationPid(), forceDaemon, qPrintable(url));

  if (!forceDaemon) {
    // Cheap path: a plain QCoreApplication is enough to drive the local
    // socket handoff, so a `shinto <url>` invocation against an already
    // warm daemon never touches the GUI/Wayland platform plugin at all.
    bool handedOff = false;
    {
      QCoreApplication probe(argc, argv);
      handedOff = shinto::SingletonClient::tryHandoff(openCommand(url));
    }
    qWarning("shinto: [debug] initial probe handedOff=%d", handedOff);
    if (handedOff) {
      return 0;
    }
  }

  // No daemon answered (or --daemon forces this unconditionally): this
  // process becomes the daemon.
  qWarning("shinto: [debug] becoming the daemon");
  QApplication app(argc, argv);
  app.setApplicationName(QString::fromLatin1(shinto::kAppId));
  app.setDesktopFileName(QString::fromLatin1(shinto::kAppId));
  // The whole point of dropping the old hidden spare-window trick: a warm
  // daemon with zero windows open is simply a QApplication that doesn't
  // quit when the last window closes.
  app.setQuitOnLastWindowClosed(false);

  QWebEngineProfile *profile = shinto::createSharedProfile(&app);

  shinto::HistoryStore history;
  if (!history.open()) {
    qWarning() << "shinto: continuing without persistent typed/visited history";
  }

  shinto::BrowserWindow::applyPaletteToAll(shinto::loadPalette());

  shinto::SingletonServer server;
  QObject::connect(&server, &shinto::SingletonServer::openRequested,
                    [profile, &history](const QString &openUrl) {
                      qWarning("shinto: [debug] server openRequested url=%s", qPrintable(openUrl));
                      shinto::BrowserWindow::spawn(profile, &history, openUrl);
                    });
  QObject::connect(&server, &shinto::SingletonServer::themeReloadRequested,
                    [] { shinto::BrowserWindow::applyPaletteToAll(shinto::loadPalette()); });

  const bool listening = server.listen();
  qWarning("shinto: [debug] server.listen() -> %d", listening);
  if (!listening) {
    // Lost a race with another process becoming the daemon in the tiny
    // window since our own handoff attempt failed above.
    if (shinto::SingletonClient::tryHandoff(openCommand(url))) {
      return 0;
    }
    qWarning() << "shinto: could not become the daemon and handoff failed";
    return 1;
  }

  if (!forceDaemon) {
    qWarning("shinto: [debug] direct spawn() from main()");
    shinto::BrowserWindow::spawn(profile, &history, url);
  }
  // else: `--daemon` (the systemd unit) starts with zero windows, warm and
  // waiting for the first Super+Shift+Return handoff.

  const int rc = app.exec();
  qWarning("shinto: [debug] app.exec() returned %d", rc);
  return rc;
}
