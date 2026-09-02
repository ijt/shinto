# Shinto

A page viewer for Omarchy. One window is one document. Hyprland is the tab bar.

Zen reimplements workspaces, split view, and tabs inside the browser because most desktops are bad at those. Omarchy is not. Shinto is the shrine: a gate you walk through (the overlay omnibox) and then it is gone.

## Why it starts fast

Stock Chromium on `Super + Shift + Return` feels slow because closing the last window kills the process. The next launch is a cold start.

Shinto keeps a warm daemon (systemd user unit) plus a hidden spare window on a special workspace. `Super + Shift + Return` asks the already-running process for a new window. Warm opens measure about **150ms**, not seconds.

`Super + Shift + B` stays stock Chromium, so you still have an escape hatch.

## Install

Omarchy / Hyprland. Chromium is the renderer; Hyprland groups are the tabs.

```bash
git clone https://github.com/ijt/shinto.git
cd shinto
./shinto install
```

That will:

- symlink `~/.local/bin/shinto`
- enable `shinto.service` so Chromium is warm after login
- rebind `Super + Shift + Return` to Shinto and `Super + Shift + Y` to YouTube in Shinto
- tag Shinto windows like other Chromium browsers

`Super + Shift + B` and `xdg-open` stay on Chromium unless you opt in with `shinto default`.

```bash
./shinto uninstall   # profile is left in ~/.local/share/shinto
```

## Keys

Browser (inside a Shinto window):

| Key | Action |
|-----|--------|
| `Ctrl + T` | New page (new window) |
| `Ctrl + L` / `Ctrl + K` | Overlay omnibox |
| `↑` / `↓` / `Tab` | Complete from typed URLs, history, Google |
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

- Dedicated Chromium profile at `~/.local/share/shinto/profile` — your main Chromium logins are untouched.
- Overlay omnibox on the new-tab page (`Ctrl+L` on http(s) pages too).
- Empty `Super + Shift + Return` opens a loopback new-tab page so Chromium does not draw the custom tab bar (title + pencil). Hazy Fuji in the void, address line at the bottom.
- `Super + Shift + B` stays Omarchy's default-browser launcher (`omarchy-launch-browser` / XDG). `shinto default` points that at Shinto.
