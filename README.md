# Shinto

A page viewer for Omarchy. One window is one document. Hyprland is the tab bar.

Zen reimplements workspaces, split view, and tabs inside the browser because most desktops are bad at those. Omarchy is not. Shinto is the shrine: a gate you walk through (the overlay omnibox) and then it is gone.

A small native C++/Qt6 app embedding QtWebEngine (`app/`) — not a Chrome extension driving real Chromium. There is no tab strip and no New-Tab-Page to fight with: the omnibox is a widget Shinto draws itself, never a page Chromium could show its own UI on top of.

## Why it starts fast

The app itself is the warm daemon — no separate hidden window needed to keep it alive. `shinto.service` runs it with zero windows open; `Super + Shift + Return` asks the already-running process for a new window over a local socket. Warm opens measure well under 150ms, not seconds.

`Super + Shift + B` stays stock Chromium, so you still have an escape hatch.

## Install

Omarchy / Hyprland.

Build the app once:

```bash
cmake -S app -B app/build
cmake --build app/build
```

Then:

```bash
git clone https://github.com/ijt/shinto.git
cd shinto
./shinto install
```

That will:

- symlink `~/.local/bin/shinto`
- enable `shinto.service` so the daemon is warm after login
- rebind `Super + Shift + Return` to Shinto and `Super + Shift + Y` to YouTube in Shinto
- tag Shinto windows like other Chromium-family browsers

`Super + Shift + B` and `xdg-open` stay on Chromium unless you opt in with `shinto default`.

```bash
./shinto uninstall   # data is left in ~/.local/share/shinto
```

## Keys

Browser (inside a Shinto window):

| Key | Action |
|-----|--------|
| `Ctrl + T` / `Ctrl + N` | New empty page (new window) |
| `Ctrl + L` / `Ctrl + K` | Edit this window's address (whole address selected, so typing replaces it). Escape goes back. |
| `Ctrl + W` / `Super + Q` | Close this page |

Hyprland groups (these are the tabs):

| Key | Action |
|-----|--------|
| `Super + G` | Toggle group on the focused window. New windows join an unlocked group. |
| `Super + Ctrl + Left/Right` | Cycle pages in the group |
| `Super + Alt + 1/2/3/4` | Jump to grouped window N |
| `Super + Alt + G` | Pull this window out of the group |
| `Super + G` again | Disband the group |

## Notes

- Dedicated QtWebEngine profile at `~/.local/share/shinto/profile/webengine` — your main Chromium logins are untouched.
- `Ctrl+N` / `Ctrl+T` open a new empty window. `Ctrl+L` edits the address in this window, whole address selected. Escape goes back. On the empty gate, Ctrl+L is a no-op.
- `Super + Shift + B` stays Omarchy's default-browser launcher (`omarchy-launch-browser` / XDG). `shinto default` points that at Shinto.
