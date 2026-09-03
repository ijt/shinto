// The native omnibox: a widget drawn on top of a BrowserWindow's
// QWebEngineView. Two modes off one flag: a fresh window's empty gate --
// just a caret at the top-left, no visible field -- and a thin "edit in
// place" bar over the current page (Ctrl+L), which looks like an address
// bar. Just a text input -- no suggestion dropdown.
//
// Unlike the old gate, this is never a navigable URL -- Escape simply hides
// it, since the underlying page was never touched.
#pragma once

#include <QWidget>

#include "HistoryStore.h"
#include "ThemeLoader.h"

class QLineEdit;
class QResizeEvent;

namespace shinto {

class OmniboxOverlay : public QWidget {
  Q_OBJECT

 public:
  explicit OmniboxOverlay(HistoryStore *history, QWidget *parent = nullptr);

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

  // How tall this widget wants to be in editing mode: just the input row
  // plus margins.
  int preferredEditingHeight() const;

 signals:
  // The user submitted a destination (typed text resolved via
  // HistoryStore::toUrl).
  void navigateRequested(const QString &url);
  // The user backed out (Escape) without navigating.
  void cancelled();

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

 private:
  void submit();
  void layoutInput();

  HistoryStore *history_;
  QLineEdit *input_;
  bool emptyMode_ = true;
};

}  // namespace shinto
