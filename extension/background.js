"use strict";

const moving = new Set();

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

const LOCAL_NEWTAB = "http://127.0.0.1:18764/newtab.html";

async function openNewPage() {
  await chrome.windows.create({
    url: `${LOCAL_NEWTAB}?n=${Date.now()}`,
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

chrome.commands.onCommand.addListener((command) => {
  if (command === "new-page" || command === "focus-location") openNewPage();
});

async function openUrl(url) {
  await chrome.windows.create({
    url: url || LOCAL_NEWTAB,
    type: "popup",
    focused: true,
  });
}

const TYPED_MAX = 300;
const COMPLETE_SHOW = 7;

function prettyUrl(url) {
  try {
    const u = new URL(url);
    const host = u.hostname.replace(/^www\./, "");
    const path = `${u.pathname}${u.search}${u.hash}`;
    if (!path || path === "/") return host;
    return host + path;
  } catch {
    return url;
  }
}

function isInternalUrl(url) {
  if (!url) return true;
  if (url.startsWith("chrome://") || url.startsWith("chrome-extension://")) return true;
  if (url.includes("127.0.0.1") && url.includes("newtab.html")) return true;
  if (url.includes("localhost") && url.includes("newtab.html")) return true;
  return false;
}

function completeToUrl(raw) {
  const q = (raw || "").trim();
  if (!q) return null;
  if (/^[a-z][a-z0-9+.-]*:/i.test(q)) return q;
  if (q.startsWith("//")) return "https:" + q;
  if (/^localhost(:\d+)?(\/|$)/i.test(q)) return "http://" + q;
  if (/^(\d{1,3}\.){3}\d{1,3}(:\d+)?(\/|$)/.test(q)) return "http://" + q;
  if (/^\S+\.\S+$/.test(q) && !/\s/.test(q)) return "https://" + q;
  return "https://duckduckgo.com/?q=" + encodeURIComponent(q);
}

async function recordTyped(q, url) {
  if (!url || isInternalUrl(url)) return;
  const { typed = [] } = await chrome.storage.local.get("typed");
  const prev = typed.find((x) => x.url === url);
  const next = typed.filter((x) => x.url !== url);
  next.unshift({
    url,
    q: (q || "").trim() || prettyUrl(url),
    t: Date.now(),
    n: (prev?.n || 0) + 1,
  });
  await chrome.storage.local.set({ typed: next.slice(0, TYPED_MAX) });
}

function typedMatches(item, qn) {
  if (!qn) return true;
  const url = (item.url || "").toLowerCase();
  const typed = (item.q || "").toLowerCase();
  const pretty = prettyUrl(item.url).toLowerCase();
  return typed.includes(qn) || url.includes(qn) || pretty.includes(qn);
}

async function googleSuggest(q) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 700);
  try {
    const res = await fetch(
      "https://suggestqueries.google.com/complete/search?client=firefox&q=" +
        encodeURIComponent(q),
      { signal: ctrl.signal }
    );
    const data = await res.json();
    return Array.isArray(data?.[1]) ? data[1].map(String) : [];
  } catch {
    return [];
  } finally {
    clearTimeout(timer);
  }
}

async function completeQuery(q) {
  const query = (q || "").trim();
  const qn = query.toLowerCase();
  const out = [];
  const seen = new Set();

  function add(item) {
    if (!item?.url || isInternalUrl(item.url) || seen.has(item.url)) return;
    seen.add(item.url);
    out.push(item);
  }

  try {
    const { typed = [] } = await chrome.storage.local.get("typed");
    for (const item of typed) {
      if (out.length >= COMPLETE_SHOW) break;
      if (!typedMatches(item, qn)) continue;
      add({
        url: item.url,
        label: item.q || prettyUrl(item.url),
        kind: "typed",
      });
    }
  } catch {
    /* ignore */
  }

  try {
    const hist = await chrome.history.search({
      text: query,
      maxResults: 24,
      startTime: Date.now() - 1000 * 60 * 60 * 24 * 180,
    });
    hist.sort((a, b) => (b.visitCount || 0) - (a.visitCount || 0));
    for (const hit of hist) {
      if (out.length >= COMPLETE_SHOW) break;
      add({
        url: hit.url,
        label: prettyUrl(hit.url),
        kind: "history",
      });
    }
  } catch {
    /* ignore */
  }

  if (qn.length >= 2 && out.length < COMPLETE_SHOW) {
    const suggestions = await googleSuggest(query);
    for (const phrase of suggestions) {
      if (out.length >= COMPLETE_SHOW) break;
      const url = completeToUrl(phrase);
      if (!url) continue;
      add({
        url,
        label: phrase,
        kind: "search",
      });
    }
  }

  return out.slice(0, COMPLETE_SHOW);
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
    openNewPage().then(() => sendResponse({ ok: true }));
    return true;
  }
  if (msg?.type === "typed") {
    recordTyped(msg.q, msg.url).then(() => sendResponse({ ok: true }));
    return true;
  }
  if (msg?.type === "complete") {
    completeQuery(msg.q).then((hits) => sendResponse(hits));
    return true;
  }
  return false;
});
