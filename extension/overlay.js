"use strict";

function isShintoNewtab() {
  return (
    location.pathname.endsWith("/newtab.html") &&
    (location.hostname === "127.0.0.1" || location.hostname === "localhost")
  );
}

function isLocationShortcut(event) {
  if ((!event.ctrlKey && !event.metaKey) || event.altKey || event.repeat) return false;
  const key = event.key.toLowerCase();
  return event.code === "KeyL" || event.code === "KeyK" || key === "l" || key === "k";
}

function isNewPageShortcut(event) {
  if ((!event.ctrlKey && !event.metaKey) || event.altKey || event.repeat) return false;
  return event.code === "KeyT" || event.key.toLowerCase() === "t";
}

if (location.protocol !== "chrome-extension:" && window === window.top) {
  const onGate = isShintoNewtab();
  window.addEventListener(
    "keydown",
    (event) => {
      if (isLocationShortcut(event)) {
        event.preventDefault();
        event.stopImmediatePropagation();
        if (!onGate) chrome.runtime.sendMessage({ type: "new-page" });
        return;
      }
      if (isNewPageShortcut(event)) {
        event.preventDefault();
        event.stopImmediatePropagation();
        chrome.runtime.sendMessage({ type: "new-page" });
      }
    },
    true
  );
}
