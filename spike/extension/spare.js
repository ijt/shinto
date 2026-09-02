"use strict";

function attachHost() {
  let port;
  try {
    port = chrome.runtime.connectNative("org.shinto.control");
  } catch (err) {
    document.title = "Shinto Spare: " + err;
    setTimeout(attachHost, 1000);
    return;
  }
  document.title = "Shinto Spare";
  port.onMessage.addListener((msg) => {
    chrome.runtime.sendMessage(msg);
  });
  port.onDisconnect.addListener(() => {
    const why = chrome.runtime.lastError && chrome.runtime.lastError.message;
    document.title = "Shinto Spare: " + (why || "host disconnected");
    setTimeout(attachHost, 500);
  });
}

attachHost();
