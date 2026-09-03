// TEMPORARY diagnostic build: previous commit proved an idle/never-
// navigated QWebEngineView flickers while an actively-loading one
// doesn't. This tests the cheap fix: load "about:blank" explicitly (a
// real navigation Chromium actually commits/paints a frame for) instead
// of leaving the view truly untouched -- still looks empty to the user,
// but gives QtWebEngine's compositor something to actually attach to.
#include <QApplication>
#include <QMainWindow>
#include <QSurfaceFormat>
#include <QUrl>
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
  win.setWindowTitle(QStringLiteral("Shinto (about:blank test)"));
  auto *view = new QWebEngineView(&win);
  view->setUrl(QUrl(QStringLiteral("about:blank")));
  win.setCentralWidget(view);
  win.resize(1200, 800);
  win.show();

  return app.exec();
}
