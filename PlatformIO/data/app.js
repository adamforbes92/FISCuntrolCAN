const POLL_MS = 500;
const SETTINGS_AUTOSAVE_MS = 250;

let settingsSaveTimer = null;

function byId(id) {
  return document.getElementById(id);
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

function pad2(n) {
  return String(n).padStart(2, "0");
}

function formatBrowserTime() {
  const d = new Date();
  return (
    `${d.getFullYear()}-${pad2(d.getMonth() + 1)}-${pad2(d.getDate())} ` +
    `${pad2(d.getHours())}:${pad2(d.getMinutes())}:${pad2(d.getSeconds())}`
  );
}

// Send the browser's local wall-clock to the device (no NTP in AP mode).
async function postDeviceTime() {
  const d = new Date();
  try {
    await fetchJson("/api/time", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        year: d.getFullYear(),
        month: d.getMonth() + 1,
        day: d.getDate(),
        hour: d.getHours(),
        minute: d.getMinutes(),
        second: d.getSeconds(),
      }),
    });
  } catch (e) {
    /* device clock sync is best-effort */
  }
}

function setNav() {
  const tabs = document.querySelectorAll(".nav-tab");
  const pages = document.querySelectorAll(".page");

  tabs.forEach((tab) => {
    tab.addEventListener("click", () => {
      tabs.forEach((t) => t.classList.remove("active"));
      pages.forEach((p) => p.classList.remove("active"));
      tab.classList.add("active");
      byId(`${tab.dataset.page}-page`).classList.add("active");
    });
  });
}

function setFISLines(lines) {
  const nodes = document.querySelectorAll("#fisLines .fis-line");
  nodes.forEach((node, index) => {
    node.textContent = (lines && lines[index]) || "";
  });
}

async function pressButton(button) {
  await fetchJson("/api/buttons/press", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ button }),
  });
}

async function restartIgnition() {
  await fetchJson("/api/ignition/restart", {
    method: "POST",
  });
}

async function loadSettings() {
  const data = await fetchJson("/api/settings");
  byId("uiTitle").textContent = data.title || "FISCuntrolCAN";
  byId("bootSource").value = data.bootSource || "can";
  byId("bootScreen").value = String(data.bootScreen ?? 2);
  const logCat = data.logCat || {};
  byId("logSys").value = String(logCat.sys !== false);
  byId("logCan").value = String(!!logCat.can);
  byId("logKline").value = String(logCat.kline !== false);
  byId("logIo").value = String(logCat.io !== false);
  byId("logWifi").value = String(!!logCat.wifi);
  byId("oilSensorEnabled").value = String(!!data.oilSensorEnabled);
  byId("serialMirrorToCard").value = String(!!data.serialMirrorToCard);
  const rpmThreshold = Number(data.rpmLogoThreshold ?? 3000);
  byId("rpmLogoThreshold").value = String(rpmThreshold);
  byId("rpmLogoThresholdValue").textContent = String(rpmThreshold);
  byId("rpmLogoSelection").value = String(data.rpmLogoSelection ?? 1);
  byId("bootLogoStatus").textContent = data.customBootLogoAvailable
    ? "Custom logo stored and ready."
    : "No custom logo uploaded.";
  byId("rpmLogoStatus").textContent = data.customRpmLogoAvailable
    ? "RPM logo stored and ready."
    : "No RPM logo uploaded.";
  if (data.klineDefaultModule !== undefined) {
    byId("klineDefaultModule").value = String(data.klineDefaultModule);
  }
  updateExtOutputUI(!!data.extOutputEnabled, data.extOutputPin);
  byId("extOutputMode").value = String(data.extOutputMode ?? 0);
  const extRpm = Number(data.extOutputRpmThreshold ?? 3000);
  byId("extOutputRpmThreshold").value = String(extRpm);
  byId("extOutputRpmThresholdValue").textContent = String(extRpm);
  const extSpeed = Number(data.extOutputSpeedThreshold ?? 60);
  byId("extOutputSpeedThreshold").value = String(extSpeed);
  byId("extOutputSpeedThresholdValue").textContent = String(extSpeed);
  updateExtOutputModeUI(Number(data.extOutputMode ?? 0));

  byId("welcomeGreetingEnabled").value = String(!!data.welcomeGreetingEnabled);
  byId("welcomeGreetName").value = data.welcomeGreetName || "";
  byId("specialDatesEnabled").value = String(!!data.specialDatesEnabled);
  const sd = data.specialDates || [];
  for (let i = 0; i < 5; i++) {
    const dateVal = sd[i]?.date;
    byId(`specialDate${i}Date`).value = (!dateVal || dateVal === "1970-01-01") ? "" : dateVal;
    byId(`specialDate${i}Text`).value = sd[i]?.text || "";
  }
}

