// Best-effort "show this file in the user's file manager, selected."
#pragma once

#include <QString>

namespace shinto {

// org.freedesktop.FileManager1.ShowItems (implemented by most file
// managers -- Nautilus, Dolphin, Nemo, etc.) opens the containing folder
// with `path` highlighted, not just opened; falls back to a plain
// (unselected) folder-open via xdg-open if nothing answers that D-Bus
// call (no file manager running one, or the file doesn't exist yet --
// e.g. clicked while still downloading, before Chromium's rename-on-
// completion).
void revealInFileManager(const QString &path);

}  // namespace shinto
