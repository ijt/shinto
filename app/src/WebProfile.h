// Builds the single QWebEngineProfile shared by every BrowserWindow, so
// cookies/localStorage/session are shared across windows the same way one
// Chromium --user-data-dir was shared across --app windows before.
#pragma once

class QObject;
class QWebEngineProfile;

namespace shinto {

// Ownership: the returned profile is a child of `parent` (typically the
// QApplication or another long-lived owner). Do not call this more than once
// per process — QtWebEngine expects one persistent profile instance.
QWebEngineProfile *createSharedProfile(QObject *parent);

}  // namespace shinto
