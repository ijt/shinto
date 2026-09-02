"use strict";

const LOCAL_NEWTAB = "http://127.0.0.1:18764/newtab.html";

function isShintoNewtab() {
  return (
    location.pathname.endsWith("/newtab.html") &&
    (location.hostname === "127.0.0.1" || location.hostname === "localhost")
  );
}

function isLocationShortcut(event) {
  if ((!event.ctrlKey && !event.metaKey) || event.altKey || event.shiftKey || event.repeat) {
    return false;
  }
  const key = event.key.toLowerCase();
  return event.code === "KeyL" || event.code === "KeyK" || key === "l" || key === "k";
}

function isNewWindowShortcut(event) {
  if ((!event.ctrlKey && !event.metaKey) || event.altKey || event.shiftKey || event.repeat) {
    return false;
  }
  const key = event.key.toLowerCase();
  return event.code === "KeyT" || event.code === "KeyN" || key === "t" || key === "n";
}

if (location.protocol !== "chrome-extension:" && window === window.top) {
  const onGate = isShintoNewtab();
  window.addEventListener(
    "keydown",
    (event) => {
      if (isLocationShortcut(event)) {
        event.preventDefault();
        event.stopImmediatePropagation();
        if (onGate) return;
        chrome.runtime.sendMessage({ type: "edit-here", url: location.href }, (res) => {
          if (chrome.runtime.lastError) return;
          if (!res || !res.dest) return;
          if (location.pathname.endsWith("/newtab.html")) return;
          location.href = res.dest;
        });
        return;
      }
      if (isNewWindowShortcut(event)) {
        event.preventDefault();
        event.stopImmediatePropagation();
        chrome.runtime.sendMessage({ type: "new-page" });
      }
    },
    true
  );
}
