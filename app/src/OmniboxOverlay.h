// The native omnibox: a widget drawn on top of a BrowserWindow's
// QWebEngineView. Two modes off one flag: full-screen "empty gate" (a new
// window) and a thin "edit in place" bar over the current page (Ctrl+L).
// Replaces extension/newtab.html + newtab.js + complete.js + gate.css.
//
// Unlike the old gate, this is never a navigable URL -- Escape simply hides
// it, since the underlying page was never touched.
#pragma once

#include <QVector>
#include <QWidget>

#include "HistoryStore.h"
#include "ThemeLoader.h"

class QLineEdit;
class QListWidget;
class QTimer;

namespace shinto {

class OmniboxOverlay : public QWidget {
  Q_OBJECT

 public:
  explicit OmniboxOverlay(HistoryStore *history, QWidget *parent = nullptr);

  void applyPalette(const Palette &palette);

  // Full-screen empty-gate mode: clears the input, focuses it.
  void showEmpty();

  // Edit-in-place mode: prefills with `currentUrl`, caret at the end,
  // focuses the input. Caller (BrowserWindow) is responsible for sizing
  // this widget as a thin bar rather than full-screen.
  void showEditing(const QString &currentUrl);

  // Hides without navigating anywhere -- what Escape does.
  void hideOverlay();

  bool isEmptyMode() const { return emptyMode_; }

  // How tall this widget wants to be right now in editing mode: just the
  // input row, plus the suggestion list's actual content height when it's
  // showing. BrowserWindow uses this to size the edit-in-place bar instead
  // of a fixed height, so a taller suggestion list never gets crushed.
  int preferredEditingHeight() const;

 signals:
  // The user submitted a destination (typed text resolved via
  // HistoryStore::toUrl, or picked a suggestion).
  void navigateRequested(const QString &url);
  // The user backed out (Escape) without navigating.
  void cancelled();
  // The suggestion list appeared/disappeared/changed size -- preferredEditingHeight()
  // has a new answer, so the caller should re-layout.
  void contentSizeChanged();

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  void requestCompletions();
  void onCompletions(int requestId, QVector<HistoryStore::Suggestion> results);
  void renderSuggestions(const QVector<HistoryStore::Suggestion> &items);
  void clearSuggestions();
  void moveSelection(int delta);
  void applySelectionToInput();
  void submit();
  void caretToEnd();

  HistoryStore *history_;
  QLineEdit *input_;
  QListWidget *list_;
  QTimer *debounce_;
  bool emptyMode_ = true;
  int latestRequestId_ = 0;
  QVector<HistoryStore::Suggestion> items_;
};

}  // namespace shinto
