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

const form = document.getElementById("go");
const input = document.getElementById("q");

form.addEventListener("submit", (event) => {
  event.preventDefault();
  const url = toUrl(input.value);
  if (url) location.href = url;
});

document.addEventListener("mousedown", () => input.focus());

window.addEventListener(
  "keydown",
  (event) => {
    if (event.altKey || event.metaKey) return;
    const key = event.key.toLowerCase();
    if (event.ctrlKey && (event.code === "KeyT" || key === "t")) {
      event.preventDefault();
      event.stopImmediatePropagation();
      if (typeof chrome !== "undefined" && chrome.runtime) {
        chrome.runtime.sendMessage({ type: "new-page" });
      }
      return;
    }
    if (event.ctrlKey && (event.code === "KeyL" || event.code === "KeyK" || key === "l" || key === "k")) {
      event.preventDefault();
      event.stopImmediatePropagation();
      input.focus();
      input.select();
    }
  },
  true
);

window.addEventListener("keydown", (event) => {
  if (event.key !== "Escape" || event.defaultPrevented) return;
  if (history.length > 1) {
    event.preventDefault();
    history.back();
  }
});

input.focus();
