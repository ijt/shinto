"use strict";

(async () => {
  const params = new URLSearchParams(location.search);
  const target = params.get("u") || chrome.runtime.getURL("newtab.html");
  try {
    await chrome.runtime.sendMessage({ type: "open", url: target });
  } catch (err) {
    console.warn("shinto launch failed", err);
  }
  // Do not window.close() here: Chromium treats popups opened from an --app
  // trampoline as app-owned and kills them with the trampoline.
})();
