#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "DHT.h"
#include "esp_wifi.h"

// --- CONFIGURACIÓN DE HARDWARE ---
#define DHTPIN 4
#define DHTTYPE DHT11
const int PIN_LDR  = 32;
const int PIN_SOIL = 34;
const float SOIL_DRY_RAW = 4095.0;
const float SOIL_WET_RAW = 1200.0;
const int NUM_ADC_SAMPLES = 16;

// --- OBJETOS ---
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

// --- NTP UTC-5 Colombia/Bogotá ---
const long GMT_OFFSET_SEC      = -5L * 3600L;
const int  DAYLIGHT_OFFSET_SEC = 0;

// --- HISTORIAL ---
struct HistoryRecord {
    char timeStr[9];
    unsigned long timestampSec;
    float tempAvg,  tempMin,  tempMax;
    float humAvg,   humMin,   humMax;
    float sueloAvg, sueloMin, sueloMax;
    float luzAvg,   luzMin,   luzMax;
    int samplesCount;
};
const int MAX_HISTORY_RECORDS = 48;
HistoryRecord historyBuffer[MAX_HISTORY_RECORDS];
int historyHead = 0, historyCount = 0;

// --- ACUMULADORES ---
const unsigned long HISTORY_INTERVAL_MS = 600000UL;
unsigned long lastHistoryIntervalStart = 0;
double accumTemp=0,accumHum=0,accumSuelo=0,accumLuz=0;
float wTMin=999,wTMax=-999,wHMin=999,wHMax=-999;
float wSMin=999,wSMax=-999,wLMin=999,wLMax=-999;
int wCount=0;

// --- CACHE SENSORES ---
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 3000;
float cTemp=0,cHum=0,cSuelo=0;
int   cLuz=0;
bool  cSensorOk=false;

