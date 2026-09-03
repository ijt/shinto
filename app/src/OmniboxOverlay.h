// The native omnibox: a widget drawn on top of a BrowserWindow's
// QWebEngineView. One appearance: a caret at the top-left, no visible
// field at rest -- used both for a fresh window's empty gate and for
// Ctrl+L on an already-loaded page (which shows this same blank gate over
// the still-loaded page, rather than a prefilled address bar). A dropdown
// of offline domain-prefix suggestions (PopularDomains) appears below the
// input as you type; nothing typed ever leaves the machine to power it.
//
// This is never a navigable URL -- Escape simply hides it, since the
// underlying page (if any) was never touched.
#pragma once

#include <QVector>
#include <QWidget>

#include "Config.h"
#include "HistoryStore.h"
#include "PopularDomains.h"
#include "ThemeLoader.h"

class QGraphicsOpacityEffect;
class QLineEdit;
class QListWidget;
class QPropertyAnimation;
class QResizeEvent;

namespace shinto {

class OmniboxOverlay : public QWidget {
  Q_OBJECT

 public:
  OmniboxOverlay(HistoryStore *history, const PopularDomains *domains, const ShintoConfig *config,
                 QWidget *parent = nullptr);

  void applyPalette(const Palette &palette);

  // Shows the gate, focuses it. `prefill` empty means a blank gate (a
  // fresh window); non-empty (Ctrl+L on a loaded page) fills the input
  // with it, fully selected, so typing replaces it but Enter alone just
  // re-submits the current address. BrowserWindow always gives this
  // widget the full window area before calling this.
  void showGate(const QString &prefill = QString());

  // Hides without navigating anywhere -- what Escape does.
  void hideOverlay();

  // A thin accent-colored line at the top, `percent` of the window wide --
  // the only feedback while the gate stays up waiting for a just-submitted
  // navigation to actually load (see BrowserWindow::onOverlayNavigate).
  void setProgress(int percent);

 signals:
  // The user submitted a destination (typed text resolved via
  // HistoryStore::toUrl (using config_->searchEngineUrl), or picked a
  // suggestion).
  void navigateRequested(const QString &url);
  // The user backed out (Escape) without navigating.
  void cancelled();

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

 private:
  void submit();
  void layoutInput();
  void layoutProgressBar();
  void updateSuggestions();
  void renderSuggestions(const QVector<PopularDomains::Suggestion> &items);
  void clearSuggestions();
  void moveSelection(int delta);
  void applySelectionToInput();
  int suggestionListHeight() const;
  void startShimmer();
  void stopShimmer();

  HistoryStore *history_;
  const PopularDomains *domains_;
  const ShintoConfig *config_;
  QLineEdit *input_;
  QListWidget *list_;
  QWidget *progressBar_;
  QGraphicsOpacityEffect *inputOpacity_;
  QPropertyAnimation *shimmer_;
  int progress_ = 0;
  QVector<PopularDomains::Suggestion> items_;
};

}  // namespace shinto
