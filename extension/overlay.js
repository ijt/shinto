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

const GATE_CSS = `
:host {
  --bg: #1a1b26;
  --fg: #c0caf5;
  --accent: #7aa2f7;
  --muted: #565f89;
  --font: "JetBrainsMono Nerd Font", "JetBrains Mono", ui-monospace, monospace;
}
.gate { width: 100%; }
.gate-row {
  display: flex;
  align-items: center;
  gap: 0.4em;
  width: calc(100% - 16vw);
  margin: 0 8vw 10vh;
  border-bottom: 1px solid color-mix(in srgb, var(--fg) 14%, transparent);
}
.gate-input {
  flex: 1;
  min-width: 0;
  margin: 0;
  border: 0;
  border-radius: 0;
  background: transparent;
  color: var(--fg);
  outline: none;
  appearance: none;
  font: 16px/1.8 var(--font);
  letter-spacing: 0.02em;
  font-weight: 400;
  padding: 8px 0;
  caret-color: color-mix(in srgb, var(--accent) 70%, var(--fg));
  caret-animation: manual;
}
.gate-mark {
  flex: none;
  width: 1.15em;
  height: 1.15em;
  margin-bottom: 0.12em;
  color: var(--accent);
  opacity: 0.42;
  pointer-events: none;
}
.gate-input::selection {
  background: color-mix(in srgb, var(--accent) 18%, transparent);
  color: var(--fg);
}
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
`;

function init() {
  if (location.protocol === "chrome-extension:" || isShintoNewtab()) return;

  if (window !== window.top) {
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
    return;
  }

  boot();
}

function boot() {
  let host = null;
  let input = null;
  let open = false;

  function mount() {
    if (host) return;
    const root = document.documentElement;
    if (!root) return;
    host = document.createElement("div");
    host.setAttribute("data-shinto", "omnibox");
    host.style.cssText = "position:absolute;width:0;height:0;overflow:visible;";
    const shadow = host.attachShadow({ mode: "closed" });
    shadow.innerHTML = `
      <style>${GATE_CSS}</style>
      <div id="veil">
        <form class="gate" autocomplete="off" spellcheck="false">
          <div class="gate-row">
            <svg class="gate-mark" viewBox="0 0 128 128" aria-hidden="true">
              <g fill="none" stroke="currentColor" stroke-width="10" stroke-linecap="round" stroke-linejoin="round">
                <path d="M18 34h92" />
                <path d="M28 50h72" />
                <path d="M40 34v62" />
                <path d="M88 34v62" />
              </g>
            </svg>
            <input class="gate-input" type="text" spellcheck="false" />
          </div>
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

    fetch(chrome.runtime.getURL("theme.css"))
      .then((r) => r.text())
      .then((css) => {
        const style = document.createElement("style");
        style.textContent = css;
        shadow.appendChild(style);
      })
      .catch(() => {});
  }

  function show() {
    mount();
    if (!host || !input) return;
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

  if (document.documentElement) mount();
  else document.addEventListener("DOMContentLoaded", mount, { once: true });
}

if (!globalThis.__shintoGate) {
  globalThis.__shintoGate = true;
  init();
}