// =========================================================
// HTML COMPLETO — Una sola constante PROGMEM, ~9KB
// El browser carga Chart.js desde CDN en paralelo sin bloquear
// el WebServer porque ese request va a internet, no al ESP32.
// =========================================================
const char HTML_INDEX[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="es"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HidroFlow Gamma</title>
<style>
:root{--bg:#0d1117;--card:#161b22;--brd:#30363d;--txt:#e6edf3;--sec:#8b949e;--blu:#58a6ff;--grn:#3fb950;--red:#f85149;--ylw:#d29922;--org:#d97706;--shd:0 4px 12px rgba(0,0,0,.3);--mbg:rgba(13,17,23,.85)}
[data-theme=light]{--bg:#f6f8fa;--card:#fff;--brd:#d0d7de;--txt:#1f2328;--sec:#656d76;--blu:#0969da;--grn:#1a7f37;--red:#cf222e;--ylw:#9a6700;--org:#b45309;--shd:0 4px 12px rgba(0,0,0,.08);--mbg:rgba(246,248,250,.85)}
*{box-sizing:border-box;margin:0;padding:0;transition:background .25s,color .25s,border-color .25s}
body{background:var(--bg);color:var(--txt);font-family:system-ui,sans-serif;display:flex;flex-direction:column;align-items:center;min-height:100vh;padding:15px}
.wrap{width:100%;max-width:780px}
header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;flex-wrap:wrap;gap:10px}
h1{color:var(--blu);font-size:1.5rem;font-weight:700}
.hdr{display:flex;align-items:center;gap:10px}
.tbtn,.badge{background:var(--card);border:1px solid var(--brd);border-radius:20px;font-size:.85rem;padding:5px 13px}
.tbtn{color:var(--txt);cursor:pointer}.tbtn:hover{border-color:var(--blu)}
.badge{color:var(--sec);display:flex;align-items:center;gap:6px}.badge.on{color:var(--grn);border-color:var(--grn)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--red)}.badge.on .dot{background:var(--grn)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:14px}
.card{background:var(--card);border:1px solid var(--brd);border-radius:12px;padding:18px;text-align:center;box-shadow:var(--shd);display:flex;flex-direction:column;align-items:center}
.ci{font-size:1.6rem;margin-bottom:4px}.cl{font-size:.75rem;color:var(--sec);text-transform:uppercase;letter-spacing:.5px;margin:4px 0}
.val{font-size:1.6rem;font-weight:700;margin:4px 0}
.tc{color:var(--red)}.th{color:var(--blu)}.ts{color:var(--grn)}.tl{color:var(--ylw)}
.hs{font-size:.75rem;font-weight:700;padding:3px 10px;border-radius:12px;margin-top:6px;display:inline-block}
.ok{background:rgba(63,185,80,.15);color:var(--grn);border:1px solid var(--grn)}
.wn{background:rgba(217,119,6,.15);color:var(--org);border:1px solid var(--org)}
.dg{background:rgba(248,81,73,.15);color:var(--red);border:1px solid var(--red);animation:pulse 1.8s infinite}
.inf{background:rgba(88,166,255,.15);color:var(--blu);border:1px solid var(--blu)}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.6}}
.actions{display:flex;justify-content:center;gap:12px;margin-top:24px;flex-wrap:wrap}
.btn{padding:10px 18px;border:none;border-radius:8px;font-weight:600;font-size:.9rem;cursor:pointer;display:inline-flex;align-items:center;gap:6px;box-shadow:var(--shd)}
.bp{background:var(--blu);color:#fff}.bd{background:var(--red);color:#fff}
footer{margin-top:25px;font-size:.78rem;color:var(--sec);text-align:center}
.mo{position:fixed;top:0;left:0;width:100%;height:100%;background:var(--mbg);backdrop-filter:blur(6px);display:none;align-items:center;justify-content:center;padding:15px;z-index:1000}
.mo.active{display:flex}
.mc{background:var(--card);border:1px solid var(--brd);border-radius:16px;width:100%;max-width:760px;max-height:90vh;display:flex;flex-direction:column;box-shadow:var(--shd);overflow:hidden}
.mh{padding:16px 20px;border-bottom:1px solid var(--brd);display:flex;justify-content:space-between;align-items:center}
.mt{font-size:1.1rem;font-weight:700;color:var(--blu)}.xb{background:none;border:none;color:var(--sec);font-size:1.4rem;cursor:pointer}
.mb{padding:16px;overflow-y:auto;flex:1;display:flex;flex-direction:column;gap:14px}
.cc{position:relative;width:100%;height:200px;background:var(--bg);border:1px solid var(--brd);border-radius:10px;padding:8px}
.ht{width:100%;border-collapse:collapse;font-size:.83rem}
.ht th,.ht td{padding:9px 11px;border-bottom:1px solid var(--brd);text-align:left}
.ht th{color:var(--sec);font-weight:600;background:var(--bg);position:sticky;top:0}
.sv{font-size:.72rem;color:var(--sec)}
</style></head>
<body>
<div class="wrap">
<header>
<h1>HidroFlow <span style="font-size:.8rem;font-weight:400;opacity:.8">Gamma</span></h1>
<div class="hdr">
<button class="tbtn" id="themeBtn" onclick="toggleTheme()">🌙 Modo Oscuro</button>
<div class="badge" id="b"><span class="dot"></span><span id="st">Conectando...</span></div>
</div></header>
<div class="grid">
<div class="card"><div class="ci">🌱</div><div class="cl">Hum. Suelo</div><div class="val ts" id="suelo">-- %</div><div class="hs inf" id="stS">Evaluando...</div></div>
<div class="card"><div class="ci">🌡️</div><div class="cl">Temperatura</div><div class="val tc" id="temp">-- °C</div><div class="hs inf" id="stT">Evaluando...</div></div>
<div class="card"><div class="ci">💧</div><div class="cl">Hum. Ambiente</div><div class="val th" id="hum">-- %</div><div class="hs inf" id="stH">Evaluando...</div></div>
<div class="card"><div class="ci">☀️</div><div class="cl">Luz Solar</div><div class="val tl" id="luz">-- %</div><div class="hs inf" id="stL">Evaluando...</div></div>
</div>
<div class="actions">
<button class="btn bp" onclick="openHistory()">📊 Gráfica e Historial</button>
<button class="btn bd" onclick="resetWifi()">⚙️ Reconfigurar Wi-Fi</button>
</div>
<footer>HidroFlow Gamma v0.01.00 &bull; Monitoreo Agrícola Inteligente</footer>
</div>
<div class="mo" id="histM">
<div class="mc">
<div class="mh"><div class="mt">📊 Tendencia e Historial (promedios 10 min)</div><button class="xb" onclick="closeHistory()">&times;</button></div>
<div class="mb">
<div class="cc"><canvas id="tc"></canvas></div>
<table class="ht"><thead><tr><th>Hora / Hace</th><th>Temp °C</th><th>Hum Amb %</th><th>Suelo %</th><th>Luz %</th></tr></thead>
<tbody id="hb"><tr><td colspan="5" style="text-align:center;padding:20px;color:var(--sec)">Cargando...</td></tr></tbody>
</table></div></div></div>
<script>
// Chart.js se carga async desde CDN — no bloquea el servidor ESP32
var s=document.createElement('script');
s.src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js';
document.head.appendChild(s);
var CI=null;
function applyTheme(t){document.documentElement.setAttribute('data-theme',t);document.getElementById('themeBtn').innerHTML=t==='light'?'☀️ Modo Claro':'🌙 Modo Oscuro';if(CI)upC();}
function toggleTheme(){var n=(localStorage.getItem('theme')||'dark')==='dark'?'light':'dark';localStorage.setItem('theme',n);applyTheme(n);}
applyTheme(localStorage.getItem('theme')||'dark');
function sH(id,txt,cls){var e=document.getElementById(id);e.innerText=txt;e.className='hs '+cls;}
function evalH(d){
  var s=d.suelo;
  if(s<20)sH('stS','¡Riego Urgente!','dg');else if(s<40)sH('stS','Humedad Baja','wn');else if(s<=75)sH('stS','Óptimo','ok');else sH('stS','Saturado','inf');
  if(!d.sensor_ok){sH('stT','Sin Datos','wn');sH('stH','Sin Datos','wn');}
  else{var t=d.temperatura;if(t<15)sH('stT','Frío','inf');else if(t<=30)sH('stT','Óptimo','ok');else sH('stT','Calor Elevado','dg');
    var h=d.humedad_amb;if(h<30)sH('stH','Ambiente Seco','wn');else if(h<=70)sH('stH','Óptimo','ok');else sH('stH','Ambiente Húmedo','inf');}
  var l=d.luz;if(l<20)sH('stL','Sombra','inf');else if(l<=80)sH('stL','Luz Adecuada','ok');else sH('stL','Sol Intenso','wn');
}
async function updateData(){
  var ctrl=new AbortController(),tid=setTimeout(function(){ctrl.abort();},3500);
  try{var r=await fetch('/api/data',{signal:ctrl.signal});clearTimeout(tid);if(!r.ok)throw 0;
    var d=await r.json();
    document.getElementById('temp').innerText=d.sensor_ok?d.temperatura.toFixed(1)+' °C':'Error DHT';
    document.getElementById('hum').innerText=d.sensor_ok?d.humedad_amb.toFixed(1)+' %':'Error DHT';
    document.getElementById('suelo').innerText=d.suelo.toFixed(1)+' %';
    document.getElementById('luz').innerText=d.luz+' %';
    evalH(d);document.getElementById('b').className='badge on';document.getElementById('st').innerText='En línea';
  }catch(e){clearTimeout(tid);document.getElementById('b').className='badge';document.getElementById('st').innerText='Reintentando...';}
}
function upC(){var lt=document.documentElement.getAttribute('data-theme')==='light',gc=lt?'rgba(0,0,0,.06)':'rgba(255,255,255,.08)',tc=lt?'#1f2328':'#8b949e';
  ['x','y'].forEach(function(a){CI.options.scales[a].ticks.color=tc;CI.options.scales[a].grid.color=gc;});CI.options.plugins.legend.labels.color=tc;CI.update();}
function mkChart(list){
  var rev=list.slice().reverse(),labels=rev.map(function(i){return i.hr||'Hace '+i.ma+'m';}),
    lt=document.documentElement.getAttribute('data-theme')==='light',gc=lt?'rgba(0,0,0,.06)':'rgba(255,255,255,.08)',tc=lt?'#1f2328':'#8b949e';
  var ds=[
    {label:'Suelo %',data:rev.map(function(i){return i.suelo.avg;}),borderColor:'#3fb950',backgroundColor:'rgba(63,185,80,.1)',tension:.3,borderWidth:2,pointRadius:2},
    {label:'Temp °C',data:rev.map(function(i){return i.temp.avg;}),borderColor:'#f85149',backgroundColor:'transparent',tension:.3,borderWidth:2,pointRadius:2},
    {label:'Hum Amb %',data:rev.map(function(i){return i.hum.avg;}),borderColor:'#58a6ff',backgroundColor:'transparent',tension:.3,borderWidth:2,pointRadius:2},
    {label:'Luz %',data:rev.map(function(i){return i.luz.avg;}),borderColor:'#d29922',backgroundColor:'transparent',tension:.3,borderWidth:2,pointRadius:2}
  ];
  var opts={responsive:true,maintainAspectRatio:false,plugins:{legend:{labels:{color:tc,font:{size:10}}}},scales:{x:{ticks:{color:tc,font:{size:9}},grid:{color:gc}},y:{ticks:{color:tc,font:{size:9}},grid:{color:gc}}}};
  if(CI){CI.data.labels=labels;CI.data.datasets.forEach(function(d,i){d.data=ds[i].data;});CI.options=opts;CI.update();}
  else{CI=new Chart(document.getElementById('tc').getContext('2d'),{type:'line',data:{labels:labels,datasets:ds},options:opts});}
}
async function openHistory(){
  document.getElementById('histM').classList.add('active');
  var tb=document.getElementById('hb');tb.innerHTML='<tr><td colspan="5" style="text-align:center;padding:20px">Cargando...</td></tr>';
  try{var r=await fetch('/api/history');if(!r.ok)throw 0;var list=await r.json();
    if(!list.length){tb.innerHTML='<tr><td colspan="5" style="text-align:center;padding:20px;color:var(--sec)">Sin registros. El primero aparece en 10 min.</td></tr>';return;}
    if(typeof Chart!=='undefined')mkChart(list);
    tb.innerHTML=list.map(function(i){return'<tr><td><b>'+(i.hr||'--:--')+'</b><br><span class="sv">Hace '+i.ma+'m</span></td><td><b>'+i.temp.avg.toFixed(1)+'°C</b><br><span class="sv">↓'+i.temp.min.toFixed(1)+' ↑'+i.temp.max.toFixed(1)+'</span></td><td><b>'+i.hum.avg.toFixed(1)+'%</b><br><span class="sv">↓'+i.hum.min.toFixed(1)+' ↑'+i.hum.max.toFixed(1)+'</span></td><td><b>'+i.suelo.avg.toFixed(1)+'%</b><br><span class="sv">↓'+i.suelo.min.toFixed(1)+' ↑'+i.suelo.max.toFixed(1)+'</span></td><td><b>'+i.luz.avg.toFixed(0)+'%</b><br><span class="sv">↓'+i.luz.min.toFixed(0)+' ↑'+i.luz.max.toFixed(0)+'</span></td></tr>';}).join('');
  }catch(e){tb.innerHTML='<tr><td colspan="5" style="text-align:center;padding:20px;color:var(--red)">Error al obtener historial.</td></tr>';}
}
function closeHistory(){document.getElementById('histM').classList.remove('active');}
async function resetWifi(){if(confirm('¿Borrar Wi-Fi y reiniciar portal?')){try{await fetch('/reset_wifi');}catch(e){}alert('Reinicio iniciado. Conéctate a ESP32_Config_AP');}}
updateData();setInterval(updateData,5000);
</script></body></html>)HTML";

