#include "BrowserWindow.h"

#include <QKeyEvent>
#include <QResizeEvent>
#include <QShortcut>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

#include "OmniboxOverlay.h"

namespace shinto {

namespace {

bool isShintoShortcut(const QKeyEvent *ke) {
  if (ke->modifiers() != Qt::ControlModifier) return false;
  switch (ke->key()) {
    case Qt::Key_T:
    case Qt::Key_N:
    case Qt::Key_L:
    case Qt::Key_K:
    case Qt::Key_W:
      return true;
    default:
      return false;
  }
}

// QWebEnginePage's default javaScriptConsoleMessage() prints every page's
// own console.log/warn/error output to stderr -- fine for web development,
// but Shinto is an app-mode browser, not a devtools console, and real
// sites (YouTube included) constantly emit their own warnings that have
// nothing to do with Shinto. Swallow it.
class WebPage : public QWebEnginePage {
 public:
  using QWebEnginePage::QWebEnginePage;

 protected:
  void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel /*level*/,
                                 const QString & /*message*/, int /*lineNumber*/,
                                 const QString & /*sourceID*/) override {}
};

}  // namespace

// A QWebEngineView by default can "eat" key events that also match one of
// our QShortcuts (Chromium's own input handling marks ShortcutOverride
// events accepted for many keys, which tells Qt not to fire the shortcut).
// Intercepting ShortcutOverride here, before it reaches the base class,
// guarantees Ctrl+T/N/L/K/W always reach BrowserWindow's shortcuts even
// when a page has focus -- there is no JS-level race to lose, unlike the
// old content-script approach.
class WebView : public QWebEngineView {
 public:
  explicit WebView(QWebEngineProfile *profile, QWidget *parent = nullptr)
      : QWebEngineView(parent) {
    setPage(new WebPage(profile, this));
  }

 protected:
  bool event(QEvent *e) override {
    if (e->type() == QEvent::ShortcutOverride) {
      auto *ke = static_cast<QKeyEvent *>(e);
      if (isShintoShortcut(ke)) {
        e->ignore();
        return true;
      }
    }
    return QWebEngineView::event(e);
  }
};

QVector<BrowserWindow *> BrowserWindow::instances_;
Palette BrowserWindow::currentPalette_;

BrowserWindow *BrowserWindow::spawn(QWebEngineProfile *profile, HistoryStore *history,
                                     const PopularDomains *domains, const QString &url) {
  auto *win = new BrowserWindow(profile, history, domains, url);
  win->setAttribute(Qt::WA_DeleteOnClose);
  instances_.push_back(win);
  win->resize(1200, 800);
  win->show();
  return win;
}

void BrowserWindow::applyPaletteToAll(const Palette &palette) {
  currentPalette_ = palette;
  for (auto *w : instances_) {
    w->overlay_->applyPalette(palette);
  }
}

