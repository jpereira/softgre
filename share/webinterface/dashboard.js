"use strict";
const form = document.querySelector("#query");
form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const button = form.querySelector("button");
  const state = document.querySelector("#state");
  button.disabled = true;
  state.textContent = "Loading…";
  const params = new URLSearchParams({action: document.querySelector("#action").value,
                                    arg: document.querySelector("#arg").value.trim()});
  try {
    const response = await fetch(`api.php?${params}`, {signal: AbortSignal.timeout(10000)});
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