// =========================================================
//  SENSORES, NVS, HISTORIAL
// =========================================================
int readAnalogAverage(int pin, int samples) {
    long sum=0;
    for(int i=0;i<samples;i++){sum+=analogRead(pin);delayMicroseconds(100);}
    return sum/samples;
}

void saveHistoryToNVS() {
    Preferences prefs;
    prefs.begin("hidro_h",false);
    prefs.putBytes("buf",historyBuffer,sizeof(historyBuffer));
    prefs.putInt("head",historyHead);
    prefs.putInt("cnt",historyCount);
    prefs.end();
    Serial.println("💾 Historial guardado en NVS.");
}

void loadHistoryFromNVS() {
    Preferences prefs;
    prefs.begin("hidro_h",true);
    historyHead =prefs.getInt("head",0);
    historyCount=prefs.getInt("cnt",0);
    if(historyCount>0) prefs.getBytes("buf",historyBuffer,sizeof(historyBuffer));
    prefs.end();
    Serial.printf("📦 NVS: %d registros recuperados.\n",historyCount);
}

void updateAccum(float t,float h,float s,float l){
    accumTemp+=t;accumHum+=h;accumSuelo+=s;accumLuz+=l;
    if(t<wTMin)wTMin=t;if(t>wTMax)wTMax=t;
    if(h<wHMin)wHMin=h;if(h>wHMax)wHMax=h;
    if(s<wSMin)wSMin=s;if(s>wSMax)wSMax=s;
    if(l<wLMin)wLMin=l;if(l>wLMax)wLMax=l;
    wCount++;
}

