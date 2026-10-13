"use strict";
const form = document.querySelector("#query");
form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const button = form.querySelector("button");
  const state = document.querySelector("#state");
  button.disabled = true;
  state.textContent = "Loading…";
  const params = new URLSearchParams({
    action: document.querySelector("#action").value,
    arg: document.querySelector("#arg").value.trim(),
  });
  try {
    const response = await fetch(`api.php?${params}`, { signal: AbortSignal.timeout(10000) });
    const result = await response.json();
    state.textContent = result.status;
    document.querySelector("#result").textContent = result.body;
  } catch {
    state.textContent = "Request failed. Check the API endpoint and backend.";
  } finally {
    button.disabled = false;
  }
});
form.requestSubmit();

function rows(target, records) {
  const body = document.querySelector(target);
  body.replaceChildren();
  for (const record of records) {
    const row = document.createElement("tr");
    for (const value of record) {
      const cell = document.createElement("td");
      cell.textContent = String(value);
      row.append(cell);
    }
    body.append(row);
  }
}

async function refreshOverview() {
  const state = document.querySelector("#overview-state");
  try {
    const response = await fetch("api.php?action=overview", { signal: AbortSignal.timeout(10000) });
    if (response.status === 400) {
      state.textContent =
        "Live overview requires the Python adapter. Command queries are available.";
      return;
    }
    const result = await response.json();
    if (!response.ok || result.status !== "OK") throw new Error(result.body);
    const fields = result.body.split(";");
    const uptime = Math.floor(Number(fields[3]) / 3600);
    document.querySelector("#system").textContent =
      `SoftGRE ${fields[4]} · ${fields[2]} · Host uptime ${uptime} hours · ${fields[0]}`;
    rows(
      "#tunnels",
      result.tunnels.map((t) => [t.remote, t.interface, t.macs.join(", ")]),
    );
    rows(
      "#interfaces",
      result.interfaces.map((i) => [i.name, i.bridge || "—", i.state, i.rx_packets, i.tx_packets]),
    );
    const macCount = result.tunnels.reduce((total, t) => total + t.macs.length, 0);
    state.textContent =
      `${result.tunnel_count} tunnels · ${macCount} MACs shown · Updated ` +
      new Date().toLocaleTimeString() +
      (result.truncated ? " · Showing the first 64 tunnels" : "");
  } catch {
    state.textContent =
      "Overview unavailable. Last snapshot may be stale. Check the adapter and daemon.";
  }
  setTimeout(refreshOverview, 5000);
}
refreshOverview();
