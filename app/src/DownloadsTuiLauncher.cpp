#include "DownloadsTuiLauncher.h"

#include <QProcess>

namespace shinto {

void launchOrSkipDownloadsTui() {
  // A near-instant, blocking check -- pgrep exits 0 if a matching process
  // exists, 1 otherwise. -f (match the full command line), not -x/-x
  // against the bare process name: "shinto-downloads" is 16 characters,
  // one over Linux's 15-character comm-name limit (confirmed: pgrep -x
  // itself warns about this and never matches), so an exact-name match
  // silently never fires -- the duplicate-panes bug this exists to fix
  // would otherwise still happen, just as reliably as before.
  //
  // This only prevents the duplicate; it doesn't raise/focus the existing
  // window. Looked into that (this system's Hyprland, 0.56.2, has moved
  // its dispatch API to a Lua surface -- hl.dsp.window.* -- whose window
  // dispatchers are all Z-order/state controls like bring_to_top(), which
  // does not grab input focus; no plain "focus by title/class" primitive
  // was found among them). Skipping the duplicate launch is still a
  // complete fix for the actual complaint (redundant panes piling up),
  // just without the "also jump me to it" nicety.
  if (QProcess::execute(QStringLiteral("pgrep"),
                         {QStringLiteral("-f"), QStringLiteral("shinto-downloads")}) == 0) {
    return;
  }
  QProcess::startDetached(QStringLiteral("xdg-terminal-exec"),
                           {QStringLiteral("--"), QStringLiteral("shinto-downloads")});
}

}  // namespace shinto
