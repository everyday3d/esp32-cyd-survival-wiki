// Phone tools page, served at /tools (and over https, which phones need before they hand a page the compass,
// camera or location). Everything runs in the phone's browser using the phone's own sensors.
#pragma once
#include <pgmspace.h>

static const char TOOLS_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="theme-color" content="#0b0f17">
<title>Phone tools</title>
<style>
:root{--bg:#0b0f17;--card:#131b26;--line:#233142;--text:#d5dfea;--muted:#8da0b4;--acc:#ff8a1f}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--text);font:16px system-ui,sans-serif}
header{padding:10px 14px;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:12px;background:var(--card)}
header b{font-size:1.1rem}header a{color:var(--acc);text-decoration:none;font-size:.9rem}header a.r{margin-left:auto}
nav{display:flex;border-bottom:1px solid var(--line);background:var(--card)}
nav button{flex:1;background:none;border:0;color:var(--muted);padding:12px 4px;font-size:.95rem;border-bottom:3px solid transparent}
nav button.on{color:var(--text);border-bottom-color:var(--acc)}
section{display:none;padding:14px;text-align:center}section.on{display:block}
.big{font-size:3rem;font-weight:700;letter-spacing:1px}
.sub{color:var(--muted);font-size:.9rem;margin:6px 0 12px}
button.go{background:var(--acc);color:#111;border:0;border-radius:12px;padding:12px 18px;font-size:1rem;font-weight:600;margin:6px}
button.alt{background:var(--card);color:var(--text);border:1px solid var(--line);border-radius:12px;padding:12px 18px;font-size:1rem;margin:6px}
.note{background:#1b2431;border-radius:10px;padding:10px 12px;color:var(--muted);font-size:.85rem;text-align:left;margin-top:14px}
.warn{background:#7c2d12;color:#ffedd5;border-radius:10px;padding:10px 12px;margin:10px 0;font-size:.9rem;text-align:left;display:none}
#dial{width:min(80vw,340px);height:min(80vw,340px)}
#vid{width:100%;max-height:60vh;background:#000;border-radius:12px;object-fit:cover}
#vidwrap{overflow:hidden;border-radius:12px;background:#000}
table{margin:8px auto;border-collapse:collapse}td{padding:4px 12px;text-align:left}td:first-child{color:var(--muted)}
input[type=range]{width:90%}
</style></head><body>
<header><b>&#129517; Phone tools</b><a href="/map">Map</a><a class="r" href="/">Wiki</a></header>
<nav><button data-t="compass" class="on">Compass</button><button data-t="sun">Sun</button><button data-t="level">Level</button><button data-t="cam">Camera</button></nav>
<div id="warn" class="warn"></div>

<section id="compass" class="on">
<svg id="dial" viewBox="-110 -110 220 220">
 <g id="ring"><circle r="100" fill="#131b26" stroke="#233142" stroke-width="2"/>
 <g id="ticks"></g>
 <text y="-72" text-anchor="middle" fill="#ff8a1f" font-size="22" font-weight="700">N</text>
 <text x="76" y="8" text-anchor="middle" fill="#d5dfea" font-size="18">E</text>
 <text y="86" text-anchor="middle" fill="#d5dfea" font-size="18">S</text>
 <text x="-76" y="8" text-anchor="middle" fill="#d5dfea" font-size="18">W</text>
 <circle id="sunDot" r="7" cx="0" cy="-90" fill="#ffd34d" style="display:none"/></g>
 <polygon points="0,-104 -9,-86 9,-86" fill="#e11d48"/>
 <circle r="3" fill="#d5dfea"/>
</svg>
<div class="big" id="hdg">---</div>
<div class="sub" id="hdgSub">Hold the phone flat.</div>
<button class="go" id="cstart">Start compass</button>
<div class="note">This shows <b>magnetic north</b>, the way a compass needle points. True north can be a few degrees off depending on where you are. Keep the phone away from metal, magnets and speakers. If it drifts, wave the phone in a figure 8 to calibrate.</div>
</section>

<section id="sun">
<div class="big" id="sunAz">---</div>
<div class="sub" id="sunSub">Needs your location to work out where the sun is.</div>
<table id="sunTab"></table>
<button class="go" id="sstart">Find the sun</button>
<div class="note">Line the compass up with the sun's direction (the yellow dot on the Compass tab) and you know the compass is pointing the right way. The sunrise and sunset times are worked out on the phone with no internet.</div>
</section>

<section id="level">
<div class="big" id="tilt">---</div>
<div class="sub" id="tiltSub">Lay the phone flat on a surface, or stand it on its long edge to read a slope.</div>
<svg id="bub" viewBox="-110 -110 220 220" style="width:min(70vw,300px)"><circle r="100" fill="#131b26" stroke="#233142" stroke-width="2"/><circle r="50" fill="none" stroke="#233142"/><line x1="-100" y1="0" x2="100" y2="0" stroke="#233142"/><line y1="-100" y2="100" x1="0" x2="0" stroke="#233142"/><circle id="bubble" r="14" fill="#ff8a1f"/></svg>
<div><button class="go" id="lstart">Start level</button></div>
<div class="note">Handy for checking a flat spot for a shelter or tent, or the slope of a hill. The angle is from level (0 degrees).</div>
</section>

<section id="cam">
<div id="vidwrap"><video id="vid" playsinline muted autoplay></video></div>
<div class="sub" id="camSub">A magnifier for small things: a splinter, a tick, tiny print.</div>
<input type="range" id="zoom" min="1" max="5" step="0.1" value="1" style="display:none">
<div><button class="go" id="camstart">Start camera</button><button class="alt" id="torch" style="display:none">Light on</button></div>
<div class="note">The picture stays on your phone. Nothing is saved or sent. Not every phone lets a web page switch on the light (iPhones do not).</div>
</section>

<script>
var $=function(i){return document.getElementById(i)};
var warn=$('warn');
function say(t){warn.textContent=t;warn.style.display=t?'block':'none'}
if(location.protocol==='http:'){
  say('Your phone only gives web pages the compass, camera and location on a secure page. Tap "Start" and it will open the secure version. Your phone warns about the certificate once: choose Advanced, then Continue.');
}
function secure(){ if(location.protocol==='http:'){location.href='https://'+location.hostname+'/tools'+location.hash;return false} return true}
var tabs=document.querySelectorAll('nav button'),secs=document.querySelectorAll('section');
function show(n){tabs.forEach(function(b){b.className=b.dataset.t===n?'on':''});secs.forEach(function(s){s.className=s.id===n?'on':''});location.hash=n}
tabs.forEach(function(b){b.onclick=function(){show(b.dataset.t)}});
if(location.hash.length>1&&$(location.hash.slice(1)))show(location.hash.slice(1));
// ---- compass dial ticks
(function(){var g=$('ticks'),s='';for(var a=0;a<360;a+=10){var r=a%90===0?0:(a%30===0?8:4);var x1=Math.sin(a*Math.PI/180),y1=-Math.cos(a*Math.PI/180);
  if(a%90)s+='<line x1="'+(x1*92).toFixed(1)+'" y1="'+(y1*92).toFixed(1)+'" x2="'+(x1*(96-r)).toFixed(1)+'" y2="'+(y1*(96-r)).toFixed(1)+'" stroke="#8da0b4" stroke-width="'+(a%30?1:2)+'"/>'}g.innerHTML=s})();
var heading=null,oriOn=false,tiltB=0,tiltG=0;
var DIRS=['N','NE','E','SE','S','SW','W','NW'];
function onOri(e){
  var h=null;
  if(e.webkitCompassHeading!=null)h=e.webkitCompassHeading;
  else if(e.absolute&&e.alpha!=null)h=(360-e.alpha)%360;
  if(e.beta!=null){tiltB=e.beta;tiltG=e.gamma}
  if(h!==null){
    if(heading===null)heading=h;else{var d=((h-heading+540)%360)-180;heading=(heading+d*0.25+360)%360}   // smooth the jitter
    $('hdg').textContent=Math.round(heading)+'° '+DIRS[Math.round(heading/45)%8];
    $('hdgSub').textContent='Magnetic north is at the top of the dial.';
    $('ring').setAttribute('transform','rotate('+(-heading).toFixed(1)+')');
  }
  drawLevel();
}
function startOri(cb){
  if(!secure())return;
  var go=function(){
    if(!oriOn){oriOn=true;window.addEventListener('deviceorientationabsolute',onOri,true);window.addEventListener('deviceorientation',onOri,true)}
    say('');if(cb)cb()};
  if(typeof DeviceOrientationEvent!=='undefined'&&DeviceOrientationEvent.requestPermission){
    DeviceOrientationEvent.requestPermission().then(function(r){if(r==='granted')go();else say('The phone did not allow motion and compass access.')}).catch(function(e){say('Could not start the compass: '+e)})
  }else if(typeof DeviceOrientationEvent!=='undefined')go();
  else say('This browser has no compass sensor.');
}
$('cstart').onclick=function(){startOri(function(){$('cstart').style.display='none'})};
// ---- level
function drawLevel(){
  var b=tiltB,g=tiltG,flat=Math.acos(Math.max(-1,Math.min(1,Math.cos(b*Math.PI/180)*Math.cos(g*Math.PI/180))))*180/Math.PI;
  var x=Math.max(-90,Math.min(90,g*3)),y=Math.max(-90,Math.min(90,b*3));
  $('bubble').setAttribute('cx',x.toFixed(1));$('bubble').setAttribute('cy',y.toFixed(1));
  $('tilt').textContent=flat.toFixed(1)+'°';
}
$('lstart').onclick=function(){startOri(function(){$('lstart').style.display='none'})};
// ---- sun
function sunPos(ms,lat,lon){
  var R=Math.PI/180,d=ms/86400000+2440587.5-2451545.0;
  var g=(357.529+0.98560028*d)%360,q=(280.459+0.98564736*d)%360;
  var L=(q+1.915*Math.sin(g*R)+0.020*Math.sin(2*g*R))%360,e=23.439-0.00000036*d;
  var ra=Math.atan2(Math.cos(e*R)*Math.sin(L*R),Math.cos(L*R))/R,dec=Math.asin(Math.sin(e*R)*Math.sin(L*R));
  var gm=(18.697374558+24.06570982441908*d)%24,lst=((gm*15+lon)%360+360)%360,H=(lst-ra)*R;
  var la=lat*R,alt=Math.asin(Math.sin(la)*Math.sin(dec)+Math.cos(la)*Math.cos(dec)*Math.cos(H));
  var az=Math.atan2(Math.sin(H),Math.cos(H)*Math.sin(la)-Math.tan(dec)*Math.cos(la))/R+180;
  return{az:(az%360+360)%360,alt:alt/R};
}
function hm(ms){var d=new Date(ms);return d.toLocaleTimeString([], {hour:'numeric',minute:'2-digit'})}
function sunTimes(lat,lon){
  var t0=new Date();t0.setHours(0,0,0,0);var rise=null,set=null,prev=sunPos(t0.getTime(),lat,lon).alt;
  for(var m=1;m<=1440;m++){var t=t0.getTime()+m*60000,a=sunPos(t,lat,lon).alt;
    if(prev<-0.833&&a>=-0.833&&rise===null)rise=t;if(prev>=-0.833&&a<-0.833&&set===null)set=t;prev=a}
  return{rise:rise,set:set};
}
var sunLL=null;
function showSun(){
  if(!sunLL)return;var s=sunPos(Date.now(),sunLL.lat,sunLL.lon),tm=sunTimes(sunLL.lat,sunLL.lon);
  $('sunAz').textContent=Math.round(s.az)+'° '+DIRS[Math.round(s.az/45)%8];
  $('sunSub').textContent=s.alt>0?'The sun is '+Math.round(s.alt)+'° above the horizon.':'The sun is below the horizon.';
  $('sunTab').innerHTML='<tr><td>Sunrise</td><td>'+(tm.rise?hm(tm.rise):'none today')+'</td></tr><tr><td>Sunset</td><td>'+(tm.set?hm(tm.set):'none today')+'</td></tr><tr><td>Position</td><td>'+sunLL.lat.toFixed(3)+', '+sunLL.lon.toFixed(3)+'</td></tr>';
  var dot=$('sunDot');dot.style.display=s.alt>-5?'block':'none';
  var a=s.az*Math.PI/180;dot.setAttribute('cx',(Math.sin(a)*90).toFixed(1));dot.setAttribute('cy',(-Math.cos(a)*90).toFixed(1));
}
$('sstart').onclick=function(){
  if(!secure())return;
  if(!navigator.geolocation){say('This browser can not give a location.');return}
  say('Asking the phone for its location...');
  navigator.geolocation.getCurrentPosition(function(p){sunLL={lat:p.coords.latitude,lon:p.coords.longitude};say('');showSun();setInterval(showSun,30000);$('sstart').style.display='none'},
    function(e){say('The phone would not share its location ('+(e&&e.message?e.message:'blocked')+').')},{enableHighAccuracy:false,timeout:15000,maximumAge:600000});
};
// ---- camera: magnifier and light
var stream=null,track=null,zoomHW=false;
$('camstart').onclick=function(){
  if(!secure())return;
  if(!navigator.mediaDevices||!navigator.mediaDevices.getUserMedia){say('This browser can not open the camera.');return}
  navigator.mediaDevices.getUserMedia({video:{facingMode:{ideal:'environment'},width:{ideal:1280}},audio:false}).then(function(s){
    stream=s;track=s.getVideoTracks()[0];$('vid').srcObject=s;say('');$('camstart').style.display='none';
    var c=track.getCapabilities?track.getCapabilities():{};
    var z=$('zoom');z.style.display='inline-block';
    if(c.zoom){zoomHW=true;z.min=c.zoom.min;z.max=Math.min(c.zoom.max,8);z.step=c.zoom.step||0.1;z.value=c.zoom.min}
    if(c.torch)$('torch').style.display='inline-block';
  }).catch(function(e){say('Could not open the camera ('+(e&&e.message?e.message:'blocked')+'). Check the phone allows camera access for the browser.')});
};
$('zoom').oninput=function(){
  var v=parseFloat(this.value);
  if(zoomHW&&track)track.applyConstraints({advanced:[{zoom:v}]}).catch(function(){});
  else $('vid').style.transform='scale('+v+')';
};
var torchOn=false;
$('torch').onclick=function(){torchOn=!torchOn;track.applyConstraints({advanced:[{torch:torchOn}]}).then(function(){$('torch').textContent=torchOn?'Light off':'Light on'}).catch(function(){say('This phone would not switch the light on.')})};
</script></body></html>)HTML";
