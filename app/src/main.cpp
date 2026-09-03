// TEMPORARY diagnostic build: nothing but a QMainWindow containing a
// QWebEngineView loaded to example.com. No daemon/singleton logic, no
// overlay, no custom profile -- to isolate whether the intermittent
// startup close+reopen flicker is inherent to QWebEngineView itself, or
// caused by something in our own BrowserWindow/OmniboxOverlay scaffolding.
// The real main.cpp is in git history (`git log -- app/src/main.cpp`) and
// will be restored once this test answers that question.
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
  win.setWindowTitle(QStringLiteral("Shinto (web view test)"));
  auto *view = new QWebEngineView(&win);
  view->setUrl(QUrl(QStringLiteral("https://example.com")));
  win.setCentralWidget(view);
  win.resize(1200, 800);
  win.show();

  return app.exec();
}