void processTenMin(){
    unsigned long now=millis();
    if(lastHistoryIntervalStart==0){lastHistoryIntervalStart=now;return;}
    if(now-lastHistoryIntervalStart<HISTORY_INTERVAL_MS)return;
    if(wCount>0){
        HistoryRecord& r=historyBuffer[historyHead];
        r.timestampSec=now/1000;
        struct tm ti;
        if(getLocalTime(&ti))strftime(r.timeStr,9,"%H:%M:%S",&ti);
        else strncpy(r.timeStr,"--:--:--",9);
        r.tempAvg=accumTemp/wCount; r.tempMin=wTMin; r.tempMax=wTMax;
        r.humAvg=accumHum/wCount;  r.humMin=wHMin;  r.humMax=wHMax;
        r.sueloAvg=accumSuelo/wCount;r.sueloMin=wSMin;r.sueloMax=wSMax;
        r.luzAvg=accumLuz/wCount;  r.luzMin=wLMin;  r.luzMax=wLMax;
        r.samplesCount=wCount;
        historyHead=(historyHead+1)%MAX_HISTORY_RECORDS;
        if(historyCount<MAX_HISTORY_RECORDS)historyCount++;
        saveHistoryToNVS();
    }
    lastHistoryIntervalStart=now;
    accumTemp=0;accumHum=0;accumSuelo=0;accumLuz=0;
    wTMin=999;wTMax=-999;wHMin=999;wHMax=-999;
    wSMin=999;wSMax=-999;wLMin=999;wLMax=-999;
    wCount=0;
}

