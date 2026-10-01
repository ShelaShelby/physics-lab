// A very small stand-in for the browser's document, enough for the web's Learn guide code
// (lrnEl, appendChild, innerHTML, style, textContent). serialize() writes what was built as HTML
// in one fixed form that the native app's guide code also produces; interactive rows of buttons
// (lrnChips, lrnTabs, the element bar) are written as [[chips]], as the native app draws those itself.
'use strict';
function makeEl(tag) {
  const e = {tagName: tag, className: '', children: [], _html: null, _text: null, _style: [], dataset: {}, attrs: {},
    set innerHTML(v) { this._html = String(v); this.children = []; }, get innerHTML() { return this._html || ''; },
    set textContent(v) { this._text = String(v); this._html = null; this.children = []; }, get textContent() { return this._text || ''; },
    appendChild(c) { if (c !== '' && c != null) this.children.push(c); return c; },
    replaceWith() {}, get clientWidth() { return 600; },
    append(...cs) { cs.forEach(c => this.appendChild(c)); },
    after() {}, setAttribute(k, v) { this.attrs[k] = v; }, scrollIntoView() {}, addEventListener() {},
    get firstChild() { return this.children[0]; }, querySelector: () => makeEl('div'), querySelectorAll: () => [], classList: {add() {}, remove() {}, toggle() {}},
    getContext: () => fakeCtx(), add(o) { this.children.push(o); },
  };
  e.style = new Proxy({}, {set(t, k, v) { e._style.push([k, String(v)]); return true; }, get(t, k) { return k === 'setProperty' ? () => {} : ''; }});
  return e;
}
// a canvas context that draws nothing
function fakeCtx() {
  const noop = () => {};
  return new Proxy({}, {get(t, k) { if (k in t) return t[k]; if (k === 'measureText') return s => ({width: String(s).length * 6}); if (k === 'createRadialGradient' || k === 'createLinearGradient') return () => ({addColorStop: noop}); if (k === 'getTransform') return () => ({a: 1}); return noop; }, set(t, k, v) { t[k] = v; return true; }});
}
function serialize(e) {
  if (typeof e === 'string') return e;
  if (!e || !e.tagName) return '';
  if (/\blrnChips\b|\blrnTabs\b|\blrnEb\b/.test(e.className)) return '[[chips]]';
  const st = e._style.length ? ` style="${e._style.map(([k, v]) => k + ':' + v).join(';')}"` : '';
  const inner = (e._html || '') + (e._text || '') + e.children.map(serialize).join('');
  return `<${e.tagName}${e.className ? ` class="${e.className}"` : ''}${st}>${inner}</${e.tagName}>`;
}
const fold = s => s.replace(/\s+/g, ' ').trim();   // runs of spaces and line breaks fold into one space, as in a browser
module.exports = {makeEl, serialize, fold};
