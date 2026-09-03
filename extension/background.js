"use strict";

fetch("http://127.0.0.1:18764/debug", {
  method: "POST",
  cache: "no-store",
  body: "sw-boot",
}).catch(() => {});

const moving = new Set();

function skipWindow(win) {
  return !win || win.type === "devtools" || win.incognito;
}

async function toOwnWindow(tab) {
  if (tab?.id == null || moving.has(tab.id)) return;
  moving.add(tab.id);
  try {
    if (tab.url) await openUrl(tab.url);
    await chrome.tabs.remove(tab.id);
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

function gateUrl(url) {
  if (typeof url !== "string") return "";
  if (url.startsWith("http://") || url.startsWith("https://")) return url;
  return "";
}

async function openNewPage() {
  openGateAt = Date.now();
  try {
    const r = await fetch("http://127.0.0.1:18764/open", {
      method: "POST",
      cache: "no-store",
      headers: { "Content-Type": "application/json" },
      body: "{}",
    });
    dbg("openNewPage status=" + r.status);
  } catch (err) {
    dbg("openNewPage fail " + err);
    console.warn("shinto: open", err);
  }
}

function editDest(url) {
  const href = gateUrl(url);
  return (
    LOCAL_NEWTAB +
    "?n=" +
    Date.now() +
    (href ? "#" + encodeURIComponent(href) : "")
  );
}

async function editHere(tabId, url) {
  const dest = editDest(url);
  if (tabId == null) return { dest };
  try {
    await chrome.scripting.executeScript({
      target: { tabId },
      world: "MAIN",
      func: (next) => {
        location.href = next;
      },
      args: [dest],
    });
  } catch (err) {
    console.warn("shinto: edit-here", err);
  }
  return { dest };
}

function isOurGate(url) {
  return (
    typeof url === "string" &&
    url.includes("/newtab.html") &&
    (url.includes("127.0.0.1") || url.includes("localhost"))
  );
}

function isSpare(url) {
  return typeof url === "string" && url.includes("spare.html");
}

function isChromiumNewTab(url) {
  if (typeof url !== "string" || !url) return false;
  // Never treat about:blank as NTP — our --app gates pass through blank while loading.
  if (url.startsWith("chrome://newtab") || url.startsWith("chrome://new-tab-page")) return true;
  // Ctrl+N in --app mode opens a window under the extension id (pencil bar).
  // Spare/gate are loopback now — no visible UI should be chrome-extension://.
  if (url.startsWith("chrome-extension://")) return true;
  return false;
}

const replacingWindows = new Set();
let openGateAt = 0;

async function reclaimNewTabWindow(winId) {
  if (replacingWindows.has(winId)) return;
  replacingWindows.add(winId);
  try {
    await chrome.windows.remove(winId);
  } catch {
    replacingWindows.delete(winId);
    return;
  }
  replacingWindows.delete(winId);
  // Content script may already have asked for a gate; avoid spawning two.
  if (Date.now() - openGateAt < 800) return;
  openNewPage();
}

function dbg(msg) {
  fetch("http://127.0.0.1:18764/debug", {
    method: "POST",
    cache: "no-store",
    body: String(msg),
  }).catch(() => {});
}

async function replaceChromiumNewTab(win) {
  dbg("onCreated type=" + win.type + " id=" + win.id);
  for (let i = 0; i < 30; i++) {
    let tabs;
    try {
      tabs = await chrome.tabs.query({ windowId: win.id });
    } catch (err) {
      dbg("query fail " + err);
      return;
    }
    const tab = tabs[0];
    if (!tab) {
      await new Promise((r) => setTimeout(r, 50));
      continue;
    }
    const url = tab.pendingUrl || tab.url || "";
    dbg("i=" + i + " type=" + win.type + " url=" + url);
    if (isSpare(url) || isOurGate(url)) return;
    if (/^https?:/i.test(url) && !url.includes("127.0.0.1") && !url.includes("localhost")) {
      return;
    }
    if (isChromiumNewTab(url)) {
      dbg("reclaim " + url);
      await reclaimNewTabWindow(win.id);
      return;
    }
    await new Promise((r) => setTimeout(r, 50));
  }
  dbg("gave up on window " + win.id);
}

chrome.tabs.onCreated.addListener((tab) => {
  onTabCreated(tab);
});

chrome.tabs.onUpdated.addListener((tabId, info, tab) => {
  if (!info.url) return;
  dbg("tabUpdated " + info.url);
  if (isSpare(info.url) || isOurGate(info.url)) return;
  if (!isChromiumNewTab(info.url)) return;
  reclaimNewTabWindow(tab.windowId);
});

chrome.windows.onCreated.addListener((win) => {
  if (skipWindow(win)) return;
  if (win.type === "normal") explodeWindow(win.id);
  // Chromium Ctrl+N opens a stock/extension NTP with the Shinto+pencil bar.
  replaceChromiumNewTab(win);
});

// windows.onCreated is unreliable for --app windows. Poll and kill pencil NTPs.
setInterval(() => {
  chrome.tabs.query({}).then((tabs) => {
    for (const tab of tabs) {
      const url = tab.url || tab.pendingUrl || "";
      if (!url || isSpare(url) || isOurGate(url)) continue;
      if (!isChromiumNewTab(url)) continue;
      dbg("poll reclaim " + url + " win=" + tab.windowId);
      reclaimNewTabWindow(tab.windowId);
    }
  });
}, 400);

chrome.runtime.onInstalled.addListener(async () => {
  const wins = await chrome.windows.getAll({ populate: true });
  for (const win of wins) {
    if (skipWindow(win) || win.type !== "normal") continue;
    await explodeWindow(win.id);
  }
});

function isAddressEntry(url) {
  return (
    typeof url === "string" &&
    url.includes("/newtab.html") &&
    (url.includes("127.0.0.1") || url.includes("localhost"))
  );
}

chrome.commands.onCommand.addListener(async (command, tab) => {
  if (command === "new-page") {
    openNewPage();
    return;
  }
  if (command === "focus-location") {
    let url = tab?.url || "";
    let tabId = tab?.id;
    if (!url || tabId == null) {
      const tabs = await chrome.tabs.query({ active: true, lastFocusedWindow: true });
      url = tabs[0]?.url || url;
      tabId = tabs[0]?.id ?? tabId;
    }
    if (isAddressEntry(url)) return;
    await editHere(tabId, url);
  }
});

async function openUrl(url) {
  const go = gateUrl(url);
  if (!go) {
    await openNewPage();
    return;
  }
  try {
    await fetch("http://127.0.0.1:18764/open", {
      method: "POST",
      cache: "no-store",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ app: go }),
    });
  } catch (err) {
    console.warn("shinto: open-app", err);
  }
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
  if (msg?.type === "edit-here") {
    editHere(sender.tab?.id, msg.url).then((res) => sendResponse(res));
    return true;
  }
  if (msg?.type === "open") {
    openUrl(msg.url).then(() => sendResponse({ ok: true }));
    return true;
  }
  if (msg?.type === "focus-location") {
    editHere(sender.tab?.id, msg.url || sender.tab?.url).then((res) => sendResponse(res));
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
