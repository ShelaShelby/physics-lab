// Loads the web Physics Lab's app-wide code (index.html: history, language, About, Settings,
// achievements, themes, the "Did you know?" line, the Learn and Achievements tabs) into a Node
// sandbox with a small fake page, so tools and tests can run the web's own handlers and read
// what they build. Math.random and Date.now are fixed, so runs repeat exactly.
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const {makeEl} = require('./fake_dom.js');

function loadAppWeb(opts = {}) {
  const html = fs.readFileSync(path.join(__dirname, '../../../index.html'), 'utf8');
  const lines = html.split('\n');
  const find = (re, from = 0) => { for (let i = from; i < lines.length; i++) if (re.test(lines[i])) return i; throw new Error('not found ' + re); };
  const cut = (a, b) => lines.slice(a, b).join('\n');
  // the pieces of the page's script, in the order they appear
  const learn = cut(find(/^const LEARN = \{/), find(/^\/\* --- Orbit: several planets/));
  const lrnEl = lines[find(/^const lrnEl = /)];
  const panes = cut(find(/^function paintAch\(\)/), find(/^var paneName = null;/));
  const app = cut(find(/^\/\* --- Physics Lab's own history/), find(/^document\.getElementById\('logX'\)\.onclick/));
  const storage = new Map(Object.entries(opts.storage || {}));
  const els = {}, body = makeEl('body');
  const pageIds = ['dyk', 'dykT', 'dykC', 'themeBtn', 'langBtn', 'aboutBtn', 'settingsBtn', 'appLog', 'ver', 'kAch', 'kLearn', 'kSim', 'kGame', 'logBg', 'logList', 'logTitle', 'logX'];
  const findIn = (e, id) => { if (!e || typeof e !== 'object') return null; if (e.id === id) return e; for (const c of e.children || []) { const f = findIn(c, id); if (f) return f; } return null; };
  // the page's own elements always exist; anything else (pop-ups made later) only once it is on the page
  const byId = id => pageIds.includes(id) ? (els[id] || (els[id] = makeEl(id === 'dyk' ? 'button' : 'div'))) : findIn(body, id);
  const timers = [];
  const ctx = {
    Math: Object.assign(Object.create(Math), {random: () => 0.5}), JSON, console, Number, String, Array, Object, Set, Map, Event: function () {},
    Date: Object.assign(function (t) { return new Date(t === undefined ? 1790000000000 : t); }, {now: () => 1790000000000}),
    localStorage: {getItem: k => storage.has(k) ? storage.get(k) : null, setItem: (k, v) => storage.set(k, String(v)), removeItem: k => storage.delete(k)},
    document: {getElementById: byId, createElement: makeEl, body, documentElement: Object.assign(makeEl('html'), {getAttribute: k => ctx._attr[k] ?? null, setAttribute: (k, v) => { ctx._attr[k] = v; }, removeAttribute: k => { delete ctx._attr[k]; }, toggleAttribute() {}}),
      addEventListener() {}, removeEventListener() {}},
    _attr: {}, window: null, addEventListener() {}, removeEventListener() {}, innerWidth: 1400, innerHeight: 900,
    setTimeout: (f, t) => { timers.push([f, t]); return timers.length; }, clearTimeout() {}, requestAnimationFrame() {},
    matchMedia: () => ({matches: false}),
    PL_SIZES: {small: .9, normal: 1, large: 1.15, xl: 1.3}, PL_TS: 1, PL_FF: null, PL_MOTION: 'auto', PL_UN: 'metric',
    PL_FONTS: null, plFontLoad() {}, plRM: () => ctx.PL_MOTION === 'on',
    SIMS: {'Orbit Lab': {update: '7.7.2'}, 'Atom Lab': {update: '7.7.3'}}, sim: {changelog: []}, curName: 'Atom Lab', paused: false, state: {}, params: {},
    setPaused(v) { ctx.paused = v; }, kindOf: () => 'sim', fillKind() {}, load() {}, setParam() {}, lrnOpen: (t, build) => { ctx.lastTitle = t; ctx.lastBox = makeEl('div'); build(ctx.lastBox); },
    lrnClose() {}, lrn: {}, achPane: makeEl('div'), achLayout: makeEl('div'), orderedNames: ['Orbit Lab', 'Atom Lab'],
  };
  ctx.window = ctx;
  // PL_FONTS comes from the page's first script (line 206)
  ctx.PL_FONTS = vm.runInNewContext('(' + /window\.PL_FONTS=(\{.*?\});/.exec(html)[1] + ')');
  const css = fs.readFileSync(path.join(__dirname, '../../../index.html'), 'utf8');
  vm.createContext(ctx);
  vm.runInContext(lrnEl + '\n' + learn + '\n' + panes + '\n' + app + `
    globalThis.A = {APP_LOG, APP_PLAN, appTimeline, DYK, THEMES, PLAch, PLTheme, LEARN, paintAch, paintLearn, TEXT_SIZES};`, ctx);
  return {A: ctx.A, ctx, els: byId, body, storage, timers, html: css};
}
module.exports = {loadAppWeb};