void readSensorsAsync(){
    unsigned long now=millis();
    if(now-lastSensorRead<SENSOR_INTERVAL&&lastSensorRead!=0)return;
    lastSensorRead=now;
    float h=dht.readHumidity(),t=dht.readTemperature();
    if(isnan(h)||isnan(t)||t<0||t>50||h<0||h>100){cTemp=0;cHum=0;cSensorOk=false;}
    else{cTemp=t;cHum=h;cSensorOk=true;}
    cLuz  =constrain(map(readAnalogAverage(PIN_LDR, NUM_ADC_SAMPLES),0,4095,0,100),0,100);
    cSuelo=constrain(((SOIL_DRY_RAW-(float)readAnalogAverage(PIN_SOIL,NUM_ADC_SAMPLES))/(SOIL_DRY_RAW-SOIL_WET_RAW))*100.0f,0.0f,100.0f);
    if(cSensorOk)updateAccum(cTemp,cHum,cSuelo,(float)cLuz);
    processTenMin();
}

// =========================================================
//  HANDLERS
// =========================================================
void handleRoot(){
    server.sendHeader("Cache-Control","no-cache");
    server.send_P(200,"text/html",HTML_INDEX);
}

void handleApiData(){
    StaticJsonDocument<192> doc;
    doc["luz"]=cLuz; doc["suelo"]=cSuelo;
    doc["temperatura"]=cTemp; doc["humedad_amb"]=cHum;
    doc["sensor_ok"]=cSensorOk;
    String js; serializeJson(doc,js);
    server.sendHeader("Access-Control-Allow-Origin","*");
    server.sendHeader("Cache-Control","no-cache");
    server.send(200,"application/json",js);
}

