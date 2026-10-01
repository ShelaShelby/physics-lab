// Copies the Orbit Lab Learn content out of the web version (index.html) into
// native/src/ui/learn_data.inc, word for word:
//  - "What am I even looking at?" (lrnOrbitLook): the web function is RUN here with a pretend page,
//    once for each case its text depends on (2D/3D, True scale/Easy to see, something selected or not).
//    Live values (the date, the selected thing, the Time speed, the barycentre distance) come out as
//    markers like {WHEN} that the app fills in.
//  - "Why do we have seasons?" (lrnSeasons): its fixed paragraphs, in order, and the planets' season texts.
//  - The Solar System timeline (ORB_TL): every step, and what its ▶ button does.
// Run from the repository root:   node native/tools/make_learn_data.js
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const {apply, EDITS} = require('./native_edits.js');   // the few places where the native text differs from the web's
const html = fs.readFileSync(path.join(__dirname, '../../index.html'), 'utf8');

function matchBlock(text, from, open, close) {
  let i = text.indexOf(open, from), depth = 0, q = null;
  for (; i < text.length; i++) {
    const c = text[i];
    if (q) { if (c === '\\') i++; else if (q === '`' && c === '$' && text[i + 1] === '{') { i = matchBlock(text, i, '{', '}') - 1; } else if (c === q) q = null; continue; }
    if (c === '/' && text[i + 1] === '/') { i = text.indexOf('\n', i); continue; }
    if (c === '/' && text[i + 1] === '*') { i = text.indexOf('*/', i) + 1; continue; }
    if (c === "'" || c === '"' || c === '`') { q = c; continue; }
    if (c === open) depth++;
    else if (c === close && --depth === 0) return i + 1;
  }
  throw new Error('no match');
}
const cutFunction = name => { const at = html.indexOf('function ' + name + '('); if (at < 0) throw new Error(name); return html.slice(at, matchBlock(html, at, '{', '}')); };
const cutLine = start => { const at = html.indexOf(start); if (at < 0) throw new Error(start); return html.slice(at, html.indexOf('\n', at)); };
const cutArray = start => { const at = html.indexOf(start); if (at < 0) throw new Error(start); return html.slice(at, matchBlock(html, at, '[', ']')) + ';'; };

// ---- a pretend page: elements only remember their class, html and children ----
const stubs = `
const lrnEl = (tag, cls, h) => ({tag, cls: cls || '', html: h === undefined ? '' : h, kids: [], set textContent(v) { this.kids = []; this.html = ''; }, set innerHTML(v) { this.html = v; },
  append(...k) { this.kids.push(...k); }, appendChild(k) { this.kids.push(k); }, style: {}, dataset: {}, firstChild: {appendChild() {}}});
const lrnSecP = (key, title, h) => ({sec: true, key, title, html: h});
const lrnJumpBar = list => ({jumpbar: true});
`;
const out = [];
const q = s => JSON.stringify(s);
const lit = s => { if (!s) return q(""); const parts = []; for (let i = 0; i < s.length; i += 1500) parts.push(q(s.slice(i, i + 1500))); return parts.join('\n        '); };   // long text split for the compiler

