// Glowmeter — LED control + live reading for the class ESP32-S3 board.
// Protocol: JSON over WebSocket, ws://<addr>/ws. See firmware/PROTOCOL.md.
"use strict";

var $ = function(id){ return document.getElementById(id); };

var els = {
  connDot: $('connDot'), connText: $('connText'),
  addrInput: $('addrInput'), connectBtn: $('connectBtn'), demoBtn: $('demoBtn'),
  mixedContentHint: $('mixedContentHint'), httpLink: $('httpLink'), statusHint: $('statusHint'),
  demoBanner: $('demoBanner'),
  ledPanel: $('ledPanel'), swatch: $('swatch'), colorInput: $('colorInput'),
  brightInput: $('brightInput'), brightVal: $('brightVal'), presets: $('presets'),
  readingPanel: $('readingPanel'), readoutVal: $('readoutVal'), readoutUnit: $('readoutUnit'),
  sourceSelect: $('sourceSelect'), chart: $('chart'),
  scaleMin: $('scaleMin'), scaleMid: $('scaleMid'), scaleMax: $('scaleMax'),
  telemetryPanel: $('telemetryPanel'),
  tUptime: $('tUptime'), tRssi: $('tRssi'), tHeap: $('tHeap'), tClients: $('tClients')
};

var state = {
  ws: null, mode: 'idle', // idle | connecting | live | demo
  reqId: 1, reconnectAttempts: 0, reconnectTimer: null,
  blocks: [], source: 'base.temp_c', history: [], HISTORY_MAX: 600,
  draggingColor: false, draggingBright: false,
  demoTimer: null, demoT: 0
};

var SOURCES = {
  'base.temp_c': { label: 'Board temperature', unit: '°C' }
};
for (var i = 1; i <= 8; i++) {
  SOURCES['b1.ai' + i] = { label: 'Input ' + i, unit: 'V' };
}

// ---------------- helpers ----------------
function hexToRgb(hex){
  var m = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return m ? { r: parseInt(m[1],16), g: parseInt(m[2],16), b: parseInt(m[3],16) } : { r:0,g:0,b:0 };
}
function rgbToHex(r,g,b){
  function h(n){ n = Math.max(0,Math.min(255,Math.round(n))).toString(16); return n.length<2?'0'+n:n; }
  return '#'+h(r)+h(g)+h(b);
}
function fmtUptime(s){
  s = Math.floor(s||0);
  var h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sec = s%60;
  return (h?h+':':'') + (h?String(m).padStart(2,'0'):m) + ':' + String(sec).padStart(2,'0');
}
function setConn(kind, text){
  els.connDot.className = 'dot' + (kind ? ' '+kind : '');
  els.connText.textContent = text;
}

// ---------------- connection ----------------
function loadAddr(){
  try { return localStorage.getItem('glowmeter.addr') || ''; } catch(e){ return ''; }
}
function saveAddr(v){ try { localStorage.setItem('glowmeter.addr', v); } catch(e){} }

els.addrInput.value = loadAddr();

function connect(){
  var addr = els.addrInput.value.trim();
  if (!addr) { els.addrInput.focus(); return; }
  saveAddr(addr);
  stopDemo();
  if (location.protocol === 'https:') {
    els.httpLink.href = 'http://' + addr + '/';
    els.httpLink.textContent = 'http://' + addr + '/';
    els.mixedContentHint.hidden = false;
  } else {
    els.mixedContentHint.hidden = true;
  }
  openSocket(addr);
}

function openSocket(addr){
  if (state.ws) { try { state.ws.close(); } catch(e){} }
  state.mode = 'connecting';
  setConn('', 'connecting…');
  var ws;
  try { ws = new WebSocket('ws://' + addr + '/ws'); }
  catch(e){ handleConnFailure(); return; }
  state.ws = ws;
  ws.onopen = function(){ state.reconnectAttempts = 0; };
  ws.onmessage = function(ev){ onMessage(ev.data); };
  ws.onclose = function(){ if (state.mode !== 'demo') handleConnFailure(); };
  ws.onerror = function(){ /* onclose follows */ };
}

function handleConnFailure(){
  state.mode = 'idle';
  setConn('err', 'offline');
  els.ledPanel.hidden = true; els.readingPanel.hidden = true; els.telemetryPanel.hidden = true;
  if (state.reconnectAttempts < 5) {
    state.reconnectAttempts++;
    els.statusHint.textContent = 'Connection failed — retrying (' + state.reconnectAttempts + '/5)…';
    state.reconnectTimer = setTimeout(function(){ openSocket(els.addrInput.value.trim()); }, 3000);
  } else {
    els.statusHint.textContent = 'Could not reach the board. Check the address and that your phone is on the same WiFi, or try the demo.';
  }
}

