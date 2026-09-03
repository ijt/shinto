# Draft: Omarchy Suggestions discussion

**Title:** Shinto: an Omarchy-native page browser (Install > Browser proposal)

**Category:** Suggestions — https://github.com/basecamp/omarchy/discussions/categories/suggestions

---

## Pitch

Omarchy already treats the compositor as the window manager people wish browsers were. Shinto takes that seriously: **one window is one page**, Hyprland groups are the tab bar, and the omnibox is a gate you walk through and then it disappears.

It is a small Qt6/QtWebEngine app ([ijt/shinto](https://github.com/ijt/shinto)), not another Chromium skin. Warm daemon opens are well under 150ms. It picks up Omarchy theme colors for the overlay.

## Proposal (v1 — optional, not a Chromium replacement)

Add Shinto beside Firefox/Zen under **Install > Browser**, and list it under **Setup > Defaults > Browser** once installed:

- `omarchy install browser shinto` → install AUR `shinto-git`, enable `shinto.service`
- `omarchy default browser shinto` → XDG default for links / Super+Shift+B
- Tag `shinto` in `default/hypr/apps/browser.lua` like other browsers

## Explicit non-goals for v1

- **Do not** remove Chromium from the base system
- **Do not** route Omarchy webapps through Shinto — `omarchy-launch-webapp` should keep using Chromium-family `--app=`
- No extension store / Chrome Web Store story

Shinto is a *page* browser. Chromium stays the webapp host and escape hatch.

## Why this belongs in Omarchy

Firefox and Zen are fine browsers that fight the desktop less than Chrome, but they still reinvent tabs and window chrome. Shinto's UI is intentionally empty so Hyprland can do what it already does well. That matches Omarchy's taste more closely than "install yet another full browser."

## Packaging status

- Repo: https://github.com/ijt/shinto
- `cmake --install` layout: binary, `.desktop`, icon, systemd user unit
- AUR recipe in-tree at `packaging/PKGBUILD` (`shinto-git`)

Happy to open a PR against `basecamp/omarchy` wiring the install/default/remove/menu surfaces the same way Zen does, once `shinto-git` is on the AUR (or with whatever package source you prefer).

## Demo

_(attach short GIF: empty gate → navigate → Hyprland group tabs showing page titles → YouTube fullscreen)_
