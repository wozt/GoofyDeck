const state = {
  status: null,
  page: "",
  pageDetail: null,
  selected: 1,
  signature: "",
  tapActions: [
    "$page.go_to",
    "$page.back",
    "$page.previous",
    "$page.next",
    "light.toggle",
    "script.turn_on",
    "homeassistant.toggle",
    "homeassistant.turn_on",
    "homeassistant.turn_off",
    "input_boolean.toggle",
    "input_button.press",
    "$cmd.exec",
    "call_service",
  ],
  presets: [],
};

const $ = (sel) => document.querySelector(sel);
const deck = $("#deck");
const pageSelect = $("#pageSelect");
const logEl = $("#log");
const form = $("#buttonForm");

function log(message) {
  const line = `[${new Date().toLocaleTimeString()}] ${message}`;
  logEl.textContent = `${line}\n${logEl.textContent}`.slice(0, 12000);
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(data.error || response.statusText);
  return data;
}

function buttonLabel(button) {
  return button?.name || button?.icon || button?.entity_id || "";
}

function renderStatus() {
  const s = state.status;
  if (!s) return;
  $("#statusLine").textContent = `${s.online ? "Daemon OK" : "Daemon hors ligne"} · ${s.currentPage || "aucune page"} · ${s.configPath}`;
  $("#pageMeta").textContent = `${s.socket} · offset ${s.offset || 0}`;
  
  // Only rebuild page select if the pages changed
  const currentPages = Array.from(pageSelect.options).map(opt => opt.value);
  const newPages = s.pages.map(p => p.name);
  
  if (JSON.stringify(currentPages) !== JSON.stringify(newPages)) {
    pageSelect.innerHTML = "";
    for (const page of s.pages) {
      const opt = document.createElement("option");
      opt.value = page.name;
      opt.textContent = `${page.name} (${page.buttonCount})`;
      pageSelect.appendChild(opt);
    }
  }
  
  // Always sync the selected page with current state
  if (state.page && pageSelect.value !== state.page) {
    pageSelect.value = state.page;
  } else if (s.currentPage && !state.page) {
    pageSelect.value = s.currentPage;
  }
  
  // Populate tap actions dropdown
  populateTapActions();
  // Populate presets dropdown from config
  populatePresets(s.parsed);
}

function renderDeck() {
  const buttons = state.pageDetail?.buttons || [];
  
  // Update existing buttons instead of recreating them to reduce blinking
  const existingButtons = Array.from(deck.children);
  
  for (let i = 1; i <= 14; i++) {
    const info = buttons[i - 1] || { position: i, name: "" };
    let btn = existingButtons[i - 1];
    
    if (!btn || btn.dataset.position != String(i)) {
      // Create new button if it doesn't exist
      btn = document.createElement("button");
      btn.className = `deckButton ${i === 14 ? "wide" : ""}`;
      btn.title = `Bouton ${i}: clic=TAP, Shift=HOLD, Alt=LONGHOLD`;
      btn.dataset.position = String(i);

      const img = document.createElement("img");
      img.alt = "";
      img.className = "button-icon";
      
      const fallback = document.createElement("span");
      fallback.className = "fallback";
      
      const badge = document.createElement("span");
      badge.className = "badge";
      badge.textContent = String(i);

      btn.append(img, fallback, badge);
      
      btn.addEventListener("click", (event) => {
        selectButton(i);
        const type = event.altKey ? "LONGHOLD" : event.shiftKey ? "HOLD" : "TAP";
        simulate(i, type);
      });
      btn.addEventListener("contextmenu", (event) => {
        event.preventDefault();
        selectButton(i);
        simulate(i, "RELEASED");
      });
      
      if (existingButtons[i - 1]) {
        deck.replaceChild(btn, existingButtons[i - 1]);
      } else {
        deck.appendChild(btn);
      }
    }
    
    // Update button state
    btn.className = `deckButton ${i === 14 ? "wide" : ""} ${i === state.selected ? "selected" : ""}`;
    
    // Update icon source with signature for cache busting
    const img = btn.querySelector(".button-icon");
    const newSrc = `/api/icon/${encodeURIComponent(state.page)}/${i}?v=${encodeURIComponent(state.signature)}`;
    
    // Only update if source actually changed
    if (img.src !== newSrc) {
      img.style.opacity = "0";
      img.src = newSrc;
      img.onload = () => { img.style.opacity = "1"; };
      img.onerror = () => { img.style.display = "none"; };
    }
    
    // Update fallback text
    const fallback = btn.querySelector(".fallback");
    const labelText = info.name || buttonLabel(info) || "";
    if (fallback.textContent !== labelText) {
      fallback.textContent = labelText;
    }
  }
  
  // Remove any extra buttons
  while (deck.children.length > 14) {
    deck.removeChild(deck.lastChild);
  }
}

function selectButton(position) {
  state.selected = position;
  const button = state.pageDetail?.buttons?.[position - 1] || {};
  $("#selectedTitle").textContent = `Bouton ${position} · ${state.page}`;
  form.name.value = button.name || "";
  form.icon.value = button.icon || "";
  form.text.value = button.text || "";
  form.entity_id.value = button.entity_id || "";
  
  // Handle presets select
  const presetValue = Array.isArray(button.presets) ? button.presets[0] : "";
  form.presets.value = presetValue || "";
  
  // Handle tap action select
  form.tap_action.value = button.tap_action || "";
  
  form.tap_data.value = button.tap_data || "";
  renderDeck();
}

function populateTapActions() {
  const select = $("#tapActionSelect");
  select.innerHTML = '<option value="">-- Sélectionner --</option>';
  for (const action of state.tapActions) {
    const opt = document.createElement("option");
    opt.value = action;
    opt.textContent = action;
    select.appendChild(opt);
  }
}

