#include "OmniboxOverlay.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QToolButton>

namespace shinto {

namespace {
constexpr int kMargin = 24;
constexpr int kMaxSuggestions = 7;
// Leaves at least 3 slots for PopularDomains fallback suggestions even
// when history has plenty of matches -- history is more relevant when it
// has an answer, but a page you've visited once shouldn't crowd out every
// domain suggestion.
constexpr int kMaxHistorySuggestions = 4;
}  // namespace

OmniboxOverlay::OmniboxOverlay(HistoryStore *history, PopularDomains *domains,
                                const ShintoConfig *config, QWidget *parent)
    : QWidget(parent), history_(history), domains_(domains), config_(config) {
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
  currentPalette_ = palette;
  setStyleSheet(palette.toQss());
  QPalette pal = input_->palette();
  pal.setColor(QPalette::PlaceholderText, QColor(palette.muted));
  input_->setPalette(pal);
  updateSuggestionSelectionStyle();
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
  // Full window width (minus margins) to type into -- was capped at a
  // modest fixed width, which made a long URL or search query scroll
  // inside a narrow field instead of just being visible. Suggestions drop
  // down below it, matching this same width.
  const int h = input_->sizeHint().height();
  const int w = qMax(0, width() - 2 * kMargin);
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
  updateSuggestionSelectionStyle();
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
  const QString url =
      hasPick ? items_[row].url : HistoryStore::toUrl(input_->text(), config_->searchEngineUrl);
  if (url.isEmpty()) return;
  history_->recordTyped(input_->text(), url);
  clearSuggestions();
  startShimmer();
  emit navigateRequested(url);
}

namespace {
// "https://rubyonrails.org/" (a history visit, trailing slash and all) and
// "https://rubyonrails.org" (PopularDomains' bare-domain form) are the same
// site, but not the same string -- strip a bare trailing slash before
// comparing so the domain suggestion doesn't show up redundantly right
// next to the history one for the same root page.
QString dedupKey(const QString &url) {
  QString key = url;
  if (key.endsWith(QLatin1Char('/'))) key.chop(1);
  return key;
}
}  // namespace

void OmniboxOverlay::updateSuggestions() {
  const QString text = input_->text();
  QVector<Suggestion> combined;
  QSet<QString> seen;
  // "https://rubyonrails.org" and "https://rubyonrails.org/" are two
  // distinct SQLite `visited` rows (url is the PRIMARY KEY) if the site
  // was ever visited both ways -- completeVisited() can legitimately
  // return both. Dedup as everything is assembled, not just between
  // history and PopularDomains, so two history rows for the same site
  // don't both survive either.
  auto tryAdd = [&](const QString &label, const QString &url, SuggestionKind kind) {
    // QSet::insert() (unlike std::set's) doesn't report whether the value
    // was already present, so check first.
    const QString key = dedupKey(url);
    if (seen.contains(key)) return;
    seen.insert(key);
    combined.push_back({label, url, kind});
  };
  for (const auto &h : history_->completeVisited(text, kMaxHistorySuggestions)) {
    tryAdd(h.label, h.url, SuggestionKind::History);
  }
  const int remaining = kMaxSuggestions - combined.size();
  if (remaining > 0) {
    for (const auto &d : domains_->complete(text, remaining)) {
      tryAdd(d.label, d.url, SuggestionKind::Popular);
    }
  }
  renderSuggestions(combined);
}

void OmniboxOverlay::renderSuggestions(const QVector<Suggestion> &items) {
  items_ = items;
  list_->clear();
  for (const auto &item : items_) {
    // Every row -- History or Popular -- gets a dismiss button, so every
    // row is a real child widget rather than plain QListWidgetItem text
    // (which can't host one).
    auto *listItem = new QListWidgetItem();
    list_->addItem(listItem);

    auto *row = new QWidget(list_);
    row->setObjectName(QStringLiteral("SuggestionRow"));
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(8, 0, 4, 0);
    rowLayout->setSpacing(4);

    auto *label = new QLabel(item.label, row);
    label->setObjectName(QStringLiteral("SuggestionLabel"));
    rowLayout->addWidget(label, 1);

    auto *dismiss = new QToolButton(row);
    dismiss->setObjectName(QStringLiteral("SuggestionDismiss"));
    dismiss->setText(QString::fromUtf8("\xC3\x97"));  // "×"
    dismiss->setAutoRaise(true);
    dismiss->setCursor(Qt::PointingHandCursor);
    dismiss->setFocusPolicy(Qt::NoFocus);
    const bool isHistory = item.kind == SuggestionKind::History;
    dismiss->setToolTip(isHistory ? QStringLiteral("Forget this from history")
                                   : QStringLiteral("Hide this suggestion"));
    rowLayout->addWidget(dismiss);

    const QString url = item.url;
    connect(dismiss, &QToolButton::clicked, this, [this, url, isHistory] {
      if (isHistory) {
        dismissHistorySuggestion(url);
      } else {
        dismissPopularSuggestion(url);
      }
    });

    list_->setItemWidget(listItem, row);
    listItem->setSizeHint(row->sizeHint());
  }
  updateSuggestionSelectionStyle();
  layoutInput();
}

void OmniboxOverlay::dismissHistorySuggestion(const QString &url) {
  history_->forgetVisited(url);
  updateSuggestions();
}

void OmniboxOverlay::dismissPopularSuggestion(const QString &url) {
  domains_->dismiss(url);
  updateSuggestions();
}

void OmniboxOverlay::updateSuggestionSelectionStyle() {
  const int cur = list_->currentRow();
  for (int i = 0; i < items_.size(); ++i) {
    auto *row = list_->itemWidget(list_->item(i));
    if (!row) continue;
    auto *label = row->findChild<QLabel *>();
    if (!label) continue;
    // Matches card (dark) on the accent-colored selected background --
    // the same pairing QListWidget::item:selected uses for plain items.
    label->setStyleSheet(i == cur ? QStringLiteral("color: %1;").arg(currentPalette_.card)
                                   : QString());
  }
}

void OmniboxOverlay::clearSuggestions() {
  items_.clear();
  list_->clear();
  list_->hide();
}

}  // namespace shinto
