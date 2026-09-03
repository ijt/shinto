#include "OmniboxOverlay.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace shinto {

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

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->addStretch(/*stretch=*/1);
  layout->addWidget(input_);
}

void OmniboxOverlay::applyPalette(const Palette &palette) { setStyleSheet(palette.toQss()); }

void OmniboxOverlay::showEmpty() {
  emptyMode_ = true;
  input_->clear();
  show();
  raise();
  input_->setFocus();
}

void OmniboxOverlay::showEditing(const QString &currentUrl) {
  emptyMode_ = false;
  input_->setText(currentUrl);
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
  const auto margins = layout()->contentsMargins();
  return margins.top() + margins.bottom() + input_->sizeHint().height();
}

}  // namespace shinto