// ---- "What am I even looking at?" ----
{
  const code = stubs + [cutLine('const AU = Math.cbrt(100)'), cutLine('const MAX_PLANETS = 12'), cutFunction('starProps'), cutFunction('lrnOrbitLook')].join('\n') + `
globalThis.run = (view, scale, hasCard) => {
  const box = lrnEl('div');
  globalThis.SIMS = {'Orbit Lab': {info: () => '{WHEN}', params: [{key: 'tspeed', fmt: () => '{TSPEED}'}]}};
  globalThis.cardText = () => hasCard ? {title: '{CARD}'} : null;
  globalThis.baryOf = () => ({x: 0.123456789, y: 0, z: 0});
  globalThis.skyData = () => ({n: 2874});
  lrnOrbitLook(box, {}, {view, scale, GM: 100, tspeed: 0}, null);
  return box;
};`;
  const ctx = {Math, JSON, console}; vm.createContext(ctx); vm.runInContext(code, ctx);
  const variants = [];
  for (const d3 of [false, true]) for (const ts of [true, false]) for (const card of [false, true]) {
    const box = ctx.run(d3 ? '3D' : '2D', ts ? 'True scale' : 'Easy to see', card);
    const intro = box.kids[0].html, outEl = box.kids[5];
    const bdText = (0.123456789/(696000/149.6e6*Math.cbrt(100))).toFixed(1);    // the barycentre distance as the stub makes it: replaced by a marker
    const secs = outEl.kids.filter(k => k.sec).map(s => ({key: s.key, title: s.title, html: (s.key === 'sky' ? apply('lookSky', s.html) : s.html).replace('<b>' + bdText + ' Sun radii</b>', '<b>{BARY} Sun radii</b>').trim()}));
    variants.push({d3, ts, card, intro, secs});
  }
  out.push('// "What am I even looking at?": index = (3D ? 4 : 0) + (Easy to see ? 2 : 0) + (something selected ? 1 : 0)');
  out.push('struct LearnSection { const char* key; const char* title; const char* html; };');
  out.push(`static const char* const kLookIntro = ${q(variants[0].intro)};`);
  out.push(`static const int kLookSectionCount = ${variants[0].secs.length};`);
  out.push('static const LearnSection kLook[8][' + variants[0].secs.length + '] = {');
  for (const v of variants) {
    out.push('  {');
    for (const s of v.secs) out.push(`    {${q(s.key)}, ${q(s.title)},\n        ${lit(s.html)}},`);
    out.push('  },');
  }
  out.push('};');
}

// ---- "Why do we have seasons?": the fixed paragraphs, in order, and the planets' texts ----
{
  const code = stubs + [cutLine('const SEAS_MOMENTS = ['), cutFunction('lrnSeasons')].join('\n') + `
globalThis.run = () => { const box = lrnEl('div'); globalThis.state = {}; globalThis.params = {}; globalThis.earthSeason = () => null; globalThis.fmtUTC = () => ''; lrnSeasons(box, {}, {}, null); return box; };`;
  const ctx = {Math, JSON, console}; vm.createContext(ctx); vm.runInContext(code, ctx);
  const box = ctx.run();
  const blocks = box.kids.map(k => k.tag === 'p' ? {kind: k.cls.includes('lrnMuted') ? 'muted' : 'p', html: k.html}
                                                   : {kind: k.html.includes('<svg') ? 'diagram' : 'slot', html: ''});
  // the three empty boxes, in order: moment buttons, the live numbers, the planets table
  let slot = 0; const names = ['moments', 'live', 'planets'];
  blocks.forEach(b => { if (b.kind === 'slot') b.html = names[slot++]; });
  out.push('// "Why do we have seasons?": the blocks in order. kind: p, muted, diagram, or a slot the app fills (moments, live, planets)');
  out.push('struct LearnBlock { const char* kind; const char* html; };');
  out.push('static const LearnBlock kSeasons[] = {\n' + blocks.map(b => `    {${q(b.kind)}, ${lit(b.html)}},`).join('\n') + '\n};');
  const moments = eval(cutLine('const SEAS_MOMENTS = [').replace('const SEAS_MOMENTS = ', '').replace(/;\s*$/, ''));
  out.push('struct SeasonMoment { const char* name; const char* date; int y, mo, d, h, mi; };   // UTC');
  out.push('static const SeasonMoment kSeasonMoments[] = {\n' + moments.map(([n, d, ms]) => { const t = new Date(ms); return `    {${q(n)}, ${q(d)}, ${t.getUTCFullYear()}, ${t.getUTCMonth() + 1}, ${t.getUTCDate()}, ${t.getUTCHours()}, ${t.getUTCMinutes()}},`; }).join('\n') + '\n};');
  // the planets' season texts (Venus's has its lean in it: written with the marker {VENUS})
  const pnSrc = cutFunction('lrnSeasons'); const at = pnSrc.indexOf('const PN = {');
  const PN = eval('(' + pnSrc.slice(at + 11, matchBlock(pnSrc, at, '{', '}')) + ')');
  out.push('struct PlanetSeason { const char* name; const char* text; };');
  out.push('static const PlanetSeason kPlanetSeasons[] = {\n' + Object.keys(PN).map(n => `    {${q(n)}, ${q(PN[n](180 - 12.345).replace('12.3°', '{VENUS}°'))}},`).join('\n') + '\n};');
  // the live paragraph and the tables, as the web writes them (markers filled in by the app)
  const live = pnSrc.slice(pnSrc.indexOf("live.innerHTML = `<p"), pnSrc.indexOf('`;', pnSrc.indexOf("live.innerHTML = `<p")) + 1);
  out.push('// web source of the live paragraph, for reference:\n// ' + live.replace(/\n/g, ' '));
}

