#include "ThemeLoader.h"

#include <QFile>
#include <QHash>
#include <QTextStream>

#include "Shinto.h"

namespace shinto {

namespace {

// colors.toml is flat `key = "value"` lines (confirmed against a live
// Omarchy theme file) -- no need for a real TOML parser.
QHash<QString, QString> parseFlatToml(const QString &path) {
  QHash<QString, QString> out;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return out;
  }
  QTextStream stream(&file);
  while (!stream.atEnd()) {
    QString line = stream.readLine().trimmed();
    if (line.isEmpty() || line.startsWith('#') || line.startsWith('[')) {
      continue;
    }
    const int eq = line.indexOf('=');
    if (eq < 0) {
      continue;
    }
    QString key = line.left(eq).trimmed();
    QString value = line.mid(eq + 1).trimmed();
    if (value.startsWith('"') && value.endsWith('"') && value.size() >= 2) {
      value = value.mid(1, value.size() - 2);
    }
    out.insert(key, value);
  }
  return out;
}

QString pick(const QHash<QString, QString> &toml, const char *key, const QString &fallback) {
  const auto it = toml.constFind(QString::fromLatin1(key));
  if (it == toml.constEnd() || it.value().isEmpty()) {
    return fallback;
  }
  return it.value();
}

}  // namespace

Palette loadPalette() {
  Palette palette;
  const QHash<QString, QString> toml = parseFlatToml(colorsTomlPath());
  palette.bg = pick(toml, "background", palette.bg);
  palette.fg = pick(toml, "bright_foreground", palette.fg);
  palette.accent = pick(toml, "accent", palette.accent);
  palette.muted = pick(toml, "dark_foreground", palette.muted);
  palette.card = pick(toml, "lighter_background", palette.card);
  return palette;
}

QString Palette::toQss() const {
  return QStringLiteral(
             "#OmniboxOverlay { background: %1; }\n"
             // No visible field at rest -- just the caret. Same style
             // whether it's a fresh window's gate or Ctrl+L on a loaded
             // page (both show this same gate now).
             "#OmniboxOverlay QLineEdit {"
             " background: transparent; color: #ffffff; border: none;"
             " padding: 0; font-family: %5; font-size: 15px;"
             " selection-background-color: %6; }\n"
             "#OmniboxOverlay QListWidget {"
             " background: %2; color: %3; border: 1px solid %4;"
             " font-family: %5; outline: none; }\n"
             "#OmniboxOverlay QListWidget::item:selected { background: %6; color: %2; }\n"
             "#OmniboxOverlay #ProgressBar { background: %6; border: none; }\n")
      .arg(bg, card, fg, muted, font, accent);
}

}  // namespace shinto
