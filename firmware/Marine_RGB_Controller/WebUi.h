#pragma once

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#090b10">
  <meta name="description" content="Prism RGB light controller">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <meta name="apple-mobile-web-app-title" content="Prism">
  <title>Prism</title>
  <link rel="manifest" href="/manifest.webmanifest">
  <link rel="icon" type="image/png" sizes="512x512" href="/app-logo.png">
  <link rel="apple-touch-icon" href="/app-logo.png">

  <style>
    :root{
      color-scheme:dark;
      --bg:#090b10;--panel:#121721;--line:rgba(255,255,255,.09);
      --text:#f5f7fa;--muted:#929cac;--soft:#667182;--ok:#53d47a;
      --radius:24px;--small:16px
    }
    *{box-sizing:border-box}
    html{background:var(--bg)}
    body{
      margin:0;min-height:100vh;color:var(--text);
      font-family:Inter,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
      background:
        radial-gradient(circle at 50% -10%,rgba(67,96,158,.28),transparent 34%),
        radial-gradient(circle at 100% 75%,rgba(35,117,108,.10),transparent 30%),
        var(--bg)
    }
    button,input,select{font:inherit}
    button{cursor:pointer}
    .shell{width:min(100%,1120px);margin:auto;padding:20px}
    .top{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:18px;padding:2px 3px}
    .brand strong{display:block;font-size:18px;letter-spacing:-.4px}
    .brand span{display:block;color:var(--soft);font-size:11px;margin-top:3px}
    .tabs{
      position:sticky;top:10px;z-index:10;display:grid;grid-template-columns:repeat(3,1fr);
      gap:8px;padding:7px;border:1px solid var(--line);border-radius:18px;
      background:rgba(12,15,21,.82);backdrop-filter:blur(20px);margin-bottom:16px
    }
    .tab{min-height:44px;border:0;border-radius:13px;color:var(--muted);background:transparent;font-weight:700}
    .tab.active{color:#10141b;background:#f6f7f9}
    .page{display:none}.page.active{display:block}
    .grid{display:grid;grid-template-columns:1.05fr .95fr;gap:16px}
    .card{
      border:1px solid var(--line);border-radius:var(--radius);
      background:linear-gradient(145deg,rgba(255,255,255,.035),rgba(255,255,255,.01)),rgba(16,21,30,.86);
      box-shadow:0 26px 80px rgba(0,0,0,.28);padding:22px
    }
    .title{margin:0;font-size:23px;letter-spacing:-.7px}
    .desc{margin:7px 0 0;color:var(--muted);font-size:13px;line-height:1.55}
    .wheel-wrap{display:grid;place-items:center;padding:24px 0 10px}
    .wheel-shell{position:relative;width:min(74vw,330px);aspect-ratio:1}
    #hueWheel{display:block;width:100%;height:100%;border-radius:50%;touch-action:none;box-shadow:0 22px 52px rgba(0,0,0,.36)}
    .picker{
      position:absolute;width:26px;height:26px;border:4px solid #fff;border-radius:50%;
      transform:translate(-50%,-50%);box-shadow:0 3px 15px rgba(0,0,0,.78);pointer-events:none
    }
    .preview{
      height:68px;border:1px solid var(--line);border-radius:18px;margin-top:15px;
      display:flex;align-items:center;justify-content:space-between;padding:0 18px;
      background:var(--selected,#ff3b30);box-shadow:inset 0 0 40px rgba(255,255,255,.08)
    }
    .preview strong{font-size:14px;text-shadow:0 2px 8px rgba(0,0,0,.5)}
    .power{
      width:44px;height:44px;border:1px solid rgba(255,255,255,.22);border-radius:14px;
      background:rgba(0,0,0,.22);color:#fff;font-size:19px
    }
    .control{margin-top:21px}
    .control-head{display:flex;justify-content:space-between;gap:12px;margin-bottom:10px;font-size:13px}
    .control-head span:last-child{color:var(--muted)}
    input[type=range]{width:100%;accent-color:#fff}
    .quick{display:grid;grid-template-columns:repeat(4,1fr);gap:9px;margin-top:20px}
    .swatch{
      position:relative;aspect-ratio:1;border:2px solid transparent;border-radius:14px;background:var(--c);
      color:#fff;font-size:0
    }
    .swatch.active{border-color:white;box-shadow:0 0 0 3px rgba(255,255,255,.12)}
    .swatch[data-label]:hover:after{
      content:attr(data-label);position:absolute;left:50%;bottom:-27px;transform:translateX(-50%);
      white-space:nowrap;padding:4px 7px;border-radius:8px;background:#111722;color:#fff;font-size:10px;z-index:5
    }
    .effects{display:grid;grid-template-columns:repeat(2,1fr);gap:11px;margin-top:20px}
    .effect{
      min-height:105px;text-align:left;padding:16px;border:1px solid var(--line);
      border-radius:18px;color:var(--text);background:rgba(255,255,255,.025)
    }
    .effect strong{display:block;font-size:14px}
    .effect span{display:block;color:var(--muted);font-size:11px;line-height:1.45;margin-top:6px}
    .effect.active{border-color:rgba(255,255,255,.42);background:rgba(255,255,255,.08)}
    .rainbow{background:linear-gradient(135deg,rgba(255,70,70,.18),rgba(255,220,70,.12),rgba(50,220,145,.13),rgba(65,120,255,.16),rgba(190,70,255,.16))}
    .form-section{margin-top:18px;padding-top:18px;border-top:1px solid var(--line)}
    .form-section:first-of-type{border-top:0;padding-top:4px}
    .section-name{font-size:11px;color:var(--soft);font-weight:800;letter-spacing:1.1px;text-transform:uppercase;margin-bottom:12px}
    .row{display:grid;grid-template-columns:1fr auto;align-items:center;gap:18px;padding:13px 0}
    .row+.row{border-top:1px solid rgba(255,255,255,.055)}
    .row label strong{display:block;font-size:13px}
    .row label span{display:block;color:var(--muted);font-size:11px;line-height:1.45;margin-top:4px}
    .field{
      width:180px;max-width:46vw;border:1px solid var(--line);border-radius:12px;
      background:#0d1118;color:var(--text);padding:10px 12px
    }
    .toggle{width:48px;height:28px;border-radius:999px;border:0;background:#303846;padding:3px}
    .toggle:after{content:"";display:block;width:22px;height:22px;border-radius:50%;background:#fff;transition:.18s}
    .toggle.on{background:#4f89ff}.toggle.on:after{transform:translateX(20px)}
    .actions{display:flex;flex-wrap:wrap;gap:9px;margin-top:18px}
    .btn{
      min-height:43px;padding:0 15px;border:1px solid var(--line);border-radius:13px;
      color:var(--text);background:rgba(255,255,255,.04);font-weight:700;font-size:12px
    }
    .btn.primary{color:#10141b;background:#f6f7f9}
    .btn.danger{color:#ffaba5;border-color:rgba(255,100,90,.25)}
    .network-status{
      display:flex;align-items:center;justify-content:space-between;gap:16px;
      margin-top:18px;padding:15px 16px;border:1px solid var(--line);border-radius:16px;background:rgba(255,255,255,.025)
    }
    .network-status div{display:flex;align-items:center;gap:9px}
    .dot{width:8px;height:8px;border-radius:50%;background:var(--ok);box-shadow:0 0 12px #53d47a}
    .network-status span{color:var(--muted);font-size:12px}
    details{border:1px solid var(--line);border-radius:18px;margin-top:12px;background:rgba(255,255,255,.018)}
    summary{padding:16px;cursor:pointer;color:var(--muted);font-size:13px;font-weight:750}
    .details{padding:0 16px 16px}
    .toast{
      position:fixed;left:50%;bottom:24px;transform:translate(-50%,20px);opacity:0;pointer-events:none;
      padding:12px 16px;border:1px solid var(--line);border-radius:14px;background:#151b25;
      color:var(--text);font-size:12px;transition:.2s;z-index:99
    }
    .toast.show{opacity:1;transform:translate(-50%,0)}
    @media(max-width:820px){.grid{grid-template-columns:1fr}.shell{padding:12px}.card{padding:18px}.effects{grid-template-columns:1fr 1fr}}
    @media(max-width:480px){.effects{grid-template-columns:1fr}.field{width:145px}.brand strong{font-size:16px}}
  </style>
</head>
<body>
<div class="shell">
  <header class="top">
    <div class="brand"><strong id="deviceTitle">Prism</strong><span>RGB Light Controller</span></div>
  </header>

  <nav class="tabs">
    <button class="tab active" data-page="control">LIGHT</button>
    <button class="tab" data-page="effects">EFFECTS</button>
    <button class="tab" data-page="settings">SETTINGS</button>
  </nav>

  <main>
    <section class="page active" id="control">
      <div class="grid">
        <article class="card">
          <h1 class="title">Color</h1>
          <p class="desc">Choose a color for the complete LED strip.</p>
          <div class="wheel-wrap">
            <div class="wheel-shell">
              <canvas id="hueWheel" width="660" height="660"></canvas>
              <div class="picker" id="picker"></div>
            </div>
          </div>
          <div class="preview" id="preview">
            <strong id="colorLabel">Selected color</strong>
            <button class="power" id="powerBtn" aria-label="Toggle power">⏻</button>
          </div>
          <div class="quick" id="quickColors"></div>
        </article>

        <article class="card">
          <h2 class="title">Light output</h2>
          <p class="desc">Color and brightness are applied immediately. Fade timing is configured once under Advanced Settings.</p>

          <div class="control">
            <div class="control-head"><span>Brightness</span><span id="brightnessValue">70%</span></div>
            <input id="brightness" type="range" min="1" max="100" value="70">
          </div>

          <details>
            <summary>Current output</summary>
            <div class="details">
              <div class="row"><label><strong>Mode</strong><span>Active renderer</span></label><span id="currentMode">Static</span></div>
              <div class="row"><label><strong>RGB</strong><span>Selected color</span></label><span id="rgbValue">255, 128, 40</span></div>
            </div>
          </details>
        </article>
      </div>
    </section>

    <section class="page" id="effects">
      <article class="card">
        <h1 class="title">Effects</h1>
        <p class="desc">A curated set of effects with clearly different behavior.</p>
        <div class="effects">
          <button class="effect" data-effect="static"><strong>Static Color</strong><span>One steady color selected from the hue wheel.</span></button>
          <button class="effect rainbow" data-effect="rainbow"><strong>Rainbow Flow</strong><span>A smooth full-spectrum flow along the strip.</span></button>
          <button class="effect" data-effect="fade"><strong>Color Fade</strong><span>The complete strip fades through saturated colors.</span></button>
          <button class="effect" data-effect="disco"><strong>Disco</strong><span>Fast, decisive color changes without harsh flashing.</span></button>
          <button class="effect" data-effect="sparkle"><strong>Sparkle</strong><span>Small highlights shimmer over the selected base color.</span></button>
          <button class="effect" data-effect="off"><strong>Off</strong><span>Turns the output off while retaining all settings.</span></button>
        </div>

        <div class="control">
          <div class="control-head"><span>Effect speed</span><span id="speedValue">50%</span></div>
          <input id="speed" type="range" min="1" max="100" value="50">
        </div>
        <div class="control">
          <div class="control-head"><span>Effect intensity</span><span id="intensityValue">65%</span></div>
          <input id="intensity" type="range" min="1" max="100" value="65">
        </div>
      </article>
    </section>

    <section class="page" id="settings">
      <div class="grid">
        <article class="card">
          <h1 class="title">Settings</h1>
          <p class="desc">Common installation and startup options.</p>

          <div class="network-status">
            <div><i class="dot"></i><span>Controller connected</span></div>
            <strong id="connectionText">—</strong>
          </div>

          <div class="form-section">
            <div class="section-name">General</div>
            <div class="row"><label><strong>Device name</strong><span>Shown at the top and used as the installed app name.</span></label><input class="field" id="deviceName" maxlength="31"></div>
            <div class="row"><label><strong>Restore previous state</strong><span>Restores the last color, brightness and effect after startup.</span></label><button class="toggle" id="restoreState"></button></div>
            <div class="row"><label><strong>Maximum brightness</strong><span>Global output safety limit.</span></label><input class="field" id="maxBrightness" type="number" min="1" max="100"></div>
          </div>

          <div class="form-section">
            <div class="section-name">LED configuration</div>
            <div class="row"><label><strong>LED count</strong><span>Active pixels on output 1.</span></label><input class="field" id="ledCount" type="number" min="1" max="300"></div>
            <div class="row"><label><strong>Color order</strong><span>Applied after restart.</span></label>
              <select class="field" id="colorOrder"><option>RGB</option><option>GRB</option><option>BRG</option></select>
            </div>
            <div class="row"><label><strong>Warm-white compensation</strong><span>Reduces the cold shift that can appear at high brightness.</span></label><button class="toggle" id="warmCompensation"></button></div>
          </div>

          <div class="actions">
            <button class="btn primary" id="saveSettings">Save settings</button>
            <button class="btn" id="restartBtn">Restart controller</button>
          </div>
        </article>

        <article class="card">
          <h2 class="title">Advanced settings</h2>
          <p class="desc">Technical behavior for transitions, isolated inputs and networking.</p>

          <div class="form-section">
            <div class="section-name">Transitions</div>
            <div class="row"><label><strong>Smooth transitions</strong><span>Fade between static colors instead of switching instantly.</span></label><button class="toggle" id="smoothTransitions"></button></div>
            <div class="row"><label><strong>Default fade</strong><span>Transition duration in milliseconds. Zero means instant.</span></label><input class="field" id="defaultFade" type="number" min="0" max="5000"></div>
          </div>

          <div class="form-section">
            <div class="section-name">Isolated input 1</div>
            <div class="row"><label><strong>Enabled</strong><span>Enables the fixed hardware input on GPIO38.</span></label><button class="toggle" id="input1Enabled"></button></div>
            <div class="row"><label><strong>Action</strong><span id="input1Help">Select an action to see how short and long presses behave.</span></label>
              <select class="field input-action" id="input1Action">
                <option value="colors">Cycle preset colors</option>
                <option value="power">Toggle power / hold to dim</option>
                <option value="effects">Next effect</option>
                <option value="previous-effect">Previous effect</option>
                <option value="brightness-up">Brightness up</option>
                <option value="brightness-down">Brightness down</option>
                <option value="warm-white">Warm white</option>
                <option value="red">Red scene</option>
                <option value="green">Green scene</option>
                <option value="blue">Blue scene</option>
              </select>
            </div>
          </div>

          <div class="form-section">
            <div class="section-name">Isolated input 2</div>
            <div class="row"><label><strong>Enabled</strong><span>Enables the fixed hardware input on GPIO39.</span></label><button class="toggle" id="input2Enabled"></button></div>
            <div class="row"><label><strong>Action</strong><span id="input2Help">Select an action to see how short and long presses behave.</span></label>
              <select class="field input-action" id="input2Action">
                <option value="effects">Next effect</option>
                <option value="power">Toggle power / hold to dim</option>
                <option value="colors">Cycle preset colors</option>
                <option value="previous-effect">Previous effect</option>
                <option value="brightness-up">Brightness up</option>
                <option value="brightness-down">Brightness down</option>
                <option value="warm-white">Warm white</option>
                <option value="red">Red scene</option>
                <option value="green">Green scene</option>
                <option value="blue">Blue scene</option>
              </select>
            </div>
          </div>

          <div class="form-section">
            <div class="section-name">Network</div>
            <div class="row"><label><strong>mDNS hostname</strong><span>Available as hostname.local.</span></label><input class="field" id="mdnsName" maxlength="31"></div>
          </div>

          <details>
            <summary>System maintenance</summary>
            <div class="details">
              <div class="actions">
                <button class="btn" id="wifiResetBtn">Reset Wi-Fi</button>
                <button class="btn danger" id="factoryResetBtn">Factory reset</button>
              </div>
            </div>
          </details>
        </article>
      </div>
    </section>
  </main>
</div>
<div class="toast" id="toast"></div>

<script>
const $=s=>document.querySelector(s), $$=s=>[...document.querySelectorAll(s)];
let state={r:255,g:128,b:40,power:true,brightness:70,effect:"static",speed:50,intensity:65};
let dragging=false, colorTimer, controlTimer, settingsLoaded=false;

const actionHelp={
  colors:"A short press selects the next preset color. A long press repeats through the presets.",
  power:"A short press turns the light on or off. Hold the input to raise or lower brightness continuously.",
  effects:"A short press selects the next effect. Holding repeats through the effect list.",
  "previous-effect":"A short press selects the previous effect. Holding repeats backwards.",
  "brightness-up":"Each short press increases brightness. Holding ramps it upward continuously.",
  "brightness-down":"Each short press decreases brightness. Holding ramps it downward continuously.",
  "warm-white":"A short press selects the calibrated warm-white preset. Holding adjusts brightness.",
  red:"A short press selects the red scene. Holding adjusts brightness.",
  green:"A short press selects the green scene. Holding adjusts brightness.",
  blue:"A short press selects the blue scene. Holding adjusts brightness."
};

function toast(text){const t=$("#toast");t.textContent=text;t.classList.add("show");setTimeout(()=>t.classList.remove("show"),1700)}
function form(data){return new URLSearchParams(data).toString()}
async function api(path,data){
  const opt=data?{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:form(data)}:{};
  const r=await fetch(path,opt);if(!r.ok)throw new Error(await r.text());
  return r.headers.get("content-type")?.includes("json")?r.json():r.text()
}
function hsvToRgb(h,s,v){
  let f=(n,k=(n+h/60)%6)=>v-v*s*Math.max(Math.min(k,4-k,1),0);
  return [Math.round(f(5)*255),Math.round(f(3)*255),Math.round(f(1)*255)]
}
function rgbToHsv(r,g,b){
  r/=255;g/=255;b/=255;let mx=Math.max(r,g,b),mn=Math.min(r,g,b),d=mx-mn,h=0;
  if(d){if(mx===r)h=60*((g-b)/d%6);else if(mx===g)h=60*((b-r)/d+2);else h=60*((r-g)/d+4)}
  if(h<0)h+=360;return [h,mx?d/mx:0,mx]
}
function drawWheel(){
  const c=$("#hueWheel"),ctx=c.getContext("2d"),w=c.width,h=c.height,cx=w/2,cy=h/2,r=w/2-6;
  const img=ctx.createImageData(w,h),d=img.data;
  for(let y=0;y<h;y++)for(let x=0;x<w;x++){
    const dx=x-cx,dy=y-cy,dist=Math.hypot(dx,dy),i=(y*w+x)*4;
    if(dist>r){d[i+3]=0;continue}
    const hue=(Math.atan2(dy,dx)*180/Math.PI+90+360)%360;
    const sat=Math.pow(Math.min(dist/r,1),0.72);
    const [rr,gg,bb]=hsvToRgb(hue,sat,1);
    d[i]=rr;d[i+1]=gg;d[i+2]=bb;d[i+3]=255
  }
  ctx.putImageData(img,0,0)
}
function setWheelFromRgb(){
  const [h,s]=rgbToHsv(state.r,state.g,state.b),box=$(".wheel-shell"),p=$("#picker"),rad=(h-90)*Math.PI/180,radius=(box.clientWidth/2-9)*Math.pow(s,1/0.72);
  p.style.left=(box.clientWidth/2+Math.cos(rad)*radius)+"px";
  p.style.top=(box.clientHeight/2+Math.sin(rad)*radius)+"px"
}
function pick(e){
  const box=$("#hueWheel").getBoundingClientRect(),x=e.clientX-box.left-box.width/2,y=e.clientY-box.top-box.height/2;
  const dist=Math.min(Math.hypot(x,y)/(box.width/2-4),1),h=(Math.atan2(y,x)*180/Math.PI+90+360)%360;
  [state.r,state.g,state.b]=hsvToRgb(h,Math.pow(dist,.72),1);
  state.effect="static";state.power=true;render();scheduleColor()
}
function scheduleColor(){
  clearTimeout(colorTimer);
  colorTimer=setTimeout(()=>api("/api/color",{r:state.r,g:state.g,b:state.b,power:1}).catch(()=>{}),55)
}
function scheduleControl(path,data){
  clearTimeout(controlTimer);controlTimer=setTimeout(()=>api(path,data).catch(()=>{}),70)
}
function render(){
  const c=`rgb(${state.r},${state.g},${state.b})`;
  $("#preview").style.setProperty("--selected",c);$("#rgbValue").textContent=`${state.r}, ${state.g}, ${state.b}`;
  if(document.activeElement!==$("#brightness"))$("#brightness").value=state.brightness;
  $("#brightnessValue").textContent=state.brightness+"%";
  if(document.activeElement!==$("#speed"))$("#speed").value=state.speed;
  $("#speedValue").textContent=state.speed+"%";
  if(document.activeElement!==$("#intensity"))$("#intensity").value=state.intensity;
  $("#intensityValue").textContent=state.intensity+"%";
  $("#currentMode").textContent=state.effect;
  $("#powerBtn").style.opacity=state.power?1:.45;
  $$(".effect").forEach(x=>x.classList.toggle("active",x.dataset.effect===state.effect));
  setWheelFromRgb()
}
const colors=[
  {rgb:[255,0,0],label:"Red"},{rgb:[0,255,0],label:"Green"},{rgb:[0,0,255],label:"Blue"},
  {rgb:[255,190,0],label:"Amber"},{rgb:[170,0,255],label:"Violet"},{rgb:[0,220,190],label:"Turquoise"},
  {rgb:[255,128,40],label:"Warm white"},{rgb:[255,255,255],label:"White"}
];
colors.forEach(({rgb,label})=>{
  const b=document.createElement("button");b.className="swatch";b.style.setProperty("--c",`rgb(${rgb})`);b.dataset.label=label;b.title=label;
  b.onclick=()=>{[state.r,state.g,state.b]=rgb;state.power=true;state.effect="static";render();scheduleColor()};
  $("#quickColors").appendChild(b)
});
$$(".tab").forEach(b=>b.onclick=()=>{
  $$(".tab").forEach(x=>x.classList.remove("active"));$$(".page").forEach(x=>x.classList.remove("active"));
  b.classList.add("active");$("#"+b.dataset.page).classList.add("active")
});
$("#hueWheel").addEventListener("pointerdown",e=>{dragging=true;$("#hueWheel").setPointerCapture(e.pointerId);pick(e)});
$("#hueWheel").addEventListener("pointermove",e=>dragging&&pick(e));
$("#hueWheel").addEventListener("pointerup",()=>dragging=false);
$("#brightness").oninput=e=>{state.brightness=+e.target.value;render();scheduleControl("/api/brightness",{value:state.brightness})};
$("#speed").oninput=e=>{state.speed=+e.target.value;render();scheduleControl("/api/effect",{name:state.effect,speed:state.speed,intensity:state.intensity})};
$("#intensity").oninput=e=>{state.intensity=+e.target.value;render();scheduleControl("/api/effect",{name:state.effect,speed:state.speed,intensity:state.intensity})};
$("#powerBtn").onclick=()=>{state.power=!state.power;api("/api/power",{value:state.power?1:0});render()};
$$(".effect").forEach(b=>b.onclick=()=>{state.effect=b.dataset.effect;state.power=state.effect!=="off";api("/api/effect",{name:state.effect,speed:state.speed,intensity:state.intensity});render()});

function toggle(el,value){el.classList.toggle("on",!!value);el.dataset.value=value?1:0}
$$(".toggle").forEach(x=>x.onclick=()=>toggle(x,x.dataset.value!=="1"));
function updateActionHelp(){
  $("#input1Help").textContent=actionHelp[$("#input1Action").value]||"";
  $("#input2Help").textContent=actionHelp[$("#input2Action").value]||""
}
$$(".input-action").forEach(x=>x.onchange=updateActionHelp);

async function load(full=false){
  try{
    const d=await api("/api/state");Object.assign(state,d.state);
    if(full||!settingsLoaded){
      $("#deviceName").value=d.settings.deviceName;$("#deviceTitle").textContent=d.settings.deviceName;
      $("#ledCount").value=d.settings.ledCount;$("#colorOrder").value=d.settings.colorOrder;
      $("#maxBrightness").value=d.settings.maxBrightness;$("#defaultFade").value=d.settings.defaultFade;
      $("#mdnsName").value=d.settings.mdnsName;
      $("#input1Action").value=d.settings.input1Action;$("#input2Action").value=d.settings.input2Action;
      toggle($("#input1Enabled"),d.settings.input1Enabled);toggle($("#input2Enabled"),d.settings.input2Enabled);
      toggle($("#restoreState"),d.settings.restoreState);toggle($("#smoothTransitions"),d.settings.smoothTransitions);
      toggle($("#warmCompensation"),d.settings.warmCompensation);
      updateActionHelp();settingsLoaded=true
    }
    $("#connectionText").textContent=d.network.ip||"—";render()
  }catch(e){$("#connectionText").textContent="Offline"}
}
$("#saveSettings").onclick=async()=>{
  await api("/api/settings",{
    deviceName:$("#deviceName").value,ledCount:$("#ledCount").value,colorOrder:$("#colorOrder").value,
    maxBrightness:$("#maxBrightness").value,restoreState:$("#restoreState").dataset.value||0,
    smoothTransitions:$("#smoothTransitions").dataset.value||0,defaultFade:$("#defaultFade").value,
    warmCompensation:$("#warmCompensation").dataset.value||0,
    mdnsName:$("#mdnsName").value,
    input1Enabled:$("#input1Enabled").dataset.value||0,input2Enabled:$("#input2Enabled").dataset.value||0,
    input1Action:$("#input1Action").value,input2Action:$("#input2Action").value
  });
  toast("Settings saved — restart if the color order changed");setTimeout(()=>load(true),300)
};
$("#restartBtn").onclick=()=>confirm("Restart the controller?")&&api("/api/restart",{}).then(()=>toast("Restarting"));
$("#wifiResetBtn").onclick=()=>confirm("Erase saved Wi-Fi and restart?")&&api("/api/reset-wifi",{});
$("#factoryResetBtn").onclick=()=>confirm("Erase all controller settings?")&&api("/api/factory-reset",{});
drawWheel();addEventListener("resize",setWheelFromRgb);setInterval(()=>{if(!dragging)load(false)},900);load(true);
</script>
</body>
</html>
)HTML";