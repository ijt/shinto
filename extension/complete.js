"use strict";

function shintoToUrl(raw) {
  const q = raw.trim();
  if (!q) return null;
  if (/^[a-z][a-z0-9+.-]*:/i.test(q)) return q;
  if (q.startsWith("//")) return "https:" + q;
  if (/^localhost(:\d+)?(\/|$)/i.test(q)) return "http://" + q;
  if (/^(\d{1,3}\.){3}\d{1,3}(:\d+)?(\/|$)/.test(q)) return "http://" + q;
  if (/^\S+\.\S+$/.test(q) && !/\s/.test(q)) return "https://" + q;
  return "https://duckduckgo.com/?q=" + encodeURIComponent(q);
}

function attachGateComplete(input, form) {
  if (!input || !form || input.dataset.shintoComplete === "1") return;
  if (typeof chrome === "undefined" || !chrome.runtime) return;
  input.dataset.shintoComplete = "1";

  const list = document.createElement("div");
  list.className = "gate-complete";
  list.hidden = true;
  const row = form.querySelector(".gate-row");
  form.insertBefore(list, row || form.firstChild);

  let items = [];
  let selected = -1;
  let seq = 0;
  let timer = 0;

  function hideList() {
    list.hidden = true;
    list.replaceChildren();
    items = [];
    selected = -1;
  }

  function paint() {
    for (let i = 0; i < list.children.length; i++) {
      list.children[i].setAttribute("aria-selected", i === selected ? "true" : "false");
    }
  }

  function render(next) {
    items = next || [];
    selected = -1;
    list.replaceChildren();
    if (!items.length) {
      list.hidden = true;
      return;
    }
    for (let i = 0; i < items.length; i++) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.textContent = items[i].label;
      btn.setAttribute("aria-selected", i === selected ? "true" : "false");
      btn.addEventListener("mousedown", (event) => {
        event.preventDefault();
        go(items[i]);
      });
      list.appendChild(btn);
    }
    list.hidden = false;
  }

  function requestComplete() {
    const q = input.value;
    const id = ++seq;
    clearTimeout(timer);
    timer = setTimeout(() => {
      chrome.runtime.sendMessage({ type: "complete", q }, (hits) => {
        if (chrome.runtime.lastError) return;
        if (id !== seq) return;
        render(hits);
      });
    }, q.trim() ? 80 : 0);
  }

  function apply(item) {
    if (!item) return;
    input.value = item.label;
    input.setSelectionRange(input.value.length, input.value.length);
  }

  function go(item) {
    const q = input.value;
    const url = item?.url || shintoToUrl(q);
    hideList();
    if (!url) return;
    chrome.runtime.sendMessage({ type: "typed", q, url });
    chrome.runtime.sendMessage({ type: "open", url }, () => {
      window.close();
    });
  }

  function move(delta) {
    if (!items.length) return;
    if (selected < 0) selected = delta > 0 ? 0 : items.length - 1;
    else selected = (selected + delta + items.length) % items.length;
    paint();
    apply(items[selected]);
  }

  form.addEventListener(
    "submit",
    (event) => {
      const pick = selected >= 0 && !list.hidden ? items[selected] : null;
      if (pick) {
        event.preventDefault();
        event.stopImmediatePropagation();
        go(pick);
        return;
      }
      const url = shintoToUrl(input.value);
      if (url) chrome.runtime.sendMessage({ type: "typed", q: input.value, url });
    },
    true
  );

  input.addEventListener("input", requestComplete);
  input.addEventListener("focus", requestComplete);
  input.addEventListener("blur", () => {
    setTimeout(hideList, 120);
  });

  input.addEventListener("keydown", (event) => {
    if (event.key === "ArrowDown") {
      event.preventDefault();
      if (list.hidden || !items.length) {
        requestComplete();
        return;
      }
      move(1);
      return;
    }
    if (event.key === "ArrowUp") {
      event.preventDefault();
      if (list.hidden || !items.length) {
        requestComplete();
        return;
      }
      move(-1);
      return;
    }
    if (event.key === "Tab" && !list.hidden && items.length) {
      event.preventDefault();
      if (selected < 0) selected = 0;
      apply(items[selected]);
      paint();
      return;
    }
    if (event.key === "Escape" && !list.hidden) {
      event.preventDefault();
      event.stopPropagation();
      hideList();
    }
  });
}

function hookShintoNewtabComplete() {
  const input = document.getElementById("q");
  const form = document.getElementById("go");
  if (input && form) attachGateComplete(input, form);
}

if (
  location.pathname.endsWith("/newtab.html") &&
  (location.hostname === "127.0.0.1" || location.hostname === "localhost") &&
  window === window.top
) {
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", hookShintoNewtabComplete, { once: true });
  } else {
    hookShintoNewtabComplete();
  }
}
