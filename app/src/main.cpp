// TEMPORARY diagnostic build: like the previous commit, but the
// QWebEngineView is never given a URL at all (no setUrl call) -- this is
// the one concrete difference between this test and BrowserWindow's real
// "Empty" state (a freshly-opened window, nothing loaded, omnibox shown
// full-screen): does an idle/never-navigated QWebEngineView flicker where
// an actively-loading one (previous commit) didn't?
#include <QApplication>
#include <QMainWindow>
#include <QSurfaceFormat>
#include <QWebEngineView>

int main(int argc, char *argv[]) {
  QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  {
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
  }
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");

  QApplication app(argc, argv);

  QMainWindow win;
  win.setWindowTitle(QStringLiteral("Shinto (idle web view test)"));
  auto *view = new QWebEngineView(&win);
  // Deliberately no setUrl() call -- this is the difference from the last
  // test.
  win.setCentralWidget(view);
  win.resize(1200, 800);
  win.show();

  return app.exec();
}
