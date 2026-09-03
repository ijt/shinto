// Parses Omarchy's colors.toml into a small palette and renders it to a Qt
// stylesheet for the OmniboxOverlay. Replaces bash `toml_color`/`write_theme`
// and extension/theme.css.
#pragma once

#include <QString>

namespace shinto {

struct Palette {
  QString bg = QStringLiteral("#1a1b26");
  QString fg = QStringLiteral("#c0caf5");
  QString accent = QStringLiteral("#7aa2f7");
  QString muted = QStringLiteral("#565f89");
  QString card = QStringLiteral("#24283b");
  // Not read from colors.toml (the old theme.css hardcoded this too).
  QString font = QStringLiteral(
      "'JetBrainsMono Nerd Font','JetBrains Mono',ui-monospace,monospace");

  // A QSS stylesheet for OmniboxOverlay using these colors.
  QString toQss() const;
};

// Reads ~/.local/state/omarchy/current/theme/colors.toml (via
// shinto::colorsTomlPath()). Missing keys, or a missing file entirely, fall
// back to Palette's defaults above -- same behavior as the old bash
// `toml_color`.
Palette loadPalette();

}  // namespace shinto