BrowserWindow::BrowserWindow(QWebEngineProfile *profile, HistoryStore *history,
                              const PopularDomains *domains, const QString &url)
    : history_(history), domains_(domains) {
  setWindowTitle(QStringLiteral("Shinto"));

  auto *container = new QWidget(this);
  setCentralWidget(container);

  webView_ = new WebView(profile, container);

  overlay_ = new OmniboxOverlay(history_, domains_, container);
  overlay_->applyPalette(currentPalette_);

  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(webView_);
  // overlay_ is deliberately not added to this layout -- BrowserWindow
  // positions it directly on top of webView_ in relayout().

  connect(overlay_, &OmniboxOverlay::navigateRequested, this, &BrowserWindow::onOverlayNavigate);
  connect(overlay_, &OmniboxOverlay::cancelled, this, &BrowserWindow::onOverlayCancelled);
  // The suggestion list grows/shrinks as you type; keep the editing bar
  // sized to fit it instead of leaving it crushed into a fixed height.
  connect(overlay_, &OmniboxOverlay::contentSizeChanged, this, &BrowserWindow::relayout);

  connect(webView_->page(), &QWebEnginePage::urlChanged, this, [this](const QUrl &navUrl) {
    history_->recordVisit(navUrl.toString(), webView_->page()->title());
  });
  connect(webView_->page(), &QWebEnginePage::titleChanged, this, [this](const QString &title) {
    history_->recordVisit(webView_->url().toString(), title);
  });
  // The QtWebEngine equivalent of Chromium's "exploded" multi-tab windows:
  // target=_blank / window.open() / ctrl-click all route through this one
  // signal. Redirecting to our own new window (and never calling
  // request.openIn()) means the requesting page never gets a popup/tab of
  // its own -- "one window, one page" holds structurally, not by racing to
  // detect and close a stray window after the fact.
  connect(webView_->page(), &QWebEnginePage::newWindowRequested, this,
          [this](QWebEngineNewWindowRequest &request) {
            BrowserWindow::spawn(webView_->page()->profile(), history_, domains_,
                                  request.requestedUrl().toString());
          });

  auto addShortcut = [this](const QKeySequence &seq, auto slot) {
    auto *sc = new QShortcut(seq, this);
    sc->setContext(Qt::WindowShortcut);
    connect(sc, &QShortcut::activated, this, slot);
  };
  addShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), &BrowserWindow::onNewPageShortcut);
  addShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), &BrowserWindow::onNewPageShortcut);
  addShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), &BrowserWindow::onEditAddressShortcut);
  addShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), &BrowserWindow::onEditAddressShortcut);
  addShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), [this] { close(); });

  if (url.isEmpty()) {
    enterEmpty();
  } else {
    state_ = State::Loaded;
    webView_->setUrl(QUrl(url));
    overlay_->hideOverlay();
  }
}

BrowserWindow::~BrowserWindow() { instances_.removeOne(this); }

void BrowserWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  relayout();
}

void BrowserWindow::relayout() {
  const QRect area = centralWidget()->rect();
  if (state_ == State::Empty) {
    overlay_->setGeometry(area);
  } else if (state_ == State::Editing) {
    const int barHeight = qMin(area.height(), overlay_->preferredEditingHeight());
    overlay_->setGeometry(0, area.height() - barHeight, area.width(), barHeight);
  }
}

void BrowserWindow::enterEmpty() {
  state_ = State::Empty;
  relayout();
  // A QWebEngineView that's never been navigated at all can make Qt
  // recreate the window's native surface once, shortly after this window
  // is first mapped -- visible as a startup close+reopen flicker (root-
  // caused via isolated testing: reproduced with a bare idle
  // QWebEngineView, gone once it was given something, anything, to load).
  // about:blank gives the compositor a real frame to commit while still
  // looking empty -- the omnibox fully covers it either way.
  webView_->setUrl(QUrl(QStringLiteral("about:blank")));
  overlay_->showEmpty();
}

void BrowserWindow::enterEditing() {
  state_ = State::Editing;
  relayout();
  overlay_->showEditing(webView_->url().toString());
}

void BrowserWindow::onOverlayNavigate(const QString &url) {
  webView_->setUrl(QUrl(url));
  state_ = State::Loaded;
  overlay_->hideOverlay();
  webView_->setFocus();
}

void BrowserWindow::onOverlayCancelled() {
  // A no-op on the empty gate: there is nothing loaded to go back to.
  if (state_ != State::Editing) return;
  state_ = State::Loaded;
  overlay_->hideOverlay();
  webView_->setFocus();
}

void BrowserWindow::onNewPageShortcut() {
  BrowserWindow::spawn(webView_->page()->profile(), history_, domains_, QString());
}

void BrowserWindow::onEditAddressShortcut() {
  if (state_ == State::Empty) return;  // documented no-op on the empty gate
  enterEditing();
}

}  // namespace shinto
