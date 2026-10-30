/*
  pagina_web.h
  Interfaz de control (HTML + CSS + JS en un solo texto, sin librerías
  externas porque la red del carrito no tiene internet).
    1. Encendido de motores y variables de estado
    2. Gráficas: ángulo (set point y medido) y RPM (set point y medida
       de cada motor, con selector Ambos / Motor 1 / Motor 2)
    3. Constantes de los controladores de ángulo, giro y desplazamiento
    4. Voltaje de la batería (ingreso manual)
  Las secciones 2, 3 y 4 se pueden contraer.
*/
#pragma once

const char PAGINA_WEB[] = R"rawliteral(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Carrito Balancín</title>
<style>
:root{--fondo:#f4f5f7;--tarjeta:#fff;--texto:#1d2129;--sub:#6b7280;--borde:#dde1e6;--rejilla:#eceef1;
--ok:#15803d;--okf:#dcfce7;--mal:#b91c1c;--malf:#fee2e2;--aviso:#a16207;--avisof:#fef3c7;
--a:#2563eb;--b:#ea580c;--ref:#6b7280}
@media (prefers-color-scheme:dark){:root{--fondo:#111418;--tarjeta:#1b1f24;--texto:#e6e8eb;--sub:#9aa3ad;
--borde:#2c323a;--rejilla:#252a31;--ok:#4ade80;--okf:#14301f;--mal:#f87171;--malf:#3a1717;--aviso:#facc15;
--avisof:#3a3112;--a:#60a5fa;--b:#fb923c;--ref:#9aa3ad}}
*{box-sizing:border-box}
body{margin:0;background:var(--fondo);color:var(--texto);font:15px/1.4 system-ui,sans-serif}
main{max-width:760px;margin:0 auto;padding:12px 16px 32px}
h1{font-size:18px;margin:4px 0 12px}
.tarjeta{background:var(--tarjeta);border:1px solid var(--borde);border-radius:10px;padding:12px;margin-bottom:12px}
summary{cursor:pointer;font-weight:600}
details[open] summary{margin-bottom:10px}
#btn{width:100%;padding:12px;font-size:16px;font-weight:600;border:0;border-radius:8px;color:#fff;background:var(--ok);cursor:pointer}
#btn.on{background:var(--mal)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}
.chip{font-size:12px;padding:2px 8px;border-radius:999px;background:var(--rejilla);color:var(--sub)}
.chip.ok{background:var(--okf);color:var(--ok)}.chip.mal{background:var(--malf);color:var(--mal)}
.chip.aviso{background:var(--avisof);color:var(--aviso)}
.chip.parpadeo{animation:parpadeo 1s steps(2,start) infinite}
@keyframes parpadeo{to{visibility:hidden}}
#btn:disabled{opacity:.5;cursor:not-allowed}
.cab{display:flex;align-items:center;justify-content:space-between;gap:8px;font-size:13px;margin:6px 0 4px}
.ley{display:flex;flex-wrap:wrap;gap:10px;color:var(--sub);font-size:12px}
.ley i{display:inline-block;width:14px;height:0;border-top:2px solid;vertical-align:middle;margin-right:4px}
.ley i.d{border-top-style:dashed}
canvas{width:100%;height:190px;display:block}
select,input{font:inherit;font-size:13px;color:var(--texto);background:var(--tarjeta);border:1px solid var(--borde);border-radius:6px;padding:3px 6px}
fieldset{border:1px solid var(--borde);border-radius:8px;margin:0 0 10px;padding:8px 10px}
legend{font-size:13px;color:var(--sub);padding:0 4px}
.campos{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px}
label{font-size:13px;color:var(--sub);display:flex;flex-direction:column;gap:2px}
input{width:100%}
.fila{display:flex;align-items:center;gap:10px}
.aplicar{padding:8px 16px;border:0;border-radius:8px;background:var(--a);color:#fff;font-weight:600;cursor:pointer}
.msg{font-size:13px;color:var(--sub)}
.bateria{max-width:160px}
#fBat{align-items:flex-end}
</style>
</head>
<body>
<main>
<h1>Carrito Balancín</h1>

<section class="tarjeta">
  <button id="btn">Encender motores</button>
  <div class="chips">
    <span class="chip" id="cRed">Conectando…</span>
    <span class="chip" id="cMpu">MPU</span>
    <span class="chip" id="cAng">Ángulo</span>
    <span class="chip" id="cEst">Estado</span>
    <span class="chip" id="cCal" hidden></span>
  </div>
</section>

<details class="tarjeta" open>
  <summary>Gráficas (últimos 10 s)</summary>
  <div class="cab"><b>Ángulo (°)</b>
    <span class="ley"><span><i class="d" style="color:var(--ref)"></i>Set point</span><span><i style="color:var(--a)"></i>Medido</span></span>
  </div>
  <canvas id="gAng"></canvas>
  <div class="cab"><b>Velocidad (rpm)</b>
    <select id="sel"><option value="3">Ambos</option><option value="1">Motor 1</option><option value="2">Motor 2</option></select>
  </div>
  <div class="ley" id="leyRpm"></div>
  <canvas id="gRpm"></canvas>
</details>

<details class="tarjeta" open>
  <summary>Constantes de los controladores</summary>
  <form id="fParam">
    <fieldset><legend>Ángulo (PID)</legend><div class="campos">
      <label>Kp (rpm/°)<input name="kpa" type="number" step="any" inputmode="decimal"></label>
      <label>Ki (rpm/(°·s))<input name="kia" type="number" step="any" inputmode="decimal"></label>
      <label>Kd (rpm/(°/s))<input name="kda" type="number" step="any" inputmode="decimal"></label>
      <label>Ajuste sensor (°)<input name="aja" type="number" step="any" inputmode="decimal"></label>
    </div></fieldset>
    <fieldset><legend>Giro (P / PI)</legend><div class="campos">
      <label>Kp (rpm/(°/s))<input name="kpg" type="number" step="any" inputmode="decimal"></label>
      <label>Ki (rpm/°)<input name="kig" type="number" step="any" inputmode="decimal"></label>
    </div></fieldset>
    <fieldset><legend>Desplazamiento (PID)</legend><div class="campos">
      <label>Kp (°/m)<input name="kpx" type="number" step="any" inputmode="decimal"></label>
      <label>Ki (°/(m·s))<input name="kix" type="number" step="any" inputmode="decimal"></label>
      <label>Kd (°/(m/s))<input name="kdx" type="number" step="any" inputmode="decimal"></label>
      <label>Set point (m)<input name="xref" type="number" step="any" inputmode="decimal"></label>
    </div></fieldset>
    <div class="fila"><button class="aplicar" type="submit">Aplicar</button><span class="msg" id="msg"></span></div>
  </form>
</details>

<details class="tarjeta" open>
  <summary>Batería</summary>
  <form id="fBat" class="fila">
    <label class="bateria">Voltaje (V)<input name="v" type="number" step="0.01" inputmode="decimal"></label>
    <button class="aplicar" type="submit">Aplicar</button><span class="msg" id="msgBat"></span>
  </form>
</details>
</main>

<script>
const N = 500;  // muestras visibles: 10 s a 20 ms
const d = {ar:[], a:[], r1:[], w1:[], r2:[], w2:[]};
const claves = ['ar','a','r1','w1','r2','w2'];
let hab = false, calAnterior = 0;
const $ = id => document.getElementById(id);
const css = v => getComputedStyle(document.documentElement).getPropertyValue(v).trim();

function agregar(m) {
  claves.forEach((k, i) => { d[k].push(m[i]); if (d[k].length > N) d[k].shift(); });
}

function graficar(cv, series) {
  const dpr = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight;
  if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
  const g = cv.getContext('2d');
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, w, h);

  let lo = Infinity, hi = -Infinity;
  series.forEach(s => s.y.forEach(v => { if (v < lo) lo = v; if (v > hi) hi = v; }));
  if (!isFinite(lo)) { lo = -1; hi = 1; }
  if (hi - lo < 1) { const c = (hi + lo) / 2; lo = c - 0.5; hi = c + 0.5; }
  const pad = (hi - lo) * 0.1; lo -= pad; hi += pad;

  const L = 44, R = 6, T = 6, B = 18, pw = w - L - R, ph = h - T - B;
  const yPx = v => T + ph - (v - lo) / (hi - lo) * ph;
  g.font = '11px system-ui,sans-serif'; g.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const v = lo + (hi - lo) * i / 4, y = yPx(v);
    g.strokeStyle = css('--rejilla'); g.beginPath(); g.moveTo(L, y); g.lineTo(L + pw, y); g.stroke();
    g.fillStyle = css('--sub'); g.textAlign = 'right'; g.fillText(v.toFixed(1), L - 4, y + 4);
  }
  g.textAlign = 'center';
  [-10, -5, 0].forEach(s => g.fillText(s + ' s', L + pw * (1 + s / 10), h - 4));

  series.forEach(s => {
    const n = s.y.length;
    if (!n) return;
    g.strokeStyle = s.c; g.lineWidth = s.d ? 1.5 : 2; g.setLineDash(s.d ? [5, 4] : []);
    g.beginPath();
    s.y.forEach((v, i) => {
      const x = L + pw * (N - n + i) / (N - 1), y = yPx(v);
      i ? g.lineTo(x, y) : g.moveTo(x, y);
    });
    g.stroke();
  });
  g.setLineDash([]);
}

function dibujar() {
  graficar($('gAng'), [{y:d.ar, c:css('--ref'), d:1}, {y:d.a, c:css('--a')}]);
  const sel = +$('sel').value, s = [], ley = [];
  if (sel & 1) { s.push({y:d.r1, c:css('--a'), d:1}, {y:d.w1, c:css('--a')}); ley.push(['--a','d','Ref M1'], ['--a','','M1']); }
  if (sel & 2) { s.push({y:d.r2, c:css('--b'), d:1}, {y:d.w2, c:css('--b')}); ley.push(['--b','d','Ref M2'], ['--b','','M2']); }
  $('leyRpm').innerHTML = ley.map(l => `<span><i class="${l[1]}" style="color:var(${l[0]})"></i>${l[2]}</span>`).join('');
  graficar($('gRpm'), s);
}

function chip(id, txt, clase) { const c = $(id); c.textContent = txt; c.className = 'chip ' + clase; }

function mostrarEstado(j) {
  hab = !!j.hab;
  $('btn').textContent = hab ? 'Apagar motores' : 'Encender motores';
  $('btn').className = hab ? 'on' : '';
  chip('cMpu', j.mpu ? 'MPU con señal' : 'MPU sin señal', j.mpu ? 'ok' : 'mal');
  chip('cAng', j.lim ? 'Ángulo en límites' : 'Ángulo fuera de límites', j.lim ? 'ok' : 'mal');
  // mot: 0 controlando, 1 apagado, 2 esperando vertical, 3 caída, 4 falla MPU, 5 calibrando
  chip('cEst', j.txt, j.mot == 0 ? 'ok' : (j.mot == 3 || j.mot == 4 ? 'mal' : 'aviso'));

  // cal: 0 inactiva, 1 espera, 2 muestreo, 3 ok, 4 movimiento, 5 MPU, 6 flash, 7 rechazada
  const enCurso = j.cal == 1 || j.cal == 2;
  $('btn').disabled = enCurso;
  $('cCal').hidden = j.cal == 0;
  chip('cCal', j.ctxt, enCurso ? 'aviso parpadeo' : (j.cal == 3 ? 'ok' : 'mal'));
  // Al terminar bien, el ajuste del sensor cambió: se recarga el formulario
  if (j.cal == 3 && calAnterior != 3) pedir('/parametros').then(llenarParametros).catch(() => {});
  calAnterior = j.cal;
}

async function pedir(url, opciones) {
  const ctl = new AbortController(), t = setTimeout(() => ctl.abort(), 2000);
  try { const r = await fetch(url, Object.assign({signal: ctl.signal}, opciones)); return await r.json(); }
  finally { clearTimeout(t); }
}

async function ciclo() {
  try {
    const j = await pedir('/datos');
    mostrarEstado(j);
    j.m.forEach(agregar);
    dibujar();
    chip('cRed', 'Conectado', 'ok');
  } catch (e) {
    chip('cRed', 'Sin conexión', 'mal');
  }
  setTimeout(ciclo, 200);
}

function llenarParametros(p) {
  const f = $('fParam');
  ['kpa','kia','kda','aja','kpg','kig','kpx','kix','kdx','xref'].forEach(k => f[k].value = p[k]);
}

$('btn').onclick = async () => {
  try { await pedir('/motores', {method:'POST', body:new URLSearchParams({on: hab ? 0 : 1})}); } catch (e) {}
};

$('fParam').onsubmit = async ev => {
  ev.preventDefault();
  try {
    llenarParametros(await pedir('/parametros', {method:'POST', body:new URLSearchParams(new FormData(ev.target))}));
    $('msg').textContent = 'Aplicado';
  } catch (e) {
    $('msg').textContent = 'No se pudo aplicar';
  }
  setTimeout(() => $('msg').textContent = '', 2500);
};

function llenarBateria(b) {
  const f = $('fBat');
  f.v.value = b.v; f.v.min = b.min; f.v.max = b.max;
}

$('fBat').onsubmit = async ev => {
  ev.preventDefault();
  try {
    const b = await pedir('/bateria', {method:'POST', body:new URLSearchParams(new FormData(ev.target))});
    llenarBateria(b);
    $('msgBat').textContent = b.ok ? 'Aplicado' : `Fuera de rango (${b.min}–${b.max} V)`;
  } catch (e) {
    $('msgBat').textContent = 'No se pudo aplicar';
  }
  setTimeout(() => $('msgBat').textContent = '', 3000);
};

$('sel').onchange = dibujar;
window.onresize = dibujar;
pedir('/parametros').then(llenarParametros).catch(() => {});
pedir('/bateria').then(llenarBateria).catch(() => {});
dibujar();
ciclo();
</script>
</body>
</html>
)rawliteral";
