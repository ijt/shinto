#include "FindBar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace shinto {

FindBar::FindBar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("FindBar"));
  setAttribute(Qt::WA_StyledBackground, true);
  hide();

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 4, 6, 4);
  layout->setSpacing(4);

  input_ = new QLineEdit(this);
  input_->setAttribute(Qt::WA_MacShowFocusRect, false);
  input_->installEventFilter(this);
  connect(input_, &QLineEdit::textChanged, this, &FindBar::searchChanged);
  layout->addWidget(input_, 1);

  countLabel_ = new QLabel(this);
  countLabel_->setObjectName(QStringLiteral("FindBarCount"));
  countLabel_->setMinimumWidth(48);
  countLabel_->setAlignment(Qt::AlignCenter);
  layout->addWidget(countLabel_);

  auto addButton = [this, layout](const QString &symbol, const QString &tooltip) {
    auto *btn = new QToolButton(this);
    btn->setObjectName(QStringLiteral("FindBarButton"));
    btn->setText(symbol);
    btn->setToolTip(tooltip);
    btn->setAutoRaise(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(btn);
    return btn;
  };
  prevBtn_ = addButton(QString::fromUtf8("\xE2\x86\x91"), QStringLiteral("Previous match"));  // ↑
  nextBtn_ = addButton(QString::fromUtf8("\xE2\x86\x93"), QStringLiteral("Next match"));      // ↓
  closeBtn_ = addButton(QString::fromUtf8("\xC3\x97"), QStringLiteral("Close"));              // ×

  connect(prevBtn_, &QToolButton::clicked, this, &FindBar::findPrevious);
  connect(nextBtn_, &QToolButton::clicked, this, &FindBar::findNext);
  connect(closeBtn_, &QToolButton::clicked, this, [this] {
    hideBar();
    emit closed();
  });

  setMatchCount(0, 0);
  adjustSize();
}

void FindBar::applyPalette(const Palette &palette) {
  setStyleSheet(QStringLiteral(
                    "#FindBar { background: %2; border: 1px solid %4; border-radius: 4px; }"
                    "#FindBar QLineEdit {"
                    " background: transparent; color: %3; border: none;"
                    " font-family: %5; font-size: 13px; }"
                    "#FindBar #FindBarCount { color: %4; font-family: %5; font-size: 12px; }"
                    "#FindBar #FindBarButton {"
                    " color: %4; background: transparent; border: none; font-family: %5; }"
                    "#FindBar #FindBarButton:hover { color: %3; }"
                    "#FindBar #FindBarButton:disabled { color: %4; }")
                    .arg(palette.bg, palette.card, palette.fg, palette.muted, palette.font));
}

void FindBar::showBar() {
  show();
  raise();
  input_->setFocus();
  input_->selectAll();
}

void FindBar::hideBar() { hide(); }

QString FindBar::searchText() const { return input_->text(); }

void FindBar::setMatchCount(int active, int total) {
  countLabel_->setText(total > 0 ? QStringLiteral("%1/%2").arg(active).arg(total)
                                  : QStringLiteral("0/0"));
  const bool hasMatches = total > 0;
  prevBtn_->setEnabled(hasMatches);
  nextBtn_->setEnabled(hasMatches);
}

bool FindBar::eventFilter(QObject *obj, QEvent *event) {
  if (obj != input_ || event->type() != QEvent::KeyPress) {
    return QWidget::eventFilter(obj, event);
  }
  auto *key = static_cast<QKeyEvent *>(event);
  switch (key->key()) {
    case Qt::Key_Escape:
      hideBar();
      emit closed();
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      if (key->modifiers() & Qt::ShiftModifier) {
        emit findPrevious();
      } else {
        emit findNext();
      }
      return true;
    default:
      return QWidget::eventFilter(obj, event);
  }
}

}  // namespace shinto
