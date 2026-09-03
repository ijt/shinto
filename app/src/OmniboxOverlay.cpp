#include "OmniboxOverlay.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QSignalBlocker>

namespace shinto {

namespace {
constexpr int kMargin = 24;
constexpr int kEmptyInputWidth = 280;
constexpr int kMaxSuggestions = 7;
}  // namespace

OmniboxOverlay::OmniboxOverlay(HistoryStore *history, const PopularDomains *domains,
                                QWidget *parent)
    : QWidget(parent), history_(history), domains_(domains) {
  setObjectName(QStringLiteral("OmniboxOverlay"));
  // A plain QWidget doesn't paint its stylesheet background by default --
  // without this, the page underneath shows through everywhere except
  // exactly where a child widget sits.
  setAttribute(Qt::WA_StyledBackground, true);

  input_ = new QLineEdit(this);
  input_->setAttribute(Qt::WA_MacShowFocusRect, false);
  input_->setPlaceholderText(QStringLiteral("search or url"));
  input_->installEventFilter(this);
  connect(input_, &QLineEdit::textChanged, this, &OmniboxOverlay::updateSuggestions);

  list_ = new QListWidget(this);
  list_->setFocusPolicy(Qt::NoFocus);
  list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_->hide();

  progressBar_ = new QWidget(this);
  progressBar_->setObjectName(QStringLiteral("ProgressBar"));
  progressBar_->hide();
  // No QLayout -- position is managed by hand in layoutInput().

  // Loading a real page has some inherent latency (DNS, connect, TLS)
  // before Chromium reports the first loadProgress tick -- the shimmer is
  // immediate feedback for that gap, submitted at the exact moment Enter
  // is pressed, independent of the network. setProgress() takes over and
  // stops it once real progress data arrives.
  inputOpacity_ = new QGraphicsOpacityEffect(input_);
  inputOpacity_->setOpacity(1.0);
  input_->setGraphicsEffect(inputOpacity_);
  shimmer_ = new QPropertyAnimation(inputOpacity_, "opacity", this);
  shimmer_->setDuration(700);
  shimmer_->setStartValue(1.0);
  shimmer_->setKeyValueAt(0.5, 0.35);
  shimmer_->setEndValue(1.0);
  shimmer_->setLoopCount(-1);
}

void OmniboxOverlay::applyPalette(const Palette &palette) {
  setStyleSheet(palette.toQss());
  QPalette pal = input_->palette();
  pal.setColor(QPalette::PlaceholderText, QColor(palette.muted));
  input_->setPalette(pal);
}

void OmniboxOverlay::showGate(const QString &prefill) {
  input_->setText(prefill);
  clearSuggestions();
  progress_ = 0;
  progressBar_->hide();
  stopShimmer();
  layoutInput();
  show();
  raise();
  input_->setFocus();
  if (!prefill.isEmpty()) {
    input_->selectAll();
  }
}

void OmniboxOverlay::hideOverlay() {
  clearSuggestions();
  stopShimmer();
  hide();
}

void OmniboxOverlay::setProgress(int percent) {
  progress_ = qBound(0, percent, 100);
  // Real progress data has arrived -- the progress bar takes over as
  // feedback from here.
  if (progress_ > 0) stopShimmer();
  progressBar_->setVisible(progress_ > 0 && progress_ < 100 && isVisible());
  layoutProgressBar();
}

void OmniboxOverlay::startShimmer() {
  inputOpacity_->setOpacity(1.0);
  shimmer_->start();
}

void OmniboxOverlay::stopShimmer() {
  shimmer_->stop();
  inputOpacity_->setOpacity(1.0);
}

void OmniboxOverlay::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  layoutInput();
}

int OmniboxOverlay::suggestionListHeight() const {
  if (items_.isEmpty()) return 0;
  const int rowHeight = list_->sizeHintForRow(0);
  return rowHeight * items_.size() + 2 * list_->frameWidth();
}

void OmniboxOverlay::layoutInput() {
  // Just a caret at the top-left -- a modest fixed width to type into, not
  // the full window. Suggestions drop down below it.
  const int h = input_->sizeHint().height();
  const int w = qMin(kEmptyInputWidth, qMax(0, width() - 2 * kMargin));
  input_->setGeometry(kMargin, kMargin, w, h);
  list_->setGeometry(kMargin, kMargin + h + 4, w, suggestionListHeight());
  list_->setVisible(!items_.isEmpty());
  layoutProgressBar();
}

void OmniboxOverlay::layoutProgressBar() {
  constexpr int kBarHeight = 3;
  progressBar_->setGeometry(0, 0, width() * progress_ / 100, kBarHeight);
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
      moveSelection(1);
      return true;
    case Qt::Key_Backtab:  // Shift+Tab
      moveSelection(-1);
      return true;
    case Qt::Key_Escape:
      if (!items_.isEmpty()) {
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
  if (items_.isEmpty()) return;
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
  // setText() fires textChanged, which is wired to updateSuggestions() --
  // without blocking it here, applying a selection would immediately
  // re-query suggestions using the just-selected text as the new prefix,
  // regenerating (and likely shrinking) the very list being cycled
  // through on every Tab/Up/Down press.
  const QSignalBlocker blocker(input_);
  input_->setText(items_[row].label);
  input_->setCursorPosition(input_->text().length());
}

void OmniboxOverlay::submit() {
  const int row = list_->currentRow();
  const bool hasPick = row >= 0 && row < items_.size() && !list_->isHidden();
  const QString url = hasPick ? items_[row].url : HistoryStore::toUrl(input_->text());
  if (url.isEmpty()) return;
  history_->recordTyped(input_->text(), url);
  clearSuggestions();
  startShimmer();
  emit navigateRequested(url);
}

void OmniboxOverlay::updateSuggestions() {
  renderSuggestions(domains_->complete(input_->text(), kMaxSuggestions));
}

void OmniboxOverlay::renderSuggestions(const QVector<PopularDomains::Suggestion> &items) {
  items_ = items;
  list_->clear();
  for (const auto &item : items_) {
    list_->addItem(item.label);
  }
  layoutInput();
}

void OmniboxOverlay::clearSuggestions() {
  items_.clear();
  list_->clear();
  list_->hide();
}

}  // namespace shinto
