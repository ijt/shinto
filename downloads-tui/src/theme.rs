// Reads Omarchy's live colors.toml so this TUI matches the same palette
// Shinto's Qt side uses (see app/src/ThemeLoader.cpp's loadPalette(),
// which this mirrors) -- no toml crate: colors.toml is flat `key = "value"`
// lines (confirmed by ThemeLoader.cpp's own comment to that effect), so a
// hand-rolled line parser is enough and keeps dependencies to just
// ratatui/crossterm/rusqlite.

use std::collections::HashMap;
use std::env;
use std::fs;
use std::path::PathBuf;

use ratatui::style::Color;

pub struct Palette {
    pub bg: Color,
    pub fg: Color,
    pub accent: Color,
    pub muted: Color,
}

fn parse_hex(hex: &str) -> Option<Color> {
    let hex = hex.trim().trim_start_matches('#');
    if hex.len() != 6 {
        return None;
    }
    let r = u8::from_str_radix(&hex[0..2], 16).ok()?;
    let g = u8::from_str_radix(&hex[2..4], 16).ok()?;
    let b = u8::from_str_radix(&hex[4..6], 16).ok()?;
    Some(Color::Rgb(r, g, b))
}

fn parse_flat_toml(path: &std::path::Path) -> HashMap<String, String> {
    let mut out = HashMap::new();
    let Ok(contents) = fs::read_to_string(path) else {
        return out;
    };
    for line in contents.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') || line.starts_with('[') {
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        let key = key.trim().to_string();
        let mut value = value.trim().to_string();
        if value.starts_with('"') && value.ends_with('"') && value.len() >= 2 {
            value = value[1..value.len() - 1].to_string();
        }
        out.insert(key, value);
    }
    out
}

// Matches shinto::colorsTomlPath() in app/src/Shinto.h exactly.
fn colors_toml_path() -> PathBuf {
    let home = env::var("HOME").unwrap_or_else(|_| ".".to_string());
    PathBuf::from(home).join(".local/state/omarchy/current/theme/colors.toml")
}

// Same fallback hex defaults as Palette's struct defaults in
// app/src/ThemeLoader.h, for a missing file or a missing key.
pub fn load_palette() -> Palette {
    let toml = parse_flat_toml(&colors_toml_path());
    let pick = |key: &str, default: &str| -> Color {
        toml.get(key)
            .and_then(|v| parse_hex(v))
            .or_else(|| parse_hex(default))
            .unwrap_or(Color::White)
    };
    Palette {
        bg: pick("background", "#1a1b26"),
        fg: pick("bright_foreground", "#c0caf5"),
        accent: pick("accent", "#7aa2f7"),
        muted: pick("dark_foreground", "#565f89"),
    }
}