function onMessage(data){
  var msg;
  try { msg = JSON.parse(data); } catch(e){ return; }
  if (msg.type === 'hello') {
    state.blocks = msg.blocks || [];
    buildSourceOptions();
    state.mode = 'live';
    setConn('ok', 'connected · fw ' + (msg.fw||'?') + (msg.sim?' · sim':''));
    els.ledPanel.hidden = false; els.readingPanel.hidden = false; els.telemetryPanel.hidden = false;
    els.demoBanner.hidden = true;
  } else if (msg.type === 'status') {
    applyStatus(msg.blocks || {});
  } else if (typeof msg.ok !== 'undefined' && msg.ok === false) {
    console.warn('board error:', msg.error);
  }
}

function send(cmd, args){
  if (state.mode === 'demo') return;
  if (!state.ws || state.ws.readyState !== 1) return;
  state.ws.send(JSON.stringify({ id: state.reqId++, block: 'base', cmd: cmd, args: args }));
}

// ---------------- LED ----------------
function applyLed(led){
  if (!led) return;
  els.swatch.style.background = 'rgb(' + led.r + ',' + led.g + ',' + led.b + ')';
  if (!state.draggingColor) els.colorInput.value = rgbToHex(led.r, led.g, led.b);
  if (!state.draggingBright && typeof led.brightness === 'number') {
    els.brightInput.value = led.brightness; els.brightVal.textContent = led.brightness;
  }
}

var colorThrottle = null;
els.colorInput.addEventListener('input', function(){
  state.draggingColor = true;
  var c = hexToRgb(els.colorInput.value);
  els.swatch.style.background = 'rgb(' + c.r + ',' + c.g + ',' + c.b + ')';
  if (colorThrottle) return;
  colorThrottle = setTimeout(function(){ colorThrottle = null; send('led', c); }, 70);
});
els.colorInput.addEventListener('change', function(){
  state.draggingColor = false;
  send('led', hexToRgb(els.colorInput.value));
});

var brightThrottle = null;
els.brightInput.addEventListener('input', function(){
  state.draggingBright = true;
  var v = parseInt(els.brightInput.value, 10);
  els.brightVal.textContent = v;
  if (brightThrottle) return;
  brightThrottle = setTimeout(function(){ brightThrottle = null; send('brightness', { value: v }); }, 70);
});
els.brightInput.addEventListener('change', function(){
  state.draggingBright = false;
  send('brightness', { value: parseInt(els.brightInput.value, 10) });
});

var PRESETS = ['#ffffff','#ff3b30','#34c759','#0a84ff','#000000'];
PRESETS.forEach(function(hex){
  var b = document.createElement('button');
  b.className = 'preset'; b.style.background = hex; b.setAttribute('aria-label', hex);
  b.addEventListener('click', function(){
    els.colorInput.value = hex;
    var c = hexToRgb(hex);
    els.swatch.style.background = 'rgb(' + c.r + ',' + c.g + ',' + c.b + ')';
    send('led', c);
  });
  els.presets.appendChild(b);
});

// ---------------- reading / chart ----------------
function buildSourceOptions(){
  var hasB1 = state.blocks.indexOf('b1') !== -1;
  var keys = ['base.temp_c'];
  if (hasB1) for (var i=1;i<=8;i++) keys.push('b1.ai'+i);
  els.sourceSelect.innerHTML = '';
  keys.forEach(function(k){
    var o = document.createElement('option');
    o.value = k; o.textContent = SOURCES[k].label;
    els.sourceSelect.appendChild(o);
  });
  if (keys.indexOf(state.source) === -1) state.source = keys[0];
  els.sourceSelect.value = state.source;
}
els.sourceSelect.addEventListener('change', function(){
  state.source = els.sourceSelect.value;
  state.history = [];
});

function applyStatus(blocks){
  var base = blocks.base || {};
  applyLed(base.led);
  els.tUptime.textContent = fmtUptime(base.uptime_s);
  els.tRssi.textContent = (base.rssi ? base.rssi + ' dBm' : '—');
  els.tHeap.textContent = base.heap_free ? Math.round(base.heap_free/1024) + ' kB' : '—';
  els.tClients.textContent = (typeof base.clients === 'number') ? base.clients : '—';

  var parts = state.source.split('.');
  var v = blocks[parts[0]] ? blocks[parts[0]][parts[1]] : undefined;
  if (typeof v === 'number') pushHistory(v);
}

function pushHistory(v){
  state.history.push({ t: Date.now(), v: v });
  if (state.history.length > state.HISTORY_MAX) state.history.shift();
  drawChart();
}

