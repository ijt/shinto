// Best-effort desktop notifications via notify-send
// (org.freedesktop.Notifications). Shinto is normally launched from a
// Hyprland keybinding or systemd, with no terminal in sight, so this is
// often the only way a message reaches the user at all.
#pragma once

#include <QString>

namespace shinto {

// If notify-send isn't installed, or nothing implements the notification
// spec, this just silently does nothing -- callers that also want a
// terminal-launched run to see the message should qWarning() it too.
void notify(const QString &title, const QString &body, bool critical = false);

}  // namespace shinto
