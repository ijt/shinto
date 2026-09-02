"use strict";

function isShintoNewtab() {
  return (
    location.pathname.endsWith("/newtab.html") &&
    (location.hostname === "127.0.0.1" || location.hostname === "localhost")
  );
}

function isLocationShortcut(event) {
  if ((!event.ctrlKey && !event.metaKey) || event.altKey) return false;
  const key = event.key.toLowerCase();
  return event.code === "KeyL" || event.code === "KeyK" || key === "l" || key === "k";
}

function isNewPageShortcut(event) {
  if ((!event.ctrlKey && !event.metaKey) || event.altKey) return false;
  return event.code === "KeyT" || event.key.toLowerCase() === "t";
}

function toUrl(raw) {
  const q = raw.trim();
  if (!q) return null;
  if (/^[a-z][a-z0-9+.-]*:/i.test(q)) return q;
  if (q.startsWith("//")) return "https:" + q;
  if (/^localhost(:\d+)?(\/|$)/i.test(q)) return "http://" + q;
  if (/^(\d{1,3}\.){3}\d{1,3}(:\d+)?(\/|$)/.test(q)) return "http://" + q;
  if (/^\S+\.\S+$/.test(q) && !/\s/.test(q)) return "https://" + q;
  return "https://duckduckgo.com/?q=" + encodeURIComponent(q);
}

if (location.protocol === "chrome-extension:" || isShintoNewtab()) {
  /* Spare / launch / new-tab already own their chrome. */
} else if (window !== window.top) {
  window.addEventListener(
    "keydown",
    (event) => {
      if (!isLocationShortcut(event) && !isNewPageShortcut(event)) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      chrome.runtime.sendMessage({
        type: isNewPageShortcut(event) ? "new-page" : "focus-location",
      });
    },
    true
  );
} else {
  boot();
}

function boot() {
  const cssReady = Promise.all([
    fetch(chrome.runtime.getURL("theme.css")).then((r) => r.text()),
    fetch(chrome.runtime.getURL("gate.css")).then((r) => r.text()),
  ]).then(([theme, gate]) => `${theme}\n${gate}`);

  let host = null;
  let input = null;
  let open = false;
  let pending = false;

  async function mount() {
    if (host) return;
    const css = await cssReady;
    const root = document.documentElement;
    if (!root) return;
    host = document.createElement("div");
    host.setAttribute("data-shinto", "omnibox");
    const shadow = host.attachShadow({ mode: "closed" });
    shadow.innerHTML = `
      <style>
        ${css}
        #veil {
          position: fixed;
          inset: 0;
          z-index: 2147483647;
          display: none;
          flex-direction: column;
          justify-content: flex-end;
          background: linear-gradient(
            to top,
            var(--bg) 0%,
            color-mix(in srgb, var(--bg) 82%, transparent) 24%,
            transparent 52%
          );
          color: var(--fg);
          font-family: var(--font);
          color-scheme: dark;
        }
        #veil.open { display: flex; }
      </style>
      <div id="veil">
        <form class="gate" autocomplete="off" spellcheck="false">
          <input class="gate-input" type="text" autocapitalize="off" autocorrect="off" />
        </form>
      </div>
    `;
    input = shadow.querySelector(".gate-input");
    const veil = shadow.getElementById("veil");
    const form = shadow.querySelector("form");

    form.addEventListener("submit", (event) => {
      event.preventDefault();
      const url = toUrl(input.value);
      hide();
      if (url) location.href = url;
    });

    veil.addEventListener("mousedown", (event) => {
      if (event.target === veil) hide();
    });

    root.appendChild(host);
    host._veil = veil;
    if (pending) show();
  }

  function show() {
    if (!host || !input) {
      pending = true;
      mount();
      return;
    }
    pending = false;
    open = true;
    host._veil.classList.add("open");
    input.value = location.href;
    input.focus({ preventScroll: true });
    input.select();
    try {
      input.setSelectionRange(0, input.value.length);
    } catch {
      /* ignore */
    }
  }

  function hide() {
    if (!host) return;
    open = false;
    pending = false;
    host._veil.classList.remove("open");
    input.blur();
  }

  window.addEventListener(
    "keydown",
    (event) => {
      if (isNewPageShortcut(event)) {
        event.preventDefault();
        event.stopImmediatePropagation();
        chrome.runtime.sendMessage({ type: "new-page" });
        return;
      }
      if (isLocationShortcut(event)) {
        event.preventDefault();
        event.stopImmediatePropagation();
        show();
        return;
      }
      if (open && event.key === "Escape") {
        event.preventDefault();
        hide();
      }
    },
    true
  );

  chrome.runtime.onMessage.addListener((msg) => {
    if (msg?.type === "focus-location") show();
  });

  mount();
}
