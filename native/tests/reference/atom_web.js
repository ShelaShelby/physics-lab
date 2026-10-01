// Loads the web Atom Lab (index.html, its helpers and SIMS['Atom Lab']) into a Node sandbox, with
// small stand-ins for the browser page, and returns the sandbox so tools and tests can call the
// web's own functions. Math.random is replaced by a seeded generator (mulberry32), so runs can
// be repeated exactly and compared with the native app, which uses the same generator.
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const {makeEl} = require('./fake_dom.js');

function mulberry32(seed) { return () => { seed |= 0; seed = seed + 0x6D2B79F5 | 0; let t = Math.imul(seed ^ seed >>> 15, 1 | seed); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0)/4294967296; }; }

function loadAtomWeb() {
  const html = fs.readFileSync(path.join(__dirname, '../../../index.html'), 'utf8');
  const lines = html.split('\n');
  const find = (re, from = 0) => { for (let i = from; i < lines.length; i++) if (re.test(lines[i])) return i; throw new Error('not found ' + re); };
  const a = find(/^\/\* --- Atom Lab: hydrogen-like atom in atomic units/);
  const b = find(/^SIMS\['Black Hole Lab'\] = \{/);
  // the units helpers the Atom Lab uses come from the Orbit Lab part of the page
  const unitsStart = find(/^const MI_KM = 0\.621371192/), unitsEnd = find(/^const fmtSun = /);   // up to and including uText
  const src = lines.slice(unitsStart, unitsEnd).join('\n') + '\n' + lines.slice(a, b).join('\n')
    .replace('/* ============ 2. ENGINE ============ */', '');
  const engine = src.indexOf('const cv = document.getElementById(\'cv\')');
  const code = (engine > 0 ? src.slice(0, engine) : src);
  const rnd = {f: mulberry32(1)};
  const M = Object.create(Math); M.random = () => rnd.f();
  const el = () => ({style: {}, classList: {add() {}, remove() {}, toggle() {}}, appendChild() {}, append() {}, addEventListener() {}, setAttribute() {}, getContext: () => null, querySelector: () => null});
  const ctx = {
    Math: M, JSON, Date, console, Number, String, Array, Object, Float32Array, Int8Array, Uint8Array, Float64Array, Set, Map, Infinity, NaN, isFinite, parseFloat, parseInt,
    window: {devicePixelRatio: 1}, localStorage: {getItem: () => null, setItem() {}, removeItem() {}},
    document: {addEventListener() {}, createElement: makeEl, getElementById: el, body: el()},
    performance: {now: () => 0}, addEventListener() {},
    PL_UN: 'metric', PLAch: {grant() {}}, Option: function (t, v) { this.text = t; this.value = v; }, plRM: () => false, paused: false, SIMS: {},
    setParam: (k, v) => { ctx.params[k] = v; }, params: {}, state: null, cv: {clientWidth: 1000, clientHeight: 700},
  };
  vm.createContext(ctx);
  vm.runInContext(code + `
    globalThis.W = {uText, NAME, SYMBOL, NUC, DEFAULT_N, N_RANGE, decInfo, decMove, decWord, decShots, DEC_SHORT, isoLine, isoInfo, fmtLife, fmtHalf, decTick, decLine,
      hasStable, fmtLam, fmtLen, fmtT, fmtEV, CATS, CAT, FAMOUS, ptPos, nuclideLabel, ANG, ORB, lagCoef, fullOrb, EXT, psi, psi2, walk, dipoleOK, mcmc,
      FM_AU, NUCLEON_R, nuclearR, fitSize, sizeA0, px0, nucR, sci, cutOffset, supDigits, metres, MAGIC_N, MAGIC_Z, BETA_KNOWN, nucBeta, nucMeasured, nucShapeWord,
      buildNucleus, lvlN, lvl, curLevel, targets, rydM, specLines, photonInfo, lamColor, startJump, beginTransition, manualJump, advance, ionLabel, A0, HARTREE, AU_T,
      massBinding, BEA, TEMPD, tempState, tempLine, factsHTML, elemCardHTML, everyday, EVERYDAY, periodOf, lrnSci, lrnSci3, lrnLen, LRN_OBJ, LRN_LAND, LRN_TEN, LRN_MASS, LRN_TL,
      lrnLook, lrnScale, lrnMass, lrnTimeline, T_ROOM, T_MAX, fK, fC, fF, tAll, STATE_ICON, SIM: SIMS['Atom Lab'], get DEC_CHAIN() { return DEC_CHAIN; }, set DEC_CHAIN(v) { DEC_CHAIN = v; }};`, ctx);
  return {W: ctx.W, ctx, seed: s => { rnd.f = mulberry32(s); }, mulberry32};
}
module.exports = {loadAtomWeb, mulberry32};
