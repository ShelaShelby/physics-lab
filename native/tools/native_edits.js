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
module.exports = {EDITS, apply};
