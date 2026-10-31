// Phone chat page for the mesh, served at /mesh while the hotspot is on. It lives in the firmware, not on
// the SD card, so it works even if the card's web files are old.
#pragma once
#include <pgmspace.h>

static const char MESH_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mesh chat</title>
<style>
:root{--bg:#0b0f17;--card:#131b26;--line:#233142;--text:#d5dfea;--muted:#8da0b4;--acc:#ff8a1f;--me:#1d4ed8}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,sans-serif;display:flex;flex-direction:column;height:100dvh}
header{padding:12px 14px;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:10px;background:var(--card)}
header b{font-size:1.1rem}
header a{color:var(--acc);text-decoration:none;font-size:.9rem}#st{margin-right:auto}
#st{color:var(--muted);font-size:.85rem}
#near{padding:8px 14px;border-bottom:1px solid var(--line);color:var(--muted);font-size:.85rem;display:flex;flex-wrap:wrap;gap:6px 14px}
#near span b{color:var(--text);font-weight:600}
#log{flex:1;overflow-y:auto;padding:12px 14px;display:flex;flex-direction:column;gap:8px}
.m{max-width:85%;padding:8px 12px;border-radius:14px;background:var(--card);border:1px solid var(--line);word-wrap:break-word}
.m small{display:block;color:var(--muted);font-size:.72rem}
.m.me{align-self:flex-end;background:var(--me);border-color:var(--me)}
.m.me small{color:#c7d6ff}
.m .st{margin-top:2px;text-align:right;color:#c7d6ff;font-size:.72rem}
.m .st.ok{color:#a7f3d0}
#empty{color:var(--muted);text-align:center;margin-top:30px}
#quick{padding:6px 14px;display:flex;gap:6px;overflow-x:auto;border-top:1px solid var(--line)}
#quick button{white-space:nowrap;background:var(--card);color:var(--text);border:1px solid var(--line);border-radius:999px;padding:6px 12px;font-size:.9rem}
form{display:flex;gap:6px;padding:10px 14px;border-top:1px solid var(--line);background:var(--card)}
input{background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:10px;padding:10px;font-size:16px;min-width:0}
#name{width:30%}#msg{flex:1}
form button{background:var(--acc);color:#111;border:0;border-radius:10px;padding:0 16px;font-weight:700;font-size:1rem}
</style></head><body>
<header><b>&#128225; Mesh chat</b><span id="st">connecting...</span><a href="/map">Map</a><a href="/">Wiki</a></header>
<div id="near"></div>
<div id="log"><div id="empty">No messages yet.</div></div>
<div id="quick"></div>
<form id="f" autocomplete="off">
<input id="name" maxlength="11" placeholder="Name" aria-label="Your name">
<input id="msg" maxlength="50" placeholder="Message" aria-label="Message">
<button>Send</button></form>
<script>
var seq=0,me='',stEls={},log=document.getElementById('log'),nameEl=document.getElementById('name'),msgEl=document.getElementById('msg');
var Q=['OK','Need help','Heading out','At camp','Water found','Come to me'];
function ls(k,v){try{if(v===undefined)return localStorage.getItem(k);localStorage.setItem(k,v)}catch(e){return null}}
me=ls('meshName')||'Phone'+Math.floor(Math.random()*90+10);nameEl.value=me;
nameEl.onchange=function(){me=nameEl.value.replace(/[^\w .-]/g,'').trim()||'Phone';nameEl.value=me;ls('meshName',me)};
function ago(s){return s<60?'now':s<3600?Math.floor(s/60)+'m ago':Math.floor(s/3600)+'h ago'}
function add(m){
  var e=document.getElementById('empty');if(e)e.remove();
  var d=document.createElement('div');d.className='m'+(m.f===me?' me':'');
  var s=document.createElement('small');s.textContent=(m.f===me?'you':m.f)+' \u00b7 '+ago(m.a);
  var t=document.createElement('div');t.textContent=m.t;d.appendChild(s);d.appendChild(t);
  if(m.f===me){var k=document.createElement('div');k.className='st';k.textContent='Sent';d.appendChild(k);stEls[m.n]=k}
  log.appendChild(d);
}
function heard(a){   // like "Read" on a phone: Sent, then Heard by 2
  var k=stEls[a.n];if(!k)return;
  if(a.c>0){k.className='st ok';k.textContent='Heard by '+(a.c>a.k.length?a.c+' boards':a.k.join(', '))}
  else{k.className='st';k.textContent='Sent'}
}
function bars(r){return r>=-55?'\u2582\u2584\u2586\u2588':r>=-65?'\u2582\u2584\u2586':r>=-75?'\u2582\u2584':r>=-85?'\u2582':''}
function poll(){
  fetch('/api/mesh?since='+seq).then(function(r){return r.json()}).then(function(j){
    document.getElementById('st').textContent=j.on?('on as '+j.name):'mesh is off on the board';
    if(j.seq<seq){log.innerHTML='';stEls={};seq=0}
    var atEnd=log.scrollHeight-log.scrollTop-log.clientHeight<60;
    j.msgs.forEach(function(m){if(m.n>seq){add(m);seq=m.n}});
    j.acks.forEach(heard);
    if(atEnd)log.scrollTop=log.scrollHeight;
    var n=document.getElementById('near');n.innerHTML='';
    if(!j.peers.length){n.textContent=j.on?'No other boards heard yet':''}
    j.peers.forEach(function(p){var s=document.createElement('span'),b=document.createElement('b');b.textContent=p.n;
      s.appendChild(b);s.appendChild(document.createTextNode(' '+bars(p.r)+(p.d>0?' ~'+p.d+' m':'')));n.appendChild(s)});
  }).catch(function(){document.getElementById('st').textContent='lost the board'});
}
function send(t){
  t=t.trim();if(!t)return;
  nameEl.onchange();
  fetch('/api/mesh/send',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'from='+encodeURIComponent(me)+'&text='+encodeURIComponent(t)}).then(function(r){return r.json()}).then(function(j){
    if(!j.ok)alert('Not sent. The mesh may be off, or you are sending too fast.');setTimeout(poll,900)});
}
document.getElementById('f').onsubmit=function(e){e.preventDefault();send(msgEl.value);msgEl.value=''};
Q.forEach(function(q){var b=document.createElement('button');b.textContent=q;b.onclick=function(){send(q)};document.getElementById('quick').appendChild(b)});
poll();setInterval(poll,2000);
</script></body></html>)HTML";
