"use strict";

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
  window.addEventListener(
    "keydown",
    (event) => {
      if (!isLocationShortcut(event) && !isNewPageShortcut(event)) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      chrome.runtime.sendMessage({ type: "new-page" });
    },
    true
  );
}
