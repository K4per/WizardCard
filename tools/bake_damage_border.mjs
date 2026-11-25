// Rebuild with: node tools/bake_damage_border.mjs <path-to-pixelfx-checkout>
// PixelFX renderer: https://github.com/patch-gg/pixelfx (5294518ce7c7, MIT).
// Each original motif occupies a short strip. Godot places it along the hit player's field.
import { mkdirSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

if (!process.argv[2]) throw new Error("Pass the path to the PixelFX checkout.");
const upstream = pathToFileURL(resolve(process.argv[2]) + "/");
const { Painter, createFrame, composite, hex } = await import(new URL("src/core/index.js", upstream));
const { encodePNG } = await import(new URL("tools/lib/png.mjs", upstream));
const width = 64, height = 30, frames = 18;
const clamp = x => Math.max(0, Math.min(1, x));
const tau = Math.PI * 2;
const palette = (...colors) => colors.map(hex);
const white = hex("#ffffff");
const steel = palette("#66778d", "#adbdcf", "#eff6ff");

function glint(p, x, y, radius, color) {
  p.line(x - radius, y, x + radius, y, color);
  p.line(x, y - radius, x, y + radius, color);
  p.px(x, y, white);
}

function acid(p, t) {
  const [shade, green, lemon] = palette("#236829", "#69ef32", "#f0ff91");
  for (const [base, offset, size] of [[15, 0.25, 3], [29, 0.02, 6], [44, 0.46, 4], [50, 0.12, 2]]) {
    const age = (t * 1.3 + offset) % 1;
    const x = base + 2 * Math.sin(age * 7 + base), y = 25 - age * 20;
    const r = size * (0.5 + age);
    if (age < 0.78) {
      p.disc(x, y, r, shade, 0.8);
      p.ring(x, y, r, 1.4, green);
      p.line(x - r / 2, y - r / 2, x - r / 2 + 1, y - r / 2 - 1, lemon);
      p.glow(x, y, r + 4, green, 0.19);
    } else {
      for (let k = 0; k < 5; k++) {
        const a = k * tau / 5, d = size + (age - 0.78) * 15;
        p.dot(x + Math.cos(a) * d, y + Math.sin(a) * d, lemon, (1 - age) * 4.5);
      }
    }
  }
}

function cold(p, t) {
  const [deep, ice, facet, rim] = palette("#254585", "#438fe7", "#88e6ff", "#e9ffff");
  for (const [x, delay, length, lean, w] of [[18, 0, 14, -6, 5], [25, 0.2, 18, 3, 5], [33, 0.06, 26, 7, 7], [41, 0.21, 13, 7, 4], [48, 0.34, 11, 5, 4]]) {
    const rise = Math.sin(clamp((t - delay) * 4) * Math.PI / 2);
    if (rise === 0) continue;
    const tip = [x + lean * rise, 28 - length * rise], kink = [x + 1, 25 - length * rise * 0.45];
    p.poly([[x - w, 28], tip, [x + w, 28]], deep);
    p.poly([[x - w, 28], tip, kink], facet);
    p.poly([kink, tip, [x + w, 28]], ice);
    p.line(x - w, 27, ...tip, rim);
    if (Math.abs(t - delay - 0.45) < 0.07) glint(p, ...tip, 2, rim);
  }
  const radius = 7.6 * Math.sin(clamp((t - 0.12) * 3.1) * Math.PI / 2);
  if (radius < 1) return;
  for (let i = 0; i < 6; i++) {
    const a = i * tau / 6 + Math.PI / 6, dx = Math.cos(a), dy = Math.sin(a);
    const cx = 16, cy = 15, fork = radius * 0.57;
    p.line(cx, cy, cx+radius*dx, cy+radius*dy, facet);
    for (const turn of [-0.8, 0.8]) {
      p.line(cx+fork*dx, cy+fork*dy,
        cx+fork*dx+radius*0.39*Math.cos(a+turn), cy+fork*dy+radius*0.39*Math.sin(a+turn), rim);
    }
  }
}

function flameTongue(p, x, y, h, width, phase, colors) {
  for (const [i, k] of [1, 0.79, 0.51, 0.25].entries()) {
    const left = [], right = [];
    for (let j = 0; j <= 10; j++) {
      const up = j / 10, sway = up * 5.5 * Math.sin(phase + up * 4.6);
      const center = x + sway * k, py = y - up * h * k;
      const half = width * k * Math.pow(1 - up, 0.7) * (1 + 0.26 * Math.sin(up * 7));
      left.push([center-half, py]);
      right.push([center+half, py]);
    }
    p.poly([...left, ...right.reverse()], colors[i]);
  }
}

function fire(p, t) {
  const colors = palette("#b52b26", "#ff6123", "#ffb132", "#fff2a4");
  for (const [x, extent, w] of [[12,12,4], [19,19,5], [27,28,6], [35,14,6], [42,24,5], [49,12,4]]) {
    const phase = t * 10 + x, h = extent * (0.79 + 0.18 * Math.sin(phase));
    flameTongue(p, x, 28, h, w, phase, colors);
    p.dot(x + 3 * Math.cos(phase), 25 - h - ((t*12+x) % 4), colors[2], 0.8);
  }
}

function lightning(p, t) {
  const [violet, lilac, flash] = palette("#7436e6", "#bf70ff", "#f4dfff");
  const tick = Math.floor(t * 10);
  const points = [[2,21], [12,16], [17,19], [23,9], [27,15], [38,10], [44,14], [53,8], [61,10]];
  for (let i = 1; i < points.length - 1; i++) points[i][1] += Math.round(3 * Math.sin(tick + i * 3.4));
  for (let i = 1; i < points.length; i++) p.beam(...points[i-1], ...points[i], 4, violet);
  p.polyline(points, flash);
  p.polyline([points[3], [24,21], [20,24], [21,27]], lilac);
  p.polyline([points[6], [40,7], [43,2]], lilac);
}

function poison(p, t) {
  const [dark, cloud, face, light] = palette("#34483f", "#748777", "#a4b89a", "#d3deac");
  const x = 33 + Math.sin(t * 6) * 2, y = 17 - 6 * t;
  for (const [dx, dy, r] of [[-10,5,5], [-6,2,7], [8,3,6], [13,1,4], [-2,7,6]]) {
    p.disc(x + dx + Math.sin(t * 5 + dx), y + dy, r * (0.8 + 0.32 * t), cloud, d => 0.65 - d * 0.4);
  }
  p.ellipse(x, y, 8, 7, dark);
  p.ellipse(x - 1, y - 1, 7, 6, face);
  p.rect(x - 4, y + 3, 8, 5, face);
  p.ellipse(x - 3.4, y, 2.1, 2.1, dark);
  p.ellipse(x + 2.5, y, 2.1, 2.1, dark);
  p.px(x, y + 3, dark);
  p.line(x - 2, y + 7, x - 2, y + 9, dark);
  p.line(x + 1, y + 7, x + 1, y + 9, dark);
  p.line(x - 4, y - 5, x, y - 6, light);
}

function thunder(p, t) {
  const [shadow, iris, edge] = palette("#513578", "#9968d4", "#ca9ff1");
  for (let wave = 0; wave < 3; wave++) {
    const age = (t + wave / 3) % 1;
    const amplitude = 3 + 5 * Math.sin(age * Math.PI);
    const points = Array.from({length: 39}, (_, i) => [9 + i * 1.2, 26 - age * 18 + amplitude * Math.sin(i / 38 * tau * 1.5 - t * 9) * Math.sin(i / 38 * Math.PI)]);
    for (let i = 1; i < points.length; i++) p.beam(...points[i-1], ...points[i], 3, shadow);
    p.polyline(points, wave === 0 ? edge : iris);
  }
}

function radiant(p, t) {
  const [bronze, gold, sun, light] = palette("#d88827", "#ffc647", "#ffdf80", "#fff9ca");
  const pulse = 0.8 + 0.2 * Math.sin(t * 11);
  const x = 32, y = 28;
  for (let i = 0; i < 9; i++) {
    const a = -Math.PI * (0.08 + 0.84 * i / 8), d = (i % 2 ? 19 : 29) * pulse;
    const dx = Math.cos(a), dy = Math.sin(a);
    p.poly([[x + dx*4, y + dy*4], [x + dx*10 - dy*2, y + dy*10 + dx*2], [x + dx*d, y + dy*d], [x + dx*11 + dy*1.6, y + dy*11 - dx*1.6]], i % 2 ? gold : sun);
  }
  const colors = [bronze, gold, sun, light];
  for (const [dx, h] of [[-6,10], [0,19], [6,13]]) {
    flameTongue(p, x+dx, y, h*pulse, 4, t*8+dx, colors);
  }
  glint(p, 49, 11 + Math.sin(t * 6) * 2, 2, sun);
}

function necrotic(p, t) {
  const [ink, shadow, ridge, glimmer] = palette("#0e101b", "#212131", "#515264", "#797987");
  // Interlocking banks share a wisp. Broken highlights describe the upturned folds.
  for (const [x, phase, size] of [[15,0.35,5], [22,1.8,8], [29,0.12,9], [36,3.0,8], [43,1.3,6], [49,2.9,4]]) {
    const drift = Math.sin(t * 5 + phase), cx = x + 2.3 * drift;
    const cy = 21 - 7 * Math.sin((x - 11) / 44 * Math.PI) - 3 * drift;
    p.disc(cx, cy+3, size + 1, ink, d => 0.96 - 0.24*d);
    p.disc(cx+1, cy, size, shadow, d => 0.92 - 0.26*d);
    p.disc(cx-1.6, cy-1.5, size*0.78, ridge, d => 0.68 - 0.43*d);
    p.disc(cx+0.5, cy+0.6, size*0.68, shadow, 0.9);
    p.disc(cx+0.5, cy+size*0.6, size*0.7, ink, d => 0.85 - 0.3*d);
    p.line(cx-size*0.5,cy-size*0.55,cx-size*0.15,cy-size*0.72,glimmer,0.65);
  }
  const tail = 15 + Math.cos(t*6)*2;
  p.polyline([[29,12], [25,8], [tail+4,7], [tail,5], [tail+1,3]], ridge, 0.9);
}

function force(p, t) {
  const [red, light] = palette("#ff3846", "#ff9b90");
  for (const [y, offset, length] of [[26,0,17], [24,0.52,9], [28,0.28,13]]) {
    const head = (t * 51 + offset * 47) % 44 + 9;
    p.line(head-length, y, head, y, red);
    p.line(head-4, y, head, y, light);
  }
}

function psychic(p, t) {
  const [plum, rose, pale] = palette("#ae3e81", "#ff84c0", "#ffe5f3");
  for (let i = 0; i < 3; i++) {
    const age = (t + i/3) % 1, rx = 5+20*age, ry = 2+8*age;
    const points = Array.from({length: 50}, (_, j) => {
      const a = j * tau / 49;
      return [32 + Math.cos(a)*rx, 16 + Math.sin(a)*ry + 1.4*Math.sin(a*2+t*7)];
    });
    p.polyline(points, i === 0 ? pale : i === 1 ? rose : plum);
  }
}

function piercing(p, t) {
  for (const [x, offset, length, w] of [[23,0.07,18,2.6], [32,0,24,3.3], [42,0.17,16,2.5]]) {
    const hit = Math.sin(clamp((t - offset) * 4) * Math.PI / 2);
    const tip = 27 - hit * length + Math.max(t - 0.53, 0) * 12;
    const blade = Math.max(2, (24-tip)*0.54);
    p.line(x,tip+blade,x,27,steel[1]);
    p.poly([[x-w,tip+blade], [x,tip], [x+w,tip+blade], [x,tip+blade-1]], steel[0]);
    p.poly([[x-w,tip+blade], [x,tip], [x,tip+blade-1]], steel[2]);
    p.line(x, tip, x, tip+blade-2, white);
    if (hit > 0.92 && t < 0.45 + offset) glint(p, x, tip, 2, white);
  }
}

function bludgeoning(p, t) {
  const strike = clamp((t - 0.1) * 5);
  const angle = -0.72 + 1.05 * strike, c = Math.cos(angle), s = Math.sin(angle);
  const y = 6 + 9 * strike - Math.max(t - 0.62, 0) * 12;
  const at = (x, dy) => [36 + x*c - dy*s, y + x*s + dy*c];
  p.beam(...at(-18,8), ...at(0,0), 3, steel[0]);
  p.line(...at(-18,7), ...at(0,0), steel[2]);
  p.poly([at(-4,-9), at(3,-9), at(6,-6), at(6,6), at(3,8), at(-4,8), at(-6,5), at(-6,-7)], steel[1]);
  p.poly([at(-4,-9), at(3,-9), at(6,-6), at(-4,-5), at(-6,-7)], white);
  p.poly([at(3,-5), at(6,-6), at(6,6), at(3,8)], steel[0]);
  p.line(...at(-5,5), ...at(3,5), white);
  if (strike > 0.84) {
    const d = 5 + 5 * t;
    p.polyline([[23-d,24], [26,22], [31,25], [36,22], [44+d,23]], steel[2]);
    p.line(28,22,24-d*0.65,18, steel[1]);
    p.line(40,22,45+d*0.65,16, steel[1]);
  }
}

function slashing(p, t) {
  for (const [offset, y, bright] of [[0,0,true], [0.22,5,false]]) {
    const front = clamp((t - offset) * 4), tail = clamp((t - offset - 0.38) * 1.8);
    if (front <= tail) continue;
    const outer = [], inner = [];
    for (let i = 0; i <= 20; i++) {
      const q = tail + (front - tail) * i / 20;
      const x = 7 + q * 49, cy = 24 - 18 * q - 6 * Math.sin(q * Math.PI) + y;
      outer.push([x, cy]);
      inner.push([x, cy + 4.2 * Math.sin(i/20 * Math.PI)]);
    }
    p.poly([...outer, ...inner.reverse()], bright ? steel[1] : steel[0]);
    p.polyline(outer, bright ? white : steel[2]);
    if (front < 0.99) glint(p, ...outer.at(-1), 2, white);
  }
}

function overload(p, t) {
  const [deep, ember, spark] = palette("#a52b26", "#ff6134", "#ffe68b");
  const pulse = Math.sin(clamp(t * 1.55) * Math.PI);
  for (const [x, phase, reach] of [[12,0.1,13], [21,0.42,17], [32,0.2,21], [44,0.56,15], [54,0.3,12]]) {
    const rise = Math.max(0, Math.sin(t * 9 + phase)) * (0.45 + pulse * 0.55);
    const tip = 26 - rise * reach;
    p.poly([[x-3,28],[x-1,tip+3],[x+1,tip],[x+2,tip+5],[x+4,28]], deep);
    p.poly([[x-1,28],[x,tip+3],[x+1,tip+1],[x+2,28]], ember);
    if (rise > 0.55) p.px(x+1, tip, spark);
  }
  for (let i=0;i<5;i++) {
    const age=(t*1.2+i/5)%1, x=8+i*12+Math.sin(age*9+i)*3, y=25-age*21;
    p.dot(x,y,age<0.72?ember:spark,age<0.72?2.1:1.4);
  }
}

function exhaustion(p, t) {
  const [ink, slate, ash, pale] = palette("#151a27", "#454957", "#818592", "#c4c6cb");
  const fade = 1-clamp(t*0.7);
  for (const [x, phase] of [[9,0.1],[20,0.34],[32,0.06],[44,0.42],[55,0.22]]) {
    const pulse = Math.max(0, Math.sin((t+phase)*Math.PI*2))*fade;
    const drop = 5 + 8*pulse;
    p.poly([[x-4,1],[x-1,9],[x-3,15],[x+1,19],[x-2,28],[x+3,20],[x+2,14],[x+5,8],[x+2,3]],ink);
    p.poly([[x-1,4],[x+1,11],[x,15],[x+2,19],[x,24]],slate);
    p.line(x,15,x+drop,18+drop*0.3,ash);
    if (pulse>0.4) { p.px(x-2,24,pale); p.px(x+drop,19+drop*0.3,ash); }
  }
  for (let i=0;i<4;i++) {
    const y=4+((t*14+i*7)%23), x=7+i*15+2*Math.sin(t*5+i);
    p.dot(x,y, i%2?slate:ash, 1.4);
  }
}

function manaRecovery(p, t) {
  const [deep, violet, blue, glintColor] = palette("#36327e", "#8067df", "#54c7f5", "#d4fbff");
  for (let wave=0;wave<3;wave++) {
    const age=(t+wave/3)%1, radius=4+age*25;
    const points=Array.from({length:22},(_,i)=>{
      const a=i*tau/21;
      return [32+Math.cos(a)*radius, 18+Math.sin(a)*radius*0.43];
    });
    p.polyline(points,wave===0?glintColor:wave===1?blue:violet);
  }
  for (let i=0;i<4;i++) {
    const age=(t*1.25+i/4)%1,x=14+i*12+Math.sin(age*6+i)*2,y=26-age*22;
    p.poly([[x,y-4],[x+3,y],[x,y+3],[x-3,y]],age<0.75?blue:glintColor);
    p.line(x,y-3,x,y+2,deep);
    p.line(x-2,y,x+2,y,violet);
  }
}

function lifeRecovery(p, t) {
  const colors=palette("#27623e","#4eb877","#a3f1a0","#efffba");
  const pulse=0.6+0.4*Math.sin(t*tau*2);
  for (const [x,phase,size] of [[10,0.1,4],[21,0.4,5],[32,0.05,6],[44,0.32,5],[55,0.17,4]]) {
    const rise=(t*0.72+phase)%1, sway=2.5*Math.sin(t*7+phase*tau);
    const y=27-rise*21, half=size*(0.4+0.6*pulse);
    p.poly([[x+sway,y-half*2],[x+sway+half,y],[x+sway,y+2],[x+sway-half,y]],colors[1]);
    p.line(x+sway-half*0.65,y+1,x+sway+half*0.65,y-half,colors[3]);
    p.line(x+sway,y-1,x+sway+2,y-half*1.45,colors[0]);
    if (rise>0.3 && rise<0.9) p.dot(x+sway-half,y-1,colors[2],1.4);
  }
  for(let i=0;i<3;i++) {
    const age=(t+i/3)%1, rx=5+age*22, y=25-age*8;
    const points=Array.from({length:15},(_,j)=>[32-rx+2*rx*j/14,y-2.2*Math.sin(j/14*Math.PI)]);
    p.polyline(points,age<0.55?colors[2]:colors[1]);
  }
}

const motifs = [acid, cold, fire, lightning, poison, thunder, radiant, necrotic, force, psychic, piercing, bludgeoning, slashing, overload, exhaustion, manaRecovery, lifeRecovery];
const atlasWidth = width * frames, atlasHeight = height * motifs.length;
const atlas = new Uint8Array(atlasWidth * atlasHeight * 4);
for (const [row, motif] of motifs.entries()) {
  for (let f = 0; f < frames; f++) {
    const frame = createFrame(width, height);
    const pixels = new Uint8Array(width * height * 4);
    motif(new Painter(frame), f / (frames - 1));
    composite(frame, pixels);
    for (let y = 0; y < height; y++)
      atlas.set(pixels.subarray(y*width*4, (y+1)*width*4), ((row*height+y)*atlasWidth+f*width)*4);
  }
}
const output = new URL("../godot/art/fx/", import.meta.url);
mkdirSync(output, {recursive: true});
writeFileSync(new URL("damage-border.png", output), encodePNG(atlasWidth, atlasHeight, atlas));
