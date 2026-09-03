#include "OmniboxOverlay.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

namespace shinto {

namespace {

// The gate's backdrop art, softened into a dark haze: a cheap blur (shrink,
// then grow back with smooth interpolation) for mistiness, a dark tint
// composited only over the art's own pixels (CompositionMode_SourceAtop
// respects its existing alpha, so the transparent background stays
// transparent) for "dark", and reduced overall opacity for "subtle" --
// baked once into a plain pixmap rather than stacked QGraphicsEffects.
QPixmap mistyBackdrop(const QString &resourcePath) {
  const QPixmap src(resourcePath);
  if (src.isNull()) return src;

  const QPixmap tiny = src.scaled(src.size() / 10, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  const QPixmap blurred = tiny.scaled(src.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

  QPixmap result(blurred.size());
  result.fill(Qt::transparent);
  QPainter painter(&result);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.setOpacity(0.32);
  painter.drawPixmap(0, 0, blurred);
  painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
  painter.setOpacity(0.6);
  painter.fillRect(result.rect(), QColor(5, 6, 12));
  painter.end();
  return result;
}

}  // namespace

OmniboxOverlay::OmniboxOverlay(HistoryStore *history, QWidget *parent)
    : QWidget(parent), history_(history) {
  setObjectName(QStringLiteral("OmniboxOverlay"));
  // A plain QWidget doesn't paint its stylesheet background by default --
  // without this, the page underneath shows through everywhere except
  // exactly where a child widget sits, which is what produced the
  // overlapping/"bleeding through" look.
  setAttribute(Qt::WA_StyledBackground, true);

  fujiLabel_ = new QLabel(this);
  fujiLabel_->setPixmap(mistyBackdrop(QStringLiteral(":/shinto/fuji.png")));
  fujiLabel_->setScaledContents(false);
  fujiLabel_->setAlignment(Qt::AlignCenter);

  list_ = new QListWidget(this);
  list_->setFocusPolicy(Qt::NoFocus);
  // The list is always sized to fit its (<= 7) rows exactly -- see
  // preferredEditingHeight() -- so a scrollbar should never be needed.
  list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_->hide();

  input_ = new QLineEdit(this);
  input_->setAttribute(Qt::WA_MacShowFocusRect, false);
  input_->installEventFilter(this);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(8);
  layout->addWidget(fujiLabel_, /*stretch=*/1);
  layout->addWidget(list_);
  layout->addWidget(input_);

  connect(input_, &QLineEdit::textChanged, this, [this](const QString &text) {
    debounce_->start(text.trimmed().isEmpty() ? 0 : 80);
  });

  debounce_ = new QTimer(this);
  debounce_->setSingleShot(true);
  connect(debounce_, &QTimer::timeout, this, &OmniboxOverlay::requestCompletions);

  connect(history_, &HistoryStore::completionsReady, this, &OmniboxOverlay::onCompletions);
}

void OmniboxOverlay::applyPalette(const Palette &palette) { setStyleSheet(palette.toQss()); }

void OmniboxOverlay::showEmpty() {
  emptyMode_ = true;
  fujiLabel_->setVisible(true);
  input_->clear();
  clearSuggestions();
  show();
  raise();
  input_->setFocus();
  requestCompletions();
}

void OmniboxOverlay::showEditing(const QString &currentUrl) {
  emptyMode_ = false;
  fujiLabel_->setVisible(false);
  input_->setText(currentUrl);
  clearSuggestions();
  show();
  raise();
  input_->setFocus();
  // Standard browser Ctrl+L behavior: select the whole address, so typing
  // immediately replaces it. A freshly-shown/focused QLineEdit can reset
  // the selection once more on the next event loop turn (the same reason
  // the old gate's caret placement needed a requestAnimationFrame
  // double-apply), so re-assert it.
  input_->selectAll();
  QTimer::singleShot(0, input_, &QLineEdit::selectAll);
  requestCompletions();
}

void OmniboxOverlay::caretToEnd() { input_->setCursorPosition(input_->text().length()); }

void OmniboxOverlay::hideOverlay() {
  clearSuggestions();
  hide();
}

bool OmniboxOverlay::eventFilter(QObject *obj, QEvent *event) {
  if (obj != input_ || event->type() != QEvent::KeyPress) {
    return QWidget::eventFilter(obj, event);
  }
  auto *key = static_cast<QKeyEvent *>(event);

  switch (key->key()) {
    case Qt::Key_Down:
      moveSelection(1);
      return true;
    case Qt::Key_Up:
      moveSelection(-1);
      return true;
    case Qt::Key_Tab:
      if (!list_->isHidden() && !items_.isEmpty()) {
        if (list_->currentRow() < 0) list_->setCurrentRow(0);
        applySelectionToInput();
      }
      return true;
    case Qt::Key_Escape:
      if (!list_->isHidden() && !items_.isEmpty()) {
        clearSuggestions();
      } else {
        emit cancelled();
      }
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      submit();
      return true;
    default:
      return QWidget::eventFilter(obj, event);
  }
}

void OmniboxOverlay::moveSelection(int delta) {
  if (items_.isEmpty()) {
    requestCompletions();
    return;
  }
  int cur = list_->currentRow();
  if (cur < 0) {
    cur = delta > 0 ? 0 : items_.size() - 1;
  } else {
    cur = (cur + delta + items_.size()) % items_.size();
  }
  list_->setCurrentRow(cur);
  applySelectionToInput();
}

void OmniboxOverlay::applySelectionToInput() {
  const int row = list_->currentRow();
  if (row < 0 || row >= items_.size()) return;
  input_->setText(items_[row].label);
  caretToEnd();
}

void OmniboxOverlay::submit() {
  const int row = list_->currentRow();
  const bool hasPick = row >= 0 && row < items_.size() && !list_->isHidden();
  const QString url = hasPick ? items_[row].url : HistoryStore::toUrl(input_->text());
  if (url.isEmpty()) return;
  history_->recordTyped(input_->text(), url);
  clearSuggestions();
  emit navigateRequested(url);
}

void OmniboxOverlay::requestCompletions() {
  const int id = ++latestRequestId_;
  history_->requestCompletions(id, input_->text());
}

void OmniboxOverlay::onCompletions(int requestId, QVector<HistoryStore::Suggestion> results) {
  if (requestId != latestRequestId_) return;
  renderSuggestions(results);
}

void OmniboxOverlay::renderSuggestions(const QVector<HistoryStore::Suggestion> &items) {
  items_ = items;
  list_->clear();
  for (const auto &item : items_) {
    list_->addItem(item.label);
  }
  list_->setVisible(!items_.isEmpty());
  emit contentSizeChanged();
}

void OmniboxOverlay::clearSuggestions() {
  items_.clear();
  list_->clear();
  list_->hide();
  emit contentSizeChanged();
}

int OmniboxOverlay::preferredEditingHeight() const {
  const auto margins = layout()->contentsMargins();
  int h = margins.top() + margins.bottom() + input_->sizeHint().height();
  if (!list_->isHidden() && list_->count() > 0) {
    const int rowHeight = list_->sizeHintForRow(0);
    h += layout()->spacing() + rowHeight * list_->count() + 2 * list_->frameWidth();
  }
  return h;
}

}  // namespace shinto
