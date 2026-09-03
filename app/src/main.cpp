// TEMPORARY diagnostic build: just a blank QMainWindow, no QWebEngineView,
// no daemon/singleton logic at all -- to isolate whether the intermittent
// startup close+reopen flicker is caused by QtWebEngine specifically, or by
// something else (Qt/Wayland/compositor) unrelated to the browser. The real
// main.cpp is in git history (`git log -- app/src/main.cpp`) and will be
// restored once this test answers that question.
#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QMainWindow win;
  win.setWindowTitle(QStringLiteral("Shinto (blank test)"));
  win.resize(1200, 800);
  win.show();
  return app.exec();
}