void handleApiHistory(){
    String js; js.reserve(historyCount*290+4);
    js="[";
    unsigned long nowSec=millis()/1000;
    for(int i=0;i<historyCount;i++){
        int idx=(historyHead-1-i+MAX_HISTORY_RECORDS)%MAX_HISTORY_RECORDS;
        HistoryRecord& r=historyBuffer[idx];
        char item[295];
        snprintf(item,sizeof(item),
            "%s{\"ma\":%lu,\"hr\":\"%s\","
            "\"temp\":{\"avg\":%.1f,\"min\":%.1f,\"max\":%.1f},"
            "\"hum\":{\"avg\":%.1f,\"min\":%.1f,\"max\":%.1f},"
            "\"suelo\":{\"avg\":%.1f,\"min\":%.1f,\"max\":%.1f},"
            "\"luz\":{\"avg\":%.0f,\"min\":%.0f,\"max\":%.0f}}",
            (i>0?",":""),
            (nowSec-r.timestampSec)/60,r.timeStr,
            r.tempAvg,r.tempMin,r.tempMax,
            r.humAvg,r.humMin,r.humMax,
            r.sueloAvg,r.sueloMin,r.sueloMax,
            r.luzAvg,r.luzMin,r.luzMax);
        js+=item;
    }
    js+="]";
    server.sendHeader("Access-Control-Allow-Origin","*");
    server.sendHeader("Cache-Control","no-cache");
    server.send(200,"application/json",js);
}

void handleResetWifi(){
    server.send(200,"text/plain","Reseteando...");
    delay(800);
    WiFiManager wm; wm.resetSettings(); ESP.restart();
}

// =========================================================
//  SETUP & LOOP
// =========================================================
void setup(){
    Serial.begin(115200);
    delay(400);
    Serial.println("\n=== HidroFlow Gamma v0.01.00 ===");
    dht.begin();
    loadHistoryFromNVS();

    WiFiManager wm;
    wm.setConnectTimeout(25);
    wm.setConfigPortalTimeout(180);

    const char* wmCss=R"CSS(<style>
body{background:#0d1117!important;color:#e6edf3!important;font-family:system-ui,sans-serif!important}
.c{max-width:400px!important;margin:30px auto!important;background:#161b22!important;border:1px solid #30363d!important;border-radius:16px!important;padding:24px!important}
h1{color:#58a6ff!important;font-size:1.4rem!important;text-align:center!important;margin-bottom:15px!important}
button,input[type=submit]{background:#238636!important;color:#fff!important;border:none!important;padding:12px!important;border-radius:8px!important;font-size:1rem!important;font-weight:700!important;width:100%!important;cursor:pointer!important;margin-top:10px!important}
input[type=text],input[type=password]{background:#0d1117!important;color:#fff!important;border:1px solid #30363d!important;padding:10px!important;border-radius:8px!important;width:100%!important;margin:8px 0!important}
div,a{color:#8b949e!important}
</style>)CSS";
    wm.setCustomHeadElement(wmCss);

    if(!wm.autoConnect("ESP32_Config_AP","12345678")){
        Serial.println("❌ Wi-Fi falló. Reiniciando...");
        ESP.restart();
    }
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_ps(WIFI_PS_NONE);
    Serial.print("✅ Wi-Fi OK — http://"); Serial.println(WiFi.localIP());
    configTime(GMT_OFFSET_SEC,DAYLIGHT_OFFSET_SEC,"pool.ntp.org","time.nist.gov");
    Serial.println("🕒 NTP UTC-5 configurado.");

    server.on("/",          handleRoot);
    server.on("/api/data",  handleApiData);
    server.on("/api/history",handleApiHistory);
    server.on("/reset_wifi",handleResetWifi);
    server.begin();
    Serial.println("🚀 Servidor HTTP activo.");
}

void loop(){
    server.handleClient();
    readSensorsAsync();
    yield();
}