function statusText(value) {
  return value ? "Healthy" : "Unhealthy";
}

function connectedText(value) {
  return value ? "Connected" : "Disconnected";
}

function fisBootText(value) {
  return value ? "Ready" : "Failed/Timeout";
}

function fisFeedbackText(value) {
  return value ? "OK" : "NO";
}

function applyApiCardModeBadge(mode) {
  const normalizedMode = mode === "serial" ? "serial" : "fis";
  const apiCardModeStatus = byId("apiCardModeStatus");
  apiCardModeStatus.textContent = `API Card: ${normalizedMode.toUpperCase()}`;
  apiCardModeStatus.classList.remove("fis-mode", "serial-mode");
  apiCardModeStatus.classList.add(normalizedMode === "serial" ? "serial-mode" : "fis-mode");
}

async function refreshDashboard() {
  const data = await fetchJson("/api/fis");

  byId("uiTitle").textContent = data.title || "FISCuntrolCAN";
  byId("fisSource").textContent = data.viewSource || "none";
  byId("fisSelectedSource").textContent = data.selectedSource || "none";
  byId("fisTitle").textContent = data.viewTitle || "NO CARD";
  setFISLines(data.lines || []);

  const canHealthy = !!data.chassisCAN;
  const klineHealthy = !!data.klineHealthy;
  const apiCardMode = data.apiCardMode || (data.serialMirrorToCard ? "serial" : "fis");
  byId("canStatus").textContent = `CAN: ${canHealthy ? "OK" : "NO"}`;
  byId("klineStatus").textContent = `K-Line: ${klineHealthy ? "OK" : "NO"}`;
  applyApiCardModeBadge(apiCardMode);

  byId("diagCanConnected").textContent = connectedText(!!data.canConnected);
  byId("diagCanHealthy").textContent = statusText(canHealthy);
  byId("diagKlineConnected").textContent = connectedText(!!data.klineConnected);
  byId("diagKlineHealthy").textContent = statusText(klineHealthy);
  byId("diagIgnition").textContent = data.ignition ? "Live" : "Off";
  byId("diagHeap").textContent = Math.round((data.freeHeap || 0) / 1024);
  byId("diagFisBootReady").textContent = fisBootText(!!data.fisBootReady);
  byId("diagFisFeedback").textContent = fisFeedbackText(!!data.FIS_Feedback);

  byId("diagUpInput").textContent = `GPIO ${data.upInputPin}: ${data.upInputPressed ? "Pressed" : "Released"}`;
  byId("diagDownInput").textContent = `GPIO ${data.downInputPin}: ${data.downInputPressed ? "Pressed" : "Released"}`;
  byId("diagResetInput").textContent = `GPIO ${data.resetInputPin}: ${data.resetInputPressed ? "Pressed" : "Released"}`;

  byId("diagUpOutput").textContent = `GPIO ${data.upOutputPin}: ${data.upOutputActive ? "Active" : "Idle"} (${data.upOutputLevel})`;
  byId("diagDownOutput").textContent = `GPIO ${data.downOutputPin}: ${data.downOutputActive ? "Active" : "Idle"} (${data.downOutputLevel})`;
  byId("diagResetOutput").textContent = `GPIO ${data.resetOutputPin}: ${data.resetOutputActive ? "Active" : "Idle"} (${data.resetOutputLevel})`;

  // K-Line tab status badge
  byId("klineFaultStatus").textContent = data.klineConnected ? "Connected" : "Not Connected";

  // CAN tab status
  byId("canPageConnected").textContent = connectedText(!!data.canConnected);

  // Device clock
  byId("clockBrowserTime").textContent = formatBrowserTime();
  byId("clockDeviceTime").textContent =
    data.timeValid && data.deviceTime ? data.deviceTime : "Not set";
  byId("clockRtcTime").textContent =
    data.rtcPresent ? (data.rtcTime || "Not running") : "No RTC";
  byId("clockRtcStatus").textContent = !data.rtcPresent
    ? "Not detected"
    : data.rtcHealthy
    ? "Healthy"
    : "Not healthy";
}

