// One window = one page (matching Shinto's long-standing philosophy).
// Owns a QWebEngineView and an OmniboxOverlay, and is its own state machine:
// Empty (fresh window, overlay full-screen, nothing loaded yet) <-> Loaded
// (overlay hidden, page visible) <-> Editing (overlay is a thin bar over the
// still-visible page). There is no separate "gate window" type anymore --
// see the plan's "Window/overlay model" section.
#pragma once

#include <QMainWindow>
#include <QVector>

#include "HistoryStore.h"
#include "ThemeLoader.h"

class QWebEngineProfile;
class QResizeEvent;
class QCloseEvent;

namespace shinto {

class OmniboxOverlay;
class WebView;

class BrowserWindow : public QMainWindow {
  Q_OBJECT

 public:
  // Creates, registers, shows, and returns a new window. `url` empty means
  // start in the empty-gate state; non-empty loads it immediately.
  static BrowserWindow *spawn(QWebEngineProfile *profile, HistoryStore *history,
                               const QString &url);

  // Re-applies a reloaded theme to every live window (the `shinto theme` /
  // Omarchy theme-set-hook path, delivered over the singleton socket).
  static void applyPaletteToAll(const Palette &palette);

 ~BrowserWindow() override;

 protected:
  void resizeEvent(QResizeEvent *event) override;
  void closeEvent(QCloseEvent *event) override;  // TODO(debug): remove after diagnosing the startup flicker

 private:
  enum class State { Empty, Loaded, Editing };

  BrowserWindow(QWebEngineProfile *profile, HistoryStore *history, const QString &url);

  void relayout();
  void enterEmpty();
  void enterEditing();
  void onOverlayNavigate(const QString &url);
  void onOverlayCancelled();
  void onNewPageShortcut();
  void onEditAddressShortcut();

  HistoryStore *history_;
  WebView *webView_;
  OmniboxOverlay *overlay_;
  State state_ = State::Empty;

  static QVector<BrowserWindow *> instances_;
  static Palette currentPalette_;
};

}  // namespace shinto
