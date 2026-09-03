// One window = one page (matching Shinto's long-standing philosophy).
// Owns a QWebEngineView and an OmniboxOverlay, and is its own state
// machine: Empty (fresh window, blank gate shown, nothing loaded yet) <->
// Loaded (gate hidden, page visible) <-> Gate (Ctrl+L on a loaded page
// shows the same gate on top of it, prefilled with the current URL and
// fully selected -- Escape reverts to Loaded; the empty window's Empty
// state has nothing to revert to, so Ctrl+L there is a no-op). There is
// no separate "gate window" type -- see the plan's "Window/overlay model"
// section.
#pragma once

#include <QMainWindow>
#include <QVector>

#include "HistoryStore.h"
#include "PopularDomains.h"
#include "ThemeLoader.h"

class QWebEngineProfile;
class QResizeEvent;

namespace shinto {

class OmniboxOverlay;
class WebView;

class BrowserWindow : public QMainWindow {
  Q_OBJECT

 public:
  // Creates, registers, shows, and returns a new window. `url` empty means
  // start in the empty-gate state; non-empty loads it immediately.
  static BrowserWindow *spawn(QWebEngineProfile *profile, HistoryStore *history,
                               const PopularDomains *domains, const QString &url);

  // Re-applies a reloaded theme to every live window (the `shinto theme` /
  // Omarchy theme-set-hook path, delivered over the singleton socket).
  static void applyPaletteToAll(const Palette &palette);

 ~BrowserWindow() override;

 protected:
  void resizeEvent(QResizeEvent *event) override;

 private:
  enum class State { Empty, Loaded, Gate };

  BrowserWindow(QWebEngineProfile *profile, HistoryStore *history, const PopularDomains *domains,
                const QString &url);

  void relayout();
  void enterEmpty();
  void showGateOverPage();
  void onOverlayNavigate(const QString &url);
  void onOverlayCancelled();
  void onNewPageShortcut();
  void onEditAddressShortcut();

  HistoryStore *history_;
  const PopularDomains *domains_;
  WebView *webView_;
  OmniboxOverlay *overlay_;
  State state_ = State::Empty;

  static QVector<BrowserWindow *> instances_;
  static Palette currentPalette_;
};

}  // namespace shinto
