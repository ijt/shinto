"use strict";

const moving = new Set();
const NEWTAB = chrome.runtime.getURL("newtab.html");

function skipWindow(win) {
  return !win || win.type === "devtools" || win.incognito;
}

async function toOwnWindow(tab) {
  if (tab?.id == null || moving.has(tab.id)) return;
  moving.add(tab.id);
  try {
    await chrome.windows.create({
      tabId: tab.id,
      type: "popup",
      focused: true,
    });
  } catch (err) {
    console.warn("shinto: move tab failed", err);
  } finally {
    setTimeout(() => moving.delete(tab.id), 750);
  }
}

async function explodeWindow(windowId) {
  let win;
  try {
    win = await chrome.windows.get(windowId);
  } catch {
    return;
  }
  if (skipWindow(win) || win.type === "popup" || win.type === "app") return;

  const tabs = await chrome.tabs.query({ windowId });
  for (const tab of tabs) {
    await toOwnWindow(tab);
  }
}

async function onTabCreated(tab) {
  if (tab.id == null || moving.has(tab.id)) return;

  let win;
  try {
    win = await chrome.windows.get(tab.windowId);
  } catch {
    return;
  }
  if (skipWindow(win)) return;

  const tabs = await chrome.tabs.query({ windowId: tab.windowId });
  if (tabs.length > 1) {
    await toOwnWindow(tab);
    return;
  }
  if (win.type === "normal") {
    await toOwnWindow(tab);
  }
}

async function openNewPage() {
  await chrome.windows.create({
    url: NEWTAB,
    type: "popup",
    focused: true,
  });
}

chrome.tabs.onCreated.addListener((tab) => {
  onTabCreated(tab);
});

chrome.windows.onCreated.addListener((win) => {
  if (skipWindow(win) || win.type !== "normal") return;
  explodeWindow(win.id);
});

chrome.runtime.onInstalled.addListener(async () => {
  const wins = await chrome.windows.getAll({ populate: true });
  for (const win of wins) {
    if (skipWindow(win) || win.type !== "normal") continue;
    await explodeWindow(win.id);
  }
});

async function focusLocation(tabId) {
  if (tabId == null) return;
  try {
    await chrome.tabs.sendMessage(tabId, { type: "focus-location" }, { frameId: 0 });
  } catch (err) {
    console.warn("shinto: focus-location", err);
  }
}

chrome.commands.onCommand.addListener((command, tab) => {
  if (command === "new-page") openNewPage();
  if (command === "focus-location") focusLocation(tab?.id);
});

async function openUrl(url) {
  await chrome.windows.create({
    url: url || NEWTAB,
    type: "popup",
    focused: true,
  });
}

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  if (msg?.type === "new-page") {
    openNewPage().then(() => sendResponse({ ok: true }));
    return true;
  }
  if (msg?.type === "open") {
    openUrl(msg.url).then(() => sendResponse({ ok: true }));
    return true;
  }
  if (msg?.type === "focus-location") {
    focusLocation(sender.tab?.id).then(() => sendResponse({ ok: true }));
    return true;
  }
  return false;
});