function populatePresets(parsed) {
  const select = $("#presetsSelect");
  select.innerHTML = '<option value="">-- Sélectionner --</option>';
  
  if (parsed && parsed.presets) {
    for (const presetName of Object.keys(parsed.presets)) {
      const opt = document.createElement("option");
      opt.value = presetName;
      opt.textContent = presetName;
      select.appendChild(opt);
    }
  }
  
  // Add custom option
  const customOpt = document.createElement("option");
  customOpt.value = "custom";
  customOpt.textContent = "-- Personnalisé (voir YAML) --";
  select.appendChild(customOpt);
}

async function loadStatus() {
  const status = await api("/api/status");
  state.status = status;
  state.signature = status.signature;
  
  // Use current page from status if it changed
  if (status.currentPage && status.currentPage !== state.page) {
    state.page = status.currentPage;
    await loadPage(status.currentPage);
  } else if (!state.page && status.pages.length > 0) {
    // Set initial page if none is set
    state.page = status.pages[0].name;
    await loadPage(state.page);
  }
  
  renderStatus();
  renderDeck();
}

async function loadPage(page) {
  if (!page) return;
  state.page = page;
  state.pageDetail = await api(`/api/page/${encodeURIComponent(page)}`);
  selectButton(Math.min(state.selected, 14));
}

async function loadConfigEditor() {
  const data = await api("/api/config");
  $("#yamlEditor").value = data.raw || "";
}

async function simulate(button, event) {
  try {
    const res = await api("/api/button/simulate", {
      method: "POST",
      body: JSON.stringify({ button, event }),
    });
    log(`${event}${button}: ${res.response}`);
    // Force immediate refresh after button press
    await loadStatus();
    if (state.page) {
      await loadPage(state.page);
    }
  } catch (err) {
    log(`${event}${button}: ${err.message}`);
  }
}

async function sendControl(command) {
  try {
    const res = await api("/api/control", {
      method: "POST",
      body: JSON.stringify({ command }),
    });
    log(`${command}: ${res.response}`);
    await loadStatus();
    if (state.page) await loadPage(state.page);
    await loadConfigEditor();
  } catch (err) {
    log(`${command}: ${err.message}`);
  }
}

document.querySelectorAll("[data-command]").forEach((btn) => {
  btn.addEventListener("click", () => sendControl(btn.dataset.command));
});

$("#pingBtn").addEventListener("click", () => sendControl("ping"));
$("#refreshBtn").addEventListener("click", async () => {
  await loadStatus();
  await loadPage(state.page);
});

pageSelect.addEventListener("change", async () => {
  const newPage = pageSelect.value;
  if (newPage && newPage !== state.page) {
    state.page = newPage;
    await loadPage(newPage);
  }
});

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  
  // Handle presets - if custom selected, read from YAML or keep existing
  let presetsValue = form.presets.value;
  if (presetsValue === "custom") {
    // Keep existing presets or empty array
    const currentButton = state.pageDetail?.buttons?.[state.selected - 1] || {};
    presetsValue = Array.isArray(currentButton.presets) ? currentButton.presets.join(", ") : "";
  }
  
  const payload = {
    page: state.page,
    position: state.selected,
    name: form.name.value,
    icon: form.icon.value,
    text: form.text.value,
    entity_id: form.entity_id.value,
    presets: presetsValue ? presetsValue.split(",").map((v) => v.trim()).filter(Boolean) : [],
    tap_action: form.tap_action.value,
    tap_data: form.tap_data.value,
  };
  try {
    await api("/api/button", { method: "PUT", body: JSON.stringify(payload) });
    log(`Bouton ${state.selected} sauvegardé`);
    await loadStatus();
    await loadPage(state.page);
    await loadConfigEditor();
  } catch (err) {
    log(`Sauvegarde bouton: ${err.message}`);
  }
});

$("#saveYamlBtn").addEventListener("click", async () => {
  try {
    await api("/api/config", {
      method: "PUT",
      body: JSON.stringify({ raw: $("#yamlEditor").value, reload: true }),
    });
    log("YAML sauvegardé");
    await loadStatus();
    await loadPage(state.page);
  } catch (err) {
    log(`Sauvegarde YAML: ${err.message}`);
  }
});

$("#reloadYamlBtn").addEventListener("click", loadConfigEditor);

document.querySelectorAll(".tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((el) => el.classList.remove("active"));
    document.querySelectorAll(".tabPage").forEach((el) => el.classList.remove("active"));
    tab.classList.add("active");
    $(`#${tab.dataset.tab}Tab`).classList.add("active");
    if (tab.dataset.tab === "yaml") loadConfigEditor();
  });
});

const events = new EventSource("/api/events");
events.addEventListener("update", async (event) => {
  const data = JSON.parse(event.data);
  
  // Always check for page changes
  if (data.page && data.page !== state.page) {
    state.page = data.page;
    if (pageSelect.value !== data.page) {
      pageSelect.value = data.page;
    }
    await loadPage(data.page);
    await loadStatus();
  } 
  // Check for signature changes (config/icon updates)
  else if (data.signature !== state.signature) {
    state.signature = data.signature;
    // Force icon refresh by updating all icon sources
    const images = document.querySelectorAll('.button-icon');
    images.forEach(img => {
      const currentSrc = img.src;
      // Update just the signature parameter
      const url = new URL(currentSrc, window.location.origin);
      url.searchParams.set('v', encodeURIComponent(state.signature));
      img.src = url.toString();
    });
    await loadStatus();
  }
});

loadStatus()
  .then(loadConfigEditor)
  .catch((err) => log(err.message));