// ---- the timeline ----
{
  const recs = [];
  const code = [cutArray('const ORB_TL = [')].join('\n') + `
globalThis.TL = ORB_TL;`;
  const ctx = {Math, JSON, Date, console,
    orbGo: (name, zoom, set, moons) => recs.push({kind: 'go', name, zoom, moons: !!moons}),
    orbLook: (name, km, set) => recs.push({kind: 'look', name, km}),
    orbFar: (key, ms) => () => recs.push({kind: 'far', key, ms: ms || 0}),
    orbDate: (ms, name, km) => () => recs.push({kind: 'date', ms, name, km}),
    showKind: k => recs.push({kind: 'game'}),   // (the Slingshot button is dropped below; the native app has no such lab)
    state: {}, trueScale: () => true, zoomFor: () => 0};
  vm.createContext(ctx); vm.runInContext(code, ctx);
  const intro = html.match(/lrnTimeline\(box, set, ORB_TL, '([^']*)'/)[1];
  const rows = ctx.TL.map(e => {
    let a = {kind: ''};
    if (e.lab && e.lab[0] === EDITS.slingshotButton) e = {...e, lab: null};   // the Slingshot game is not in the native app: no button
    if (e.lab) { recs.length = 0; e.lab[1](() => recs.push({kind: 'sun'})); a = recs[0] || {kind: 'sun'}; if (a.kind === 'look' && false) {} }
    const ms = a.ms || 0;
    return `    {${q(e.y)}, ${q(e.who)}, ${q(e.t)},\n     ${lit(e.d)},\n     ${q(e.lab ? e.lab[0] : '')}, ${q(a.kind)}, ${q(a.name || a.key || '')}, ${a.zoom ?? 0}, ${a.km ?? 0}, ${a.moons ? 'true' : 'false'}, ${ms}LL,\n     ${q(e.svg)}},`;
  });
  out.push(`static const char* const kTimelineIntro = ${q(intro)};`);
  out.push('// lab: what the ▶ button does: go (name or "", zoom, moons), look (name, km), far (key, optional date), date (ms, name, km), sun');
  out.push('struct TimelineStep { const char* when; const char* who; const char* title; const char* text; const char* button; const char* lab; const char* target; double zoom, km; bool moons; long long ms; const char* svg; };   // svg: the step picture, drawn in a 56 × 56 box');
  out.push('static const TimelineStep kTimeline[] = {\n' + rows.join('\n') + '\n};');
}

fs.writeFileSync(path.join(__dirname, '../src/ui/learn_data.inc'), '// Generated by native/tools/make_learn_data.js from index.html. Do not edit by hand.\n' + out.join('\n') + '\n');
console.log('wrote learn_data.inc');