async function saveSettings() {
  const payload = {
    bootSource: byId("bootSource").value,
    bootScreen: Number(byId("bootScreen").value),
    logCat: {
      sys: byId("logSys").value === "true",
      can: byId("logCan").value === "true",
      kline: byId("logKline").value === "true",
      io: byId("logIo").value === "true",
      wifi: byId("logWifi").value === "true",
    },
    oilSensorEnabled: byId("oilSensorEnabled").value === "true",
    serialMirrorToCard: byId("serialMirrorToCard").value === "true",
    rpmLogoThreshold: Number(byId("rpmLogoThreshold").value),
    rpmLogoSelection: Number(byId("rpmLogoSelection").value),
    klineDefaultModule: Number(byId("klineDefaultModule").value),
    welcomeGreetingEnabled: byId("welcomeGreetingEnabled").value === "true",
    specialDatesEnabled: byId("specialDatesEnabled").value === "true",
    welcomeGreetName: byId("welcomeGreetName").value,
    extOutputMode: Number(byId("extOutputMode").value),
    extOutputRpmThreshold: Number(byId("extOutputRpmThreshold").value),
    extOutputSpeedThreshold: Number(byId("extOutputSpeedThreshold").value),
    specialDates: Array.from({ length: 5 }, (_, i) => ({
      date: byId(`specialDate${i}Date`).value || "1970-01-01",
      text: byId(`specialDate${i}Text`).value,
    })),
  };

  await fetchJson("/api/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
}

function setSettingsStatus(text, isError = false) {
  const node = byId("settingsStatus");
  if (!node) return;
  node.textContent = text;
  node.style.color = isError ? "#f85149" : "";
}

function queueSettingsSave() {
  if (settingsSaveTimer) {
    clearTimeout(settingsSaveTimer);
  }

  setSettingsStatus("Saving settings...");
  settingsSaveTimer = setTimeout(async () => {
    try {
      await saveSettings();
      await refreshDashboard();
      setSettingsStatus("Settings saved.");
    } catch {
      setSettingsStatus("Failed to save settings.", true);
    }
  }, SETTINGS_AUTOSAVE_MS);
}

async function uploadRpmLogo() {
  const fileInput = byId("rpmLogoFile");
  const file = fileInput.files && fileInput.files[0];

  if (!file) {
    byId("rpmLogoStatus").textContent = "Choose the raw 704-byte RPM-logo file first.";
    return;
  }

  const formData = new FormData();
  formData.append("file", file, file.name);

  const response = await fetch("/api/rpm-logo", {
    method: "POST",
    body: formData,
  });

  if (!response.ok) {
    const errorText = await response.text();
    byId("rpmLogoStatus").textContent = errorText || "Upload failed. Use only the raw 704-byte RPM-logo file.";
    throw new Error(`HTTP ${response.status}`);
  }

  const data = await response.json();
  if (data.customRpmLogoAvailable) {
    byId("rpmLogoSelection").value = "2";
  }
  byId("rpmLogoStatus").textContent = data.customRpmLogoAvailable
    ? "RPM logo uploaded successfully."
    : "Upload failed. Use only the raw 704-byte RPM-logo file.";
}

async function testShiftLogo() {
  // Persist the current selection first so the FIS shows what's chosen, then trigger the test.
  await saveSettings();
  const response = await fetch("/api/test-shift-logo", { method: "POST" });
  byId("rpmLogoStatus").textContent = response.ok
    ? "Showing shift indicator on the display for 3 seconds."
    : "Could not start the shift indicator test.";
}

async function uploadBootLogo() {
  const fileInput = byId("bootLogoFile");
  const file = fileInput.files && fileInput.files[0];

  if (!file) {
    byId("bootLogoStatus").textContent = "Choose the raw 704-byte boot-logo file first.";
    return;
  }

  const formData = new FormData();
  formData.append("file", file, file.name);

  const response = await fetch("/api/boot-logo", {
    method: "POST",
    body: formData,
  });

  if (!response.ok) {
    const errorText = await response.text();
    byId("bootLogoStatus").textContent = errorText || "Upload failed. Use only the raw 704-byte boot-logo file.";
    throw new Error(`HTTP ${response.status}`);
  }

  const data = await response.json();
  byId("bootScreen").value = data.customBootLogoAvailable ? "5" : byId("bootScreen").value;
  byId("bootLogoStatus").textContent = data.customBootLogoAvailable
    ? "Custom logo uploaded successfully."
    : "Upload failed. Use only the raw 704-byte boot-logo file.";
}

function bindAutoSaveEvents() {
  const settingIds = [
    "bootSource",
    "bootScreen",
    "logSys",
    "logCan",
    "logKline",
    "logIo",
    "logWifi",
    "oilSensorEnabled",
    "serialMirrorToCard",
    "rpmLogoThreshold",
    "rpmLogoSelection",
    "welcomeGreetingEnabled",
    "specialDatesEnabled",
    "welcomeGreetName",
    "extOutputMode",
    "specialDate0Date", "specialDate0Text",
    "specialDate1Date", "specialDate1Text",
    "specialDate2Date", "specialDate2Text",
    "specialDate3Date", "specialDate3Text",
    "specialDate4Date", "specialDate4Text",
  ];

  settingIds.forEach((id) => {
    const node = byId(id);
    if (!node) return;
    node.addEventListener("change", queueSettingsSave);
  });

  // Boot logo and welcome greeting are mutually exclusive: enabling one turns
  // the other off so they can never be selected together.
  byId("bootScreen").addEventListener("change", () => {
    if (Number(byId("bootScreen").value) >= 2) {
      byId("welcomeGreetingEnabled").value = "false";
    }
  });
  byId("welcomeGreetingEnabled").addEventListener("change", () => {
    if (byId("welcomeGreetingEnabled").value === "true") {
      byId("bootScreen").value = "0";
    }
  });

  byId("klineDefaultModule").addEventListener("change", queueSettingsSave);

  byId("rpmLogoThreshold").addEventListener("input", () => {
    byId("rpmLogoThresholdValue").textContent = byId("rpmLogoThreshold").value;
    queueSettingsSave();
  });

  byId("serialMirrorToCard").addEventListener("change", () => {
    applyApiCardModeBadge(byId("serialMirrorToCard").value === "true" ? "serial" : "fis");
  });

  byId("extOutputMode").addEventListener("change", () => {
    updateExtOutputModeUI(Number(byId("extOutputMode").value));
  });

  byId("extOutputRpmThreshold").addEventListener("input", () => {
    byId("extOutputRpmThresholdValue").textContent = byId("extOutputRpmThreshold").value;
    queueSettingsSave();
  });

  byId("extOutputSpeedThreshold").addEventListener("input", () => {
    byId("extOutputSpeedThresholdValue").textContent = byId("extOutputSpeedThreshold").value;
    queueSettingsSave();
  });
}

// Show only the controls relevant to the selected external-output trigger mode.
function updateExtOutputModeUI(mode) {
  const manual = byId("extOutputManualControls");
  const rpmGroup = byId("extOutputRpmGroup");
  const speedGroup = byId("extOutputSpeedGroup");
  if (manual) manual.style.display = mode === 0 ? "" : "none";
  if (rpmGroup) rpmGroup.style.display = mode === 1 ? "" : "none";
  if (speedGroup) speedGroup.style.display = mode === 2 ? "" : "none";
}

function updateExtOutputUI(enabled, pin) {
  const pinEl = byId("extOutputPin");
  const stateEl = byId("extOutputState");
  if (pinEl && pin !== undefined) {
    pinEl.textContent = `GPIO ${pin}`;
  }
  if (stateEl) {
    stateEl.textContent = enabled ? "ENABLED" : "DISABLED";
    stateEl.style.color = enabled ? "var(--success)" : "var(--danger)";
  }
}

async function setExtOutput(enabled) {
  try {
    await fetchJson("/api/extoutput", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled }),
    });
    updateExtOutputUI(enabled);
  } catch {
    // silently fail
  }
}

