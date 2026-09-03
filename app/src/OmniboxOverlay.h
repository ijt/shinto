// The native omnibox: a widget drawn on top of a BrowserWindow's
// QWebEngineView. Two modes off one flag: a fresh window's empty gate --
// just a caret at the top-left, no visible field at rest -- and a thin
// "edit in place" bar over the current page (Ctrl+L), which looks like an
// address bar. A dropdown of offline domain-prefix suggestions
// (PopularDomains) appears below/above the input as you type; nothing
// typed ever leaves the machine to power it.
//
// Unlike the old gate, this is never a navigable URL -- Escape simply hides
// it, since the underlying page was never touched.
#pragma once

#include <QVector>
#include <QWidget>

#include "HistoryStore.h"
#include "PopularDomains.h"
#include "ThemeLoader.h"

class QLineEdit;
class QListWidget;
class QResizeEvent;

namespace shinto {

class OmniboxOverlay : public QWidget {
  Q_OBJECT

 public:
  OmniboxOverlay(HistoryStore *history, const PopularDomains *domains, QWidget *parent = nullptr);

  void applyPalette(const Palette &palette);

  // Empty-gate mode: just a caret at the top-left, no visible field --
  // clears the input, focuses it.
  void showEmpty();

  // Edit-in-place mode: prefills with `currentUrl`, whole address selected
  // (so typing replaces it), focuses the input. Caller (BrowserWindow) is
  // responsible for sizing this widget as a thin bar rather than
  // full-screen.
  void showEditing(const QString &currentUrl);

  // Hides without navigating anywhere -- what Escape does.
  void hideOverlay();

  bool isEmptyMode() const { return emptyMode_; }

  // How tall this widget wants to be in editing mode: the input row plus
  // margins, plus the suggestion list's actual height when it's showing.
  int preferredEditingHeight() const;

 signals:
  // The user submitted a destination (typed text resolved via
  // HistoryStore::toUrl, or picked a suggestion).
  void navigateRequested(const QString &url);
  // The user backed out (Escape) without navigating.
  void cancelled();
  // The suggestion list appeared/disappeared/changed size --
  // preferredEditingHeight() has a new answer, so the caller (in editing
  // mode) should re-layout.
  void contentSizeChanged();

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

 private:
  void submit();
  void layoutInput();
  void updateSuggestions();
  void renderSuggestions(const QVector<PopularDomains::Suggestion> &items);
  void clearSuggestions();
  void moveSelection(int delta);
  void applySelectionToInput();
  int suggestionListHeight() const;

  HistoryStore *history_;
  const PopularDomains *domains_;
  QLineEdit *input_;
  QListWidget *list_;
  bool emptyMode_ = true;
  QVector<PopularDomains::Suggestion> items_;
};

}  // namespace shinto
