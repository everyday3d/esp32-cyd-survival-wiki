// Phone map page, served at /map while the hotspot is on. Tiles come from the SD card at /maps/{z}/{x}/{y}.jpg.
#pragma once
#include <pgmspace.h>

static const char MAP_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Map</title>
<style>
:root{--bg:#0b0f17;--card:#131b26;--line:#233142;--text:#d5dfea;--muted:#8da0b4;--acc:#ff8a1f}
*{box-sizing:border-box}
html,body{margin:0;height:100%;background:var(--bg);color:var(--text);font:15px system-ui,sans-serif;overscroll-behavior:none}
body{display:flex;flex-direction:column;height:100dvh}
header{padding:10px 14px;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:12px;background:var(--card)}
header b{font-size:1.05rem}header a{color:var(--acc);text-decoration:none;font-size:.9rem}
header a.r{margin-left:auto}
#view{flex:1;position:relative;overflow:hidden;touch-action:none;background:#2a323d}
#layer{position:absolute;left:0;top:0;width:100%;height:100%;will-change:transform}
#layer img{position:absolute;width:256px;height:256px;user-select:none;-webkit-user-drag:none}
.me{position:absolute;width:18px;height:18px;margin:-9px 0 0 -9px;border-radius:50%;background:#2563eb;border:3px solid #fff;box-shadow:0 0 0 6px #2563eb55}
#cross{position:absolute;left:50%;top:50%;width:22px;height:22px;margin:-11px 0 0 -11px;pointer-events:none;opacity:.7}
#cross:before,#cross:after{content:"";position:absolute;background:#e11d48}
#cross:before{left:10px;top:0;width:2px;height:22px}#cross:after{top:10px;left:0;height:2px;width:22px}
#info{position:absolute;left:8px;bottom:8px;background:#0b0f17cc;padding:3px 8px;border-radius:8px;font-size:.78rem;pointer-events:none}
#msg{position:absolute;left:8px;right:8px;top:8px;background:#7c2d12;color:#ffedd5;padding:8px 10px;border-radius:10px;display:none;font-size:.88rem}
footer{display:flex;gap:6px;padding:8px 10px;border-top:1px solid var(--line);background:var(--card)}
footer button{flex:1;background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:10px;padding:10px 4px;font-size:.95rem}
footer button.on{background:var(--acc);color:#111;border-color:var(--acc)}
</style></head><body>
<header><b>&#128506; Map</b><a href="/mesh">Chat</a><a class="r" href="/">Wiki</a></header>
<div id="view"><div id="layer"></div><div id="cross"></div><div id="info"></div><div id="msg"></div></div>
<footer>
<button id="zo">&minus;</button><button id="zi">+</button>
<button id="loc">&#128205; My GPS</button><button id="set">Set my spot</button>
</footer>
<script>
var T=256,z=13,zmin=8,zmax=16,cx=0,cy=0,me=null,setting=false;
var view=document.getElementById('view'),layer=document.getElementById('layer'),info=document.getElementById('info'),msg=document.getElementById('msg');
function wp(lat,lon,zz){var n=T*Math.pow(2,zz),r=lat*Math.PI/180;return{x:(lon+180)/360*n,y:(1-Math.log(Math.tan(r)+1/Math.cos(r))/Math.PI)/2*n}}
function ll(x,y,zz){var n=T*Math.pow(2,zz);return{lon:x/n*360-180,lat:Math.atan(Math.sinh(Math.PI*(1-2*y/n)))*180/Math.PI}}
function say(t){msg.textContent=t;msg.style.display=t?'block':'none';if(t)setTimeout(function(){msg.style.display='none'},7000)}
// The board serves only a few connections at a time, so load tiles three at a time and retry any that fail.
var queue=[],gen=0,busy=0;
function fetchTile(im,g,tries){
  busy++;var done=false;
  function fin(ok){if(done)return;done=true;busy--;
    if(!ok&&tries<4&&g===gen){setTimeout(function(){if(g===gen)fetchTile(im,g,tries+1)},500*(tries+1))}
    else if(!ok){im.style.visibility='hidden'}
    pump(g)}
  im.onload=function(){fin(true)};im.onerror=function(){fin(false)};
  im.src=im.dataset.src+(tries?'?r='+tries:'')}
function pump(g){while(g===gen&&busy<3&&queue.length)fetchTile(queue.shift(),g,0)}
function draw(){
  queue=[];busy=0;
  var W=view.clientWidth,H=view.clientHeight,n=Math.pow(2,z),left=cx-W/2,top=cy-H/2;
  layer.innerHTML='';layer.style.transform='translate(0,0)';
  for(var ty=Math.floor((top-T)/T);ty<=Math.floor((top+H+T)/T);ty++)for(var tx=Math.floor((left-T)/T);tx<=Math.floor((left+W+T)/T);tx++){
    if(tx<0||ty<0||tx>=n||ty>=n)continue;
    var im=new Image();im.draggable=false;im.dataset.src='/maps/'+z+'/'+tx+'/'+ty+'.jpg';
    im.style.left=(tx*T-left)+'px';im.style.top=(ty*T-top)+'px';layer.appendChild(im);queue.push(im)}
  gen++;pump(gen);
  if(me){var p=wp(me.lat,me.lon,z),d=document.createElement('div');d.className='me';d.style.left=(p.x-left)+'px';d.style.top=(p.y-top)+'px';layer.appendChild(d)}
  var c=ll(cx,cy,z);info.textContent=c.lat.toFixed(4)+', '+c.lon.toFixed(4)+'  z'+z;
}
function zoom(dz){var nz=Math.max(zmin,Math.min(zmax,z+dz));if(nz===z)return;var f=Math.pow(2,nz-z);cx*=f;cy*=f;z=nz;draw()}
document.getElementById('zi').onclick=function(){zoom(1)};document.getElementById('zo').onclick=function(){zoom(-1)};
var ptrs={},sx,sy,moved=0,startDist=0;
view.onpointerdown=function(e){view.setPointerCapture(e.pointerId);ptrs[e.pointerId]={x:e.clientX,y:e.clientY};sx=e.clientX;sy=e.clientY;moved=0;
  var k=Object.keys(ptrs);if(k.length===2){var a=ptrs[k[0]],b=ptrs[k[1]];startDist=Math.hypot(a.x-b.x,a.y-b.y)}};
view.onpointermove=function(e){if(!ptrs[e.pointerId])return;ptrs[e.pointerId]={x:e.clientX,y:e.clientY};
  var k=Object.keys(ptrs);
  if(k.length===2){var a=ptrs[k[0]],b=ptrs[k[1]],d=Math.hypot(a.x-b.x,a.y-b.y);
    if(startDist&&d/startDist>1.6){zoom(1);startDist=d}else if(startDist&&d/startDist<0.62){zoom(-1);startDist=d}return}
  var dx=e.clientX-sx,dy=e.clientY-sy;moved=Math.max(moved,Math.abs(dx)+Math.abs(dy));layer.style.transform='translate('+dx+'px,'+dy+'px)'};
view.onpointerup=view.onpointercancel=function(e){
  if(!ptrs[e.pointerId])return;var multi=Object.keys(ptrs).length>1;delete ptrs[e.pointerId];
  if(multi){draw();return}
  var dx=e.clientX-sx,dy=e.clientY-sy;
  if(moved<8&&setting){var r=view.getBoundingClientRect();var p=ll(cx-view.clientWidth/2+(e.clientX-r.left),cy-view.clientHeight/2+(e.clientY-r.top),z);
    me={lat:p.lat,lon:p.lon};setting=false;document.getElementById('set').className='';say('Your spot is set.')}
  else{cx-=dx;cy-=dy}
  draw()};
document.getElementById('set').onclick=function(){setting=!setting;this.className=setting?'on':'';if(setting)say('Tap the map where you are.')};
document.getElementById('loc').onclick=function(){
  if(location.protocol==='http:'){   // phones only share their location on https pages
    say('Opening the secure page. Your phone will warn about the certificate: choose Advanced, then Continue. You only do this once.');
    setTimeout(function(){location.href='https://'+location.hostname+'/map?gps=1'},2500);return}
  if(!navigator.geolocation){say('This browser can not give a location. Use "Set my spot" instead.');return}
  say('Asking the phone for its location...');
  navigator.geolocation.getCurrentPosition(function(p){me={lat:p.coords.latitude,lon:p.coords.longitude};var w=wp(me.lat,me.lon,z);cx=w.x;cy=w.y;say('');draw()},
    function(e){say('The phone would not share its location ('+(e&&e.message?e.message:'blocked')+'). Check the phone allows location for the browser. You can also use "Set my spot" and tap the map.')},
    {enableHighAccuracy:true,timeout:15000})};
fetch('/maps/center.txt').then(function(r){if(!r.ok)throw 0;return r.text()}).then(function(t){
  var a=t.trim().split(/\s+/).map(Number);zmin=a[3];zmax=a[4];z=Math.max(zmin,Math.min(zmax,a[2]));var w=wp(a[0],a[1],z);cx=w.x;cy=w.y;draw();
  if(location.search.indexOf('gps=1')>=0)document.getElementById('loc').onclick()   // we were sent here to ask for the location
}).catch(function(){info.textContent='No maps on the SD card';say('No maps found. Make them with scripts/get-map-tiles.ps1 and copy the maps folder to the card.')});
window.onresize=draw;
</script></body></html>)HTML";