async function loadCanCards() {
  try {
    const data = await fetchJson("/api/can/cards");
    const list = byId("canCardList");
    list.innerHTML = "";
    byId("canPageCurrentCard").textContent = data.cards && data.cards[data.currentIndex]
      ? data.cards[data.currentIndex] : "--";
    (data.cards || []).forEach((name, index) => {
      const btn = document.createElement("button");
      btn.className = "mode-btn" + (index === data.currentIndex ? " active-card" : "");
      btn.textContent = name;
      btn.addEventListener("click", async () => {
        await fetchJson("/api/can/cards/select", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ index }),
        });
        await loadCanCards();
      });
      list.appendChild(btn);
    });
  } catch {
    // silently fail
  }
}

function attachActions() {
  document.querySelectorAll(".sim-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      await pressButton(btn.dataset.button);
      await refreshDashboard();
    });
  });

  byId("restartIgnition").addEventListener("click", async () => {
    await restartIgnition();
    await refreshDashboard();
  });

  bindAutoSaveEvents();

  byId("uploadRpmLogo").addEventListener("click", async () => {
    await uploadRpmLogo();
  });

  byId("uploadBootLogo").addEventListener("click", async () => {
    await uploadBootLogo();
  });

  byId("testShiftLogo").addEventListener("click", async () => {
    await testShiftLogo();
  });

  // K-Line fault actions
  byId("readFaultsBtn").addEventListener("click", async () => {
    byId("faultOpStatus").textContent = "Reading fault codes...";
    byId("faultList").innerHTML = "";
    try {
      const data = await fetchJson("/api/kline/faults");
      const count = data.count ?? 0;
      byId("faultOpStatus").textContent = count === 0 ? "No faults found." : `${count} fault(s) found.`;
      const list = byId("faultList");
      (data.faults || []).forEach((f) => {
        const item = document.createElement("div");
        item.className = "fault-item";
        const badge = f.intermittent ? " <span class=\"fault-intermittent\">Intermittent</span>" : "";
        item.innerHTML = `<span class="fault-code">${String(f.code).padStart(5, "0")}</span><span class="fault-desc">${f.description}${badge}</span>${f.elaboration ? `<span class="fault-elab">${f.elaboration}</span>` : ""}`;
        list.appendChild(item);
      });
    } catch (e) {
      byId("faultOpStatus").textContent = e.message.includes("503") ? "K-Line not connected." : "Failed to read faults.";
    }
  });

  byId("clearFaultsBtn").addEventListener("click", async () => {
    byId("faultOpStatus").textContent = "Clearing fault codes...";
    try {
      const data = await fetchJson("/api/kline/faults/clear", { method: "POST" });
      byId("faultOpStatus").textContent = data.ok ? "Faults cleared." : "Clear command failed.";
      byId("faultList").innerHTML = "";
    } catch (e) {
      byId("faultOpStatus").textContent = e.message.includes("503") ? "K-Line not connected." : "Failed to clear faults.";
    }
  });

  // External Output toggle
  byId("extOutputOnBtn").addEventListener("click", () => setExtOutput(true));
  byId("extOutputOffBtn").addEventListener("click", () => setExtOutput(false));

  // CAN card list — load once on start; clicking buttons selects a card
  loadCanCards();
}

async function start() {
  setNav();
  attachActions();
  await postDeviceTime();
  await loadSettings();
  await refreshDashboard();
  setInterval(refreshDashboard, POLL_MS);
}

document.addEventListener("DOMContentLoaded", () => {
  start().catch(() => {
    byId("canStatus").textContent = "CAN: API error";
    byId("klineStatus").textContent = "K-Line: API error";
  });
});
