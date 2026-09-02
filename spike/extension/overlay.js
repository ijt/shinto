"use strict";

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

let host = null;
let input = null;
let open = false;

function mount() {
  if (host) return;
  host = document.createElement("div");
  host.setAttribute("data-shinto", "omnibox");
  const shadow = host.attachShadow({ mode: "closed" });
  shadow.innerHTML = `
    <style>
      :host { all: initial; }
      #veil {
        position: fixed;
        inset: 0;
        z-index: 2147483647;
        display: none;
        place-items: center;
        background: rgba(0, 0, 0, 0.28);
        font-family: "JetBrainsMono Nerd Font", "JetBrains Mono", ui-monospace, monospace;
      }
      #veil.open { display: grid; }
      form { width: min(640px, calc(100vw - 48px)); }
      input {
        width: 100%;
        box-sizing: border-box;
        border: 1px solid #7aa2f7;
        background: #1a1b26;
        color: #c0caf5;
        font: 16px/1.4 inherit;
        padding: 14px 18px;
        border-radius: 14px;
        outline: none;
      }
    </style>
    <div id="veil">
      <form>
        <input type="text" spellcheck="false" autocomplete="off" />
      </form>
    </div>
  `;
  input = shadow.querySelector("input");
  const veil = shadow.getElementById("veil");
  const form = shadow.querySelector("form");

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    const url = toUrl(input.value);
    hide();
    if (url) location.href = url;
  });

  veil.addEventListener("click", (event) => {
    if (event.target === veil) hide();
  });

  document.documentElement.appendChild(host);
  host._veil = veil;
}

function show() {
  mount();
  open = true;
  host._veil.classList.add("open");
  input.value = location.href;
  input.focus();
  input.select();
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
    const key = event.key.toLowerCase();
    if (event.ctrlKey && !event.altKey && !event.metaKey && key === "t") {
      event.preventDefault();
      event.stopImmediatePropagation();
      chrome.runtime.sendMessage({ type: "new-page" });
      return;
    }
    if (event.ctrlKey && !event.altKey && !event.metaKey && (key === "l" || key === "k")) {
      event.preventDefault();
      event.stopImmediatePropagation();
      if (open) hide();
      else show();
      return;
    }
    if (open && event.key === "Escape") {
      event.preventDefault();
      hide();
    }
  },
  true
);