function drawChart(){
  var canvas = els.chart, ctx = canvas.getContext('2d');
  var rect = canvas.getBoundingClientRect();
  var dpr = window.devicePixelRatio || 1;
  var cw = rect.width || 300, ch = 110;
  if (canvas.width !== Math.round(cw*dpr) || canvas.height !== Math.round(ch*dpr)) {
    canvas.width = Math.round(cw*dpr); canvas.height = Math.round(ch*dpr);
  }
  ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,cw,ch);

  var css = getComputedStyle(document.documentElement);
  var amber = css.getPropertyValue('--amber').trim() || '#FFB300';
  var grid = css.getPropertyValue('--edge').trim() || 'rgba(255,255,255,.1)';
  var faint = css.getPropertyValue('--ink-faint').trim() || '#777';

  var hist = state.history;
  var unit = SOURCES[state.source] ? SOURCES[state.source].unit : '';
  els.readoutUnit.textContent = unit;

  if (hist.length === 0) {
    els.readoutVal.textContent = '—';
    els.scaleMin.textContent = els.scaleMid.textContent = els.scaleMax.textContent = '–';
    return;
  }
  var vals = hist.map(function(p){ return p.v; });
  var min = Math.min.apply(null, vals), max = Math.max.apply(null, vals);
  if (max - min < 0.5) { var mid=(max+min)/2; min = mid-0.25; max = mid+0.25; }
  var pad = (max-min)*0.12; min -= pad; max += pad;

  ctx.strokeStyle = grid; ctx.lineWidth = 1;
  for (var g=0; g<3; g++){
    var gy = ch * (g/2);
    ctx.beginPath(); ctx.moveTo(0, gy+0.5); ctx.lineTo(cw, gy+0.5); ctx.stroke();
  }

  var n = hist.length;
  ctx.beginPath();
  hist.forEach(function(p, idx){
    var x = n>1 ? (idx/(n-1))*cw : cw;
    var y = ch - ((p.v - min)/(max-min))*ch;
    if (idx===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  });
  var grad = ctx.createLinearGradient(0,0,0,ch);
  grad.addColorStop(0, amber); grad.addColorStop(1, amber);
  ctx.strokeStyle = amber; ctx.lineWidth = 2; ctx.lineJoin='round'; ctx.stroke();

  var last = hist[n-1].v;
  els.readoutVal.textContent = (unit === '°C' ? last.toFixed(1) : last.toFixed(2));
  els.scaleMax.textContent = max.toFixed(1);
  els.scaleMid.textContent = ((max+min)/2).toFixed(1);
  els.scaleMin.textContent = min.toFixed(1);
}
window.addEventListener('resize', drawChart);

// ---------------- demo mode ----------------
function startDemo(){
  stopDemo();
  clearTimeout(state.reconnectTimer);
  if (state.ws) { try { state.ws.close(); } catch(e){} state.ws = null; }
  state.mode = 'demo';
  state.blocks = ['base','b1'];
  buildSourceOptions();
  setConn('demo', 'demo');
  els.demoBanner.hidden = false;
  els.ledPanel.hidden = false; els.readingPanel.hidden = false; els.telemetryPanel.hidden = false;
  els.statusHint.textContent = 'Enter your board’s address on the same WiFi network.';
  applyLed({ r:255, g:179, b:0, brightness:120 });
  state.demoT = 0;
  state.demoTimer = setInterval(function(){
    state.demoT += 0.12;
    var base = {
      temp_c: 24 + 2.4*Math.sin(state.demoT/3) + (Math.random()-0.5)*0.3,
      uptime_s: Math.floor(state.demoT*40),
      rssi: -58 + Math.round((Math.random()-0.5)*4),
      heap_free: 214000 + Math.round((Math.random()-0.5)*4000),
      clients: 1,
      led: hexToRgb(els.colorInput.value)
    };
    base.led.brightness = parseInt(els.brightInput.value,10);
    var b1 = {};
    for (var i=1;i<=8;i++) b1['ai'+i] = 2.5 + 1.5*Math.sin(state.demoT/2 + i) + (Math.random()-0.5)*0.1;
    applyStatus({ base: base, b1: b1 });
  }, 120);
}
function stopDemo(){
  if (state.demoTimer) { clearInterval(state.demoTimer); state.demoTimer = null; }
  if (state.mode === 'demo') { state.mode = 'idle'; els.demoBanner.hidden = true; }
}

els.connectBtn.addEventListener('click', connect);
els.addrInput.addEventListener('keydown', function(e){ if (e.key === 'Enter') connect(); });
els.demoBtn.addEventListener('click', function(){
  if (state.mode === 'demo') { stopDemo(); setConn('', 'offline'); els.ledPanel.hidden = els.readingPanel.hidden = els.telemetryPanel.hidden = true; els.demoBtn.classList.remove('active'); }
  else { startDemo(); els.demoBtn.classList.add('active'); }
});

// Auto-try a saved address once on load (quietly; falls back to the connect screen).
buildSourceOptionsInitial();
function buildSourceOptionsInitial(){ state.blocks = []; buildSourceOptions(); }
if (loadAddr()) { setConn('', 'offline'); }

// ?demo=1 auto-starts demo mode (used by the project site's "try it" link).
if (/[?&]demo=1\b/.test(location.search)) {
  startDemo();
  els.demoBtn.classList.add('active');
}
