const $ = (id) => document.getElementById(id);
const historyCache = new Map();
let currentLinks = [];
let socket;
let reconnectTimer;
const fmt = (value, digits = 1) => Number(value).toFixed(digits);
const stamp = (value) => { try { return new Date(value).toLocaleTimeString([], {hour:"2-digit",minute:"2-digit",second:"2-digit"}); } catch { return "—"; } };

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (ch) => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[ch]));
}
function renderLinks(links) {
  currentLinks = links;
  $("links").innerHTML = links.map((link) => {
    const score = Math.max(0, Math.min(100, Number(link.quality_score)));
    historyCache.set(link.id, [...(historyCache.get(link.id) || []), Number(link.rtt_ms)].slice(-48));
    return `<article class="link-card">
      <div class="link-top"><div class="link-icon">${escapeHtml(link.id.toUpperCase())}</div><div class="link-title"><strong>${escapeHtml(link.operator)}</strong><small>${escapeHtml(link.id)} · synthetic path</small></div><span class="badge ${escapeHtml(link.state)}">${escapeHtml(link.state)}</span></div>
      <div class="metric-row"><div class="metric"><span>ROUND TRIP</span><strong>${fmt(link.rtt_ms)} ms</strong></div><div class="metric"><span>PACKET LOSS</span><strong>${fmt(link.loss_pct,2)}%</strong></div><div class="metric"><span>RSSI</span><strong>${fmt(link.rssi_dbm,1)} dBm</strong></div></div>
      <div class="bar-head"><span>QUALITY SCORE</span><span>${fmt(score,1)} / 100</span></div><div class="bar"><i style="width:${score}%"></i></div>
    </article>`;
  }).join("");
  $("faultControls").innerHTML = links.map((link) => `<button class="${link.faulted ? "active" : ""}" data-link="${escapeHtml(link.id)}" data-fault="${!link.faulted}">${link.faulted ? "Restore" : "Fail"} ${escapeHtml(link.id.toUpperCase())}</button>`).join("");
  document.querySelectorAll("[data-link]").forEach((button) => button.addEventListener("click", async () => {
    try {
      const response = await fetch("/api/simulation/fault", {method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({link_id:button.dataset.link,faulted:button.dataset.fault === "true"})});
      const data = await response.json();
      if (!response.ok) throw new Error(data.detail || "Fault injection failed");
      $("faultMessage").textContent = `Simulation updated: ${button.dataset.link.toUpperCase()} ${button.dataset.fault === "true" ? "failed" : "restored"}. No real interface changed.`;
      renderDashboard(data.dashboard);
    } catch (error) { $("faultMessage").textContent = `Error: ${error.message}`; }
  }));
}
function renderWeights(links) {
  $("weights").innerHTML = links.map((link) => `<div class="weight-row"><span>${escapeHtml(link.id.toUpperCase())}</span><div class="weight-track"><i style="width:${link.recommended_weight_pct}%"></i></div><strong>${link.recommended_weight_pct}%</strong></div>`).join("");
}
function renderEvents(events) {
  $("events").innerHTML = events.slice(0,10).map((event) => `<div class="event"><i class="event-dot ${event.level === "warning" ? "warning" : ""}"></i><div><strong>${escapeHtml(event.title)}</strong><small>${escapeHtml(event.detail)}</small><time>${stamp(event.timestamp)}</time></div></div>`).join("") || '<div class="empty">No events yet.</div>';
}
function drawChart(links) {
  const canvas = $("rttChart");
  const ctx = canvas.getContext("2d");
  const w = canvas.width = Math.max(300, canvas.clientWidth * (window.devicePixelRatio || 1));
  const h = canvas.height = 270;
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle = "#203047"; ctx.lineWidth = 1;
  for (let i = 0; i < 5; i++) {
    const y = 16 + (h-40)*i/4;
    ctx.beginPath(); ctx.moveTo(38,y); ctx.lineTo(w-12,y); ctx.stroke();
    ctx.fillStyle="#71839b"; ctx.font="10px Segoe UI"; ctx.fillText(String(300-i*75),5,y+3);
  }
  const palette = ["#62e5f1","#69e1b1","#f7c977"];
  links.forEach((link,index) => {
    const values = historyCache.get(link.id) || [];
    if (values.length < 2) return;
    ctx.beginPath(); ctx.strokeStyle=palette[index%palette.length]; ctx.lineWidth=2;
    values.forEach((val,i) => {
      const x=38+(w-55)*i/Math.max(1,values.length-1);
      const y=16+(h-40)*(1-Math.max(0,Math.min(300,val))/300);
      if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
    });
    ctx.stroke();
  });
}
function renderDashboard(data) {
  const links = data.links || [];
  $("connection").textContent = "CONNECTED";
  $("connection").style.color = "var(--green)";
  $("activeLinks").textContent = `${data.active_links} / ${data.total_links}`;
  $("linkSummary").textContent = data.active_links === data.total_links ? "All simulated paths available" : "One or more paths unavailable";
  $("tick").textContent = data.tick;
  const available = links.filter((link) => link.state !== "DOWN");
  $("bestRtt").textContent = available.length ? `${fmt(Math.min(...available.map((link) => link.rtt_ms)))} ms` : "—";
  $("lastUpdate").textContent = "Updated " + stamp(data.timestamp) + " · " + data.evidence_label;
  $("linkSummary").textContent = data.evidence_label;
  renderLinks(links); renderWeights(links); renderEvents(data.events || []); drawChart(links);
}
async function poll() {
  try {
    const response = await fetch("/api/dashboard", {cache:"no-store"});
    if (!response.ok) throw new Error("API returned " + response.status);
    renderDashboard(await response.json());
  } catch (error) {
    $("connection").textContent = "BACKEND OFFLINE";
    $("connection").style.color = "var(--red)";
    $("lastUpdate").textContent = error.message;
  }
}
function connectSocket() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;
  const protocol = location.protocol === "https:" ? "wss:" : "ws:";
  socket = new WebSocket(`${protocol}//${location.host}/ws`);
  socket.onopen = () => { $("connection").textContent = "CONNECTED · LIVE"; };
  socket.onmessage = (event) => { try { renderDashboard(JSON.parse(event.data)); } catch { /* Ignore malformed telemetry frame. */ } };
  socket.onclose = () => {
    $("connection").textContent = "RECONNECTING";
    clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(connectSocket,2500);
  };
  socket.onerror = () => socket.close();
}
window.addEventListener("resize", () => drawChart(currentLinks));
poll();
connectSocket();
