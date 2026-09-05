// Reads downloads.sqlite -- the schema DownloadManager::open() creates in
// app/src/DownloadManager.cpp. Read-only: this is a separate, short-lived
// process from the Shinto daemon that's actually writing to this file, so
// there's no attempt to fix up stale rows (that's the daemon's own
// restart-recovery job, done on its own open()).

use std::path::Path;

use rusqlite::{Connection, Result};

/// Mirrors DownloadManager::State's declaration order in
/// app/src/DownloadManager.h exactly -- see the comment on that enum for
/// why these ordinals must stay in sync by hand.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum State {
    InProgress,
    Completed,
    Interrupted,
    Cancelled,
}

impl State {
    fn from_i64(v: i64) -> State {
        match v {
            0 => State::InProgress,
            1 => State::Completed,
            2 => State::Interrupted,
            _ => State::Cancelled,
        }
    }
}

pub struct Download {
    pub filename: String,
    pub path: String,
    pub total_bytes: i64,
    pub received_bytes: i64,
    pub state: State,
}

pub fn load_all(path: &Path) -> Result<Vec<Download>> {
    if !path.exists() {
        return Ok(Vec::new());
    }
    let conn = Connection::open(path)?;
    let mut stmt = conn.prepare(
        "SELECT filename, path, total_bytes, received_bytes, state \
         FROM downloads ORDER BY started_at DESC",
    )?;
    let rows = stmt.query_map([], |row| {
        Ok(Download {
            filename: row.get(0)?,
            path: row.get(1)?,
            total_bytes: row.get(2)?,
            received_bytes: row.get(3)?,
            state: State::from_i64(row.get(4)?),
        })
    })?;
    rows.collect()
}
