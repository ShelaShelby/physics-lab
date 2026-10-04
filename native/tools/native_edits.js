// The ONLY places where the native app's text differs from the web's. The native app has the Orbit Lab and the
// Atom Lab only: the Black Hole Lab, the Spacetime Lab and the Slingshot game are not in it, so the words that
// name them are taken out (and nothing else is changed). The generators use apply(); the tests keep their own,
// separately written list (tests/test_app.cpp, "native edits") and check every text against the web with it.
'use strict';
const GRAVITY_SOURCE = '<li><a href="https://arxiv.org/abs/2112.07478" target="_blank" rel="noopener noreferrer">GRAVITY Collaboration, 2022: mass distribution in the Galactic Centre</a> (Sagittarius A*, 4.3 million Suns)</li>';
const EDITS = {
  // Settings → Units: the Black Hole readout is not in the native app
  units: [{replace: [', the Black Hole readout', '']}],
  // About
  about: [
    {replace: ['real orbits, atoms, black holes and a little space game.', 'real orbits and atoms.']},
    // the sources of the Black Hole Lab and the Spacetime Lab, and the neutron star note under them
    {cut: ['<div class="srcH">🕳️ Black holes</div>', '<div class="srcH">🌌 Stars and far-away things</div>']},
    // ...but the Orbit Lab still says Sagittarius A* is 4.3 million Suns, so that one source stays, under the other Sagittarius A* source
    {replace: ['(27,000 light years)</li></ul>', '(27,000 light years)</li>' + GRAVITY_SOURCE + '</ul>']},
  ],
  // the Sagittarius A* card (far-away things)
  sgra: [{replace: [' (see the Spacetime Lab)', '']}],
  // Orbit guide "The stars and the far-away things"
  lookSky: [{replace: [' (the Spacetime Lab shows what a black hole like it does)', '']}],
  // timeline step 1977–1989: the "Try the slingshot game" button
  slingshotButton: 'Try the slingshot game',
};
// The Atom Lab timeline (LRN_TL, copied by make_atom_learn.js): two steps said things the lab cannot back (0.10.6 stage 5, audit finding 7). Each edit names the step by its web title, the field
// (t = title, d = text, lab = the button's words) and one exact replacement that must match exactly once. tests/test_atom.cpp keeps its own copy of the final words.
const ATOM_TIMELINE_EDITS = [
  // Pauli: the button opened Ne with ONE electron (Ne9+) next to a drawing of five. The lab is a one-electron lab, so the text says it.
  {step: 'Why electrons form shells', field: 'lab', replace: ['See neon in the lab', 'Open a neon nucleus with one electron']},
  {step: 'Why electrons form shells', field: 'd', replace: ['and the different chemistry of each element.', 'and the different chemistry of each element. The lab draws only one electron, so it cannot show a full shell like this: the button opens a neon nucleus with a single electron (Ne⁹⁺).']},
  // 2013: an interference pattern on a detector is not a picture of one of the lab's clouds. (The paper was not reachable in 0.10.6 stage 5: the new words claim only what the web text itself said, no more.)
  {step: 'Photographing an orbital', field: 't', replace: ['Photographing an orbital', 'An electron wave, seen on a detector']},
  {step: 'An electron wave, seen on a detector', field: 'd', replace: ['It is a direct picture of the quantum cloud, the same kind of shape you can open in the Cloud views here.', 'It is a pattern recorded on a detector, evidence for the electron’s wave, not a picture of a cloud like the ones in this lab: those are worked out from the theory.']},
  {step: 'An electron wave, seen on a detector', field: 'lab', replace: ['Open the electron cloud', 'Open a hydrogen cloud (worked out from theory)']},
];
// applies the edits to one step {t, d, lab, ...} of LRN_TL (returns a copy); a failed match throws
function applyTimeline(e) {
  const out = Object.assign({}, e);
  if (out.lab) out.lab = out.lab.slice();
  for (const ed of ATOM_TIMELINE_EDITS) {
    if (out.t !== ed.step) continue;
    const [x, y] = ed.replace;
    const get = () => ed.field === 'lab' ? out.lab[0] : out[ed.field];
    const cur = get();
    if (cur === undefined || cur.split(x).length !== 2) throw new Error(`timeline edit "${x.slice(0, 40)}" must match exactly once in ${ed.step}/${ed.field}`);
    const v = cur.replace(x, () => y);
    if (ed.field === 'lab') out.lab[0] = v; else out[ed.field] = v;
  }
  return out;
}
function apply(id, text) {
  let t = text;
  for (const e of EDITS[id]) {
    if (e.replace) {
      const [a, b] = e.replace;
      if (t.split(a).length !== 2) throw new Error(`native edit "${a.slice(0, 40)}" must match exactly once in ${id}`);
      t = t.replace(a, () => b);
    } else if (e.cut) {
      const i = t.indexOf(e.cut[0]), j = t.indexOf(e.cut[1]);
      if (i < 0 || j < i) throw new Error(`native cut in ${id} not found`);
      t = t.slice(0, i) + t.slice(j);
    }
  }
  return t;
}
module.exports = {EDITS, apply, applyTimeline, ATOM_TIMELINE_EDITS};
