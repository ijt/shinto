#include "OmniboxOverlay.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>

namespace shinto {

namespace {
constexpr int kMargin = 24;
constexpr int kEmptyInputWidth = 280;
}  // namespace

OmniboxOverlay::OmniboxOverlay(HistoryStore *history, QWidget *parent)
    : QWidget(parent), history_(history) {
  setObjectName(QStringLiteral("OmniboxOverlay"));
  // A plain QWidget doesn't paint its stylesheet background by default --
  // without this, the page underneath shows through everywhere except
  // exactly where a child widget sits.
  setAttribute(Qt::WA_StyledBackground, true);

  input_ = new QLineEdit(this);
  input_->setAttribute(Qt::WA_MacShowFocusRect, false);
  input_->installEventFilter(this);
  // No QLayout -- position is managed by hand in layoutInput(), since the
  // two modes place it completely differently (top-left, unboxed, in the
  // empty gate; a full-width bottom bar while editing).
}

void OmniboxOverlay::applyPalette(const Palette &palette) { setStyleSheet(palette.toQss()); }

void OmniboxOverlay::showEmpty() {
  emptyMode_ = true;
  input_->setProperty("emptyMode", true);
  input_->style()->unpolish(input_);
  input_->style()->polish(input_);
  input_->clear();
  layoutInput();
  show();
  raise();
  input_->setFocus();
}

void OmniboxOverlay::showEditing(const QString &currentUrl) {
  emptyMode_ = false;
  input_->setProperty("emptyMode", false);
  input_->style()->unpolish(input_);
  input_->style()->polish(input_);
  input_->setText(currentUrl);
  layoutInput();
  show();
  raise();
  input_->setFocus();
  // Standard browser Ctrl+L behavior: select the whole address, so typing
  // immediately replaces it. A freshly-shown/focused QLineEdit can reset
  // the selection once more on the next event loop turn, so re-assert it.
  input_->selectAll();
  QTimer::singleShot(0, input_, &QLineEdit::selectAll);
}

void OmniboxOverlay::hideOverlay() { hide(); }

void OmniboxOverlay::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  layoutInput();
}

void OmniboxOverlay::layoutInput() {
  const int h = input_->sizeHint().height();
  if (emptyMode_) {
    // Just a caret at the top-left -- a modest fixed width to type into,
    // not the full window.
    const int w = qMin(kEmptyInputWidth, qMax(0, width() - 2 * kMargin));
    input_->setGeometry(kMargin, kMargin, w, h);
  } else {
    const int w = qMax(0, width() - 2 * kMargin);
    input_->setGeometry(kMargin, qMax(kMargin, height() - kMargin - h), w, h);
  }
}

bool OmniboxOverlay::eventFilter(QObject *obj, QEvent *event) {
  if (obj != input_ || event->type() != QEvent::KeyPress) {
    return QWidget::eventFilter(obj, event);
  }
  auto *key = static_cast<QKeyEvent *>(event);

  switch (key->key()) {
    case Qt::Key_Escape:
      emit cancelled();
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      submit();
      return true;
    default:
      return QWidget::eventFilter(obj, event);
  }
}

void OmniboxOverlay::submit() {
  const QString url = HistoryStore::toUrl(input_->text());
  if (url.isEmpty()) return;
  history_->recordTyped(input_->text(), url);
  emit navigateRequested(url);
}

int OmniboxOverlay::preferredEditingHeight() const {
  return 2 * kMargin + input_->sizeHint().height();
}

}  // namespace shinto
