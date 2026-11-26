#include "changelog.h"
#include "ui/reading.h"

#include <algorithm>
#include <string>

#include "app/app_data.h"
#include "fonts.h"
#include "look.h"
#include "style_draw.h"

#include "imgui.h"

namespace ui {

namespace {

struct Entry { const char* version; const char* title; const char* text; };

// The app's own history. Newest first.
const Entry kNative[] = {
    {"0.88.0", "The big moons outside 1800 to 2100 follow JPL's table, and Pluto's map is read as it was made",
     "Before 1800 and after 2100, the 13 big moons (Phobos, Deimos, the four Galilean moons, Enceladus, Rhea, Titan, Titania, Oberon, Triton and Charon) no longer start at made-up angles "
     "on a plane tilted by the planet's tilt. Their orbits now follow JPL's table of mean elements: JPL's size, shape, tilt, plane and period. The table is rounded, so where "
     "each moon is on its orbit is not the real place there: the card says after about how many years the rounding alone turns the angle by a full turn (Phobos: a few years; "
     "Io: thousands). Four rows of the table (Rhea, Titan, Enceladus, Triton) start from the mean longitude of the Horizons fit, because JPL's printed one is far from Horizons; "
     "REPORT lists them. Our Moon outside 1800 to 2100 is unchanged: a simple ellipse. Pluto's map was decoded as if its bytes were sRGB, but USGS says they are a linear stretch of "
     "the measured values. They are now read linearly: its contrast changes, and its average brightness is 0.40 instead of 0.16 (the ball is still as bright as measured)."},
    {"0.77.0", "Planets as bright as measured, Earth's clouds, and fixes from a physics review",
     "An outside physics professor reviewed the app. Every planet and moon with a map is now as bright as measured: fully lit, its ball reflects its measured share "
     "of sunlight (Mallama and others 2017 for the planets, NASA's fact sheets for the moons). The Learn guide says the limits: a matte ball, no extra brightening "
     "at full phase, and Europa, Rhea, Triton and Neptune drawn darker than measured, because no matte ball can be that bright. Earth has its clouds, NASA's map "
     "of one moment, lit by the Sun under the air. A far-away star behind Saturn is now hidden by the planet and dimmed by its rings, as much as the rings block. "
     "Most realistic turns off the moving gas's clump and patches, which are models, and its notes on bending and stars were corrected. The black hole's guide "
     "names two simplifications of the disc's light."},
    {"0.76.0", "Faster tests, nothing changes in the app",
     "Nothing changes in how the app works. The automatic tests that start the app were cut to the ones that catch real breakage: the app starts, each lab opens "
     "and draws, the main controls respond, cards, pages and search work, settings are kept, and every picture the graphics card makes is still compared with the "
     "processor's. Checks of old fixed bugs, exact pixel layouts and repeats at every size and theme were removed, so a full test run takes minutes, not hours."},
    {"0.75.3", "Tidier code and tests that run on GitHub",
     "Nothing changes in how the app works (a few benchmark descriptions no longer name OpenGL, which the app stopped using in 0.69.0). The project's rules were cut down to one short file, old notes about the graphics system the app used before Vulkan were taken out "
     "of the code, and the whole test suite, including the tests that start the app and look at its pictures, now runs on GitHub after every update."},
    {"0.75.2", "Orbit Lab corrections from an outside review, and rings at their own angle for every giant",
     "An outside reviewer checked the Orbit Lab's numbers against published sources; what held up is fixed. Planet cards: the spin's outward push is now worked out as "
     "omega squared times the radius (Jupiter takes off 0.23 g, not 0.17) and the pull alone is what you feel plus that push; \"x Earth\" sizes compare like with like "
     "(planets with Earth's equatorial radius, dwarf planets with its mean radius) and the dwarf planets' gravity is worked out properly in standard g. Eris is 2,326 km "
     "wide (its 2010 occultation, Sicardy et al. 2011), a little smaller than Pluto. In Simple, running time and jumping now agree: every planet and dwarf planet is put where "
     "JPL's table says at each step, so a run to a date gives the places of a jump to it (before, running time drifted away from the table: Jupiter by 1.4 degrees a "
     "century). The cards' orbit numbers now use the Sun's pull plus the planet's own, as the real velocities do (Jupiter's orbit was 0.005 AU too big, its year 8 days too "
     "long). The Moon's lit share is the exact one from the angle at the Moon. Time that runs past 2100 (or back before 1800) now says that the lab no longer checks it. "
     "The barycentre card gives the Sun's swing from Jupiter (1.07 Sun radii) and Saturn (0.59). Text fixes: the spin axes are marked + (the right-hand-rule end: for "
     "Venus and Uranus that is the IAU's south pole); Saturn's, Uranus's and Neptune's cards name the newer rotation periods beside the IAU's; one asteroid count (about 1.2 "
     "million, give or take 0.5 million) in the belt card and the guide; the dust tail is not a straight tail; Haumea is a squashed rugby ball; the Moon's distance is its "
     "semi-major axis; 1 AU is a defined length; Real's note says which dates are UT; the guide no longer says our Moon is not real in Simple; eclipse times are given "
     "with the real errors of the fitted moons and the light's travel time. Jupiter's, Uranus's and Neptune's rings now take each part's own viewing angle, as Saturn's did "
     "in 0.75.1. Not done (see the roadmap): the Sun's darker edge (the paper is not saved), the moons outside 1800-2100 on JPL's mean elements, photometric functions for "
     "Mercury and Pluto."},
    {"0.75.1", "Saturn's rings, each part at its own angle",
     "Close up, the near and far parts of Saturn's rings are seen at very different angles, and now each part is drawn as it would look from there: how much light "
     "it stops and how brightly it shines are worked out for every pixel from that pixel's own viewing angle (the camera is at one height above the ring plane, so the "
     "angle is height / distance), and the face you see is the camera's side of the plane. The graphics card does it per pixel; the processor does the same "
     "at the corners of its triangles. It is still the single-scattering light of a thin layer (no light scattered twice). The Orbit Lab can also say, in the "
     "benchmark's rest line, why its picture was drawn again and again at rest; and the benchmark's card times no longer count frames it left out."},
    {"0.75.0", "The Orbit Lab's 3D view has real depth",
     "Zooming in 3D now flies the camera closer: it is a pinhole camera with a 45-degree field, like a phone camera, placed so that the zoom keeps its meaning at the "
     "followed thing's distance (the scale bar says \"at Earth's distance\"). Near things loom: a moon between you and its planet is drawn bigger, a ball's outline is "
     "focal x tan(asin(R / L)), lines that pass the camera are cut where they reach it, and nothing behind the camera is drawn. The stars, the constellations and the "
     "Milky Way are drawn through the same lens, so a star and a far planet in the same direction stay together while the view turns. At True scale the closest zoom "
     "keeps the camera 1.5 radii from a followed planet's middle. A click on a body flies to it even when nothing is followed, and ticking Follow glides there "
     "(Reduce motion: at once). The card's lines, rocks and Saturn's rings use the same camera, checked against the processor at start-up; a new benchmark scene, "
     "orbit.flyby, flies past Saturn's rings from the closest zoom. 2D stays the top-down map."},
    {"0.74.0", "The Orbit Lab's camera keeps what it follows, and moves smoothly",
     "Following a planet, a click on empty space or a right drag no longer lets go of it (time used to carry it off the picture in a second). A right drag, Shift-drag or a drag "
     "in 2D slides the view: the followed body stays followed where you put it. Only picking something else, Reset view (R or Home), Esc or picking the followed body again let go. "
     "A pick glides there in about 0.8 s (a moving planet is reached where it is), the wheel and the + and - keys ease the zoom, a released turn coasts a little and stops, "
     "and the 3D view tilts to 89.5 degrees. With Reduce motion on, everything moves at once, as before. About: the developer's paragraph ends with Free Palestine and a small drawn flag."},
    {"0.73.1", "A clicked moon is followed",
     "Clicking a moon (or picking it in Controls, Pick a body) kept its planet in the middle of the picture; now the moon stays in the middle, where it is drawn (the wider orbit in Easy to see, the real distance at True scale), "
     "in 2D and 3D, for Earth's Moon and every other moon. \"Slow time to watch moons\" with a moon selected selects and follows its planet, so the moons go round it."},
    {"0.73.0", "The Alpha Centauri system in the Orbit Lab",
     "A new switch at the top of the Orbit Lab's Controls: Solar System or Alpha Centauri. A and B circle each other on their measured 80-year orbit (Akeson et al. 2021) "
     "with the lab's date and time; Proxima sits at its measured place 12,000 au out on its half-million-year orbit; its planets b and d circle it with their measured "
     "periods and phases, drawn in an assumed tilt (the star's equator), their masses shown as minimums and their sizes unknown. Proxima c (disputed) and the JWST "
     "candidate around A (unconfirmed, seen once) are drawn dashed. Each star has its habitable zone, its card and its sources; Fit buttons frame the pair, the whole "
     "system or Proxima's planets. The sky is the one seen from there, with our Sun as a bright star in Cassiopeia. A new guide tells the system's story (refuted "
     "planets included), the Data panel has its tables, and the masses and distance of A and B everywhere are now Akeson et al. 2021's. Found on the way: the "
     "Orbit Lab's lines on the graphics card lost part of the view's position (up to half a point zoomed fully in on a planet): fixed."},
    {"0.72.1", "Research: the Alpha Centauri system; no change to the app",
     "Research: the Alpha Centauri system (PLAN_ALPHACEN.md); no change to what the app does. Everything known about A, B, Proxima and every planet ever "
     "claimed around them, from 57 saved papers: the stars' orbit, masses, sizes and ages, Proxima's half-million-year orbit, Proxima b and d (confirmed), "
     "Proxima c (disputed), the JWST candidate around A (unconfirmed), and what nobody knows yet. The app's masses and distance of A and B come from a 2016 "
     "orbit; a 2021 orbit measured with ALMA gives 5 % smaller masses and 1 % less distance (to change in the first build step)."},
    {"0.72.0", "The Black Hole Lab: the gas moves",
     "With the disc on, the gas now moves, in the Real and Film looks: every ring turns at its true speed, a bright clump circles the hole and bright and dark "
     "patches stretch into trailing spirals. Every pixel shows the gas as it was when its light left, worked out exactly from the light's path: the second image "
     "of the clump under the hole is older than the first. Two clocks on the picture, far away and on the clump (and yours when it differs), and in the Gas disc "
     "section Moving gas, Pause and the rate (from a millionth of real time to ten million times faster; it starts so that the innermost gas turns once in 20 s). "
     "The speeds, clocks, bending, shifts, travel time and shearing are real; the clump and the patches are labelled models (Broderick and Loeb 2006, Schnittman, "
     "Krolik and Hawley 2006), explained under More settings and in the guide's new page Moving gas. Drawn on the graphics card, checked against the processor "
     "when the lab first draws. A new benchmark scene, blackhole.moving_gas."},
    {"0.71.0", "The Black Hole Lab's Controls panel: one short panel, the rest under More settings",
     "The Black Hole Lab's Controls panel now fits a 1280 x 800 window without scrolling (it was about four screens long). It has three short sections: Black hole "
     "(the presets with their measured mass, distance and source, the look Real or Film, the spin and the guide's button), Camera (distance, field of view, "
     "Reset view) and Gas disc (on or off, the feeding rate). Everything else is in one section, More settings, which starts closed and remembers whether you "
     "opened it: the mass, the typed values, the exposure, the outer edge, Gravity bends light, Sky, Light, Sky beside the disc, Glare, the film's choices and "
     "The paper's view, each with its full explanation, word for word as before, grouped by what you want to do. No drawing and no physics changed."},
    {"0.70.0", "The Black Hole Lab's Real look: no white-outs, a camera-like picture, a better starting view",
     "Real now draws the disc as a camera would: automatic exposure puts the disc's average brightness (its log-average) at middle grey and its brightest part "
     "at white, through a camera's response curve that rolls off the highlights (Reinhard, Stark, Shirley and Ferwerda 2002, listed under Where the facts come "
     "from), so the bright parts keep their shading and the side coming towards you stays brighter. The curve acts on brightness only: each pixel keeps the "
     "colour the physics gives it. It is worked out on the graphics card, checked against the processor's exact version. Your choice of exposure is now "
     "brighter or darker than automatic in stops, and goes back to 0 when the preset, the look or the disc changes: until now it was a fixed brightness kept "
     "across presets, which could turn the whole picture white with the eye's glare (an old settings file's exposure is read as 0 stops). The lab opens at "
     "50 GM/c², 45 degrees across, 80 degrees from the disc's axis: the shadow about a quarter of the picture, the disc below it and its image over the top; the "
     "feeding preset opens there too. The grey ring next to the shadow with Sky beside the disc: Shown is the sky seen between the shadow and the disc's images. "
     "The Film look is unchanged."},
    {"0.69.0", "OpenGL removed: Vulkan is the app's only drawing system",
     "Everything is drawn with Vulkan: the OpenGL path, its card programs and the Graphics choice in Settings are gone (an old settings file's graphics= line "
     "is read and ignored); nothing else you see changes. Vulkan 1.3 and 1.4 are used as before, and now also Vulkan 1.2 with the extensions "
     "VK_KHR_dynamic_rendering and VK_KHR_synchronization2 (the Radeon Pro cards of Apple's Boot Camp drivers, Polaris, Vega and Navi, stop at 1.2 with both: "
     "vulkan.gpuinfo.org). A PC whose Vulkan cannot start, or whose Vulkan stopped the app last time, gets one window naming what is needed and what the PC has, "
     "with Try again. The lab's picture reaches the window by a copy (a blit) instead of an extra shader pass; the pictures are the same. The checks that compared "
     "the card's programs and pictures with the processor's now run on Vulkan (vk_compare_tests, the Black Hole Lab's precision checks)."},
    {"0.68.1", "Fixes: the helper's time limit, the nucleus density benchmark scene, the benchmark's own work out of the counted frames",
     "A graphics-card helper that never answers is now ended after 30 seconds (and every helper when the app closes), the log says which step hung, and the next one "
     "starts; a helper that ends without writing its result is logged as that, and a crash code is no longer taken for \"still running\". The light paths' variants are "
     "tried until all five have a line. The benchmark's nucleus density scene zooms by the nucleon's size, so it draws the Density look in a window as big as 2560 x 1369 "
     "(it drew the nucleons there before); the card path's log names its reason. The benchmark's own picture comparison is left out of the counted frames and says so."},
    {"0.68.0", "Speed: the Orbit Lab's trails and orbits and the Molecule Lab's Density surface on the graphics card; the crash-safe helper for every card program",
     "On Vulkan the Orbit Lab's trails, the planets' and dwarf planets' orbits and the distance rings are drawn by the graphics card: their points stay on "
     "the card (a trail only sends the points it gained, an orbit only when it changed) and the card turns, bends into curves near the picture, clips and "
     "widens them exactly as the processor did (checked against the processor's lines when it starts, at the heavy view, the closest zoom and a 3D close-up). "
     "The Molecule Lab's Density surface is turned, coloured by the potential, sorted far to near and drawn by the card; a vibration sends each new phase's "
     "surface once (checked against the processor's at benzene, water and a vibrating water). The pictures are the processor's within a few pixels. The "
     "helper process (the same program, no window) now makes the first pipelines and runs the first small run of every card program the app guards, not only "
     "three: if the driver crashes in one, only that program goes back to the processor. The comets' dotted orbits, the Sun's path in Real physics and the "
     "moons' orbits are still drawn by the processor, and the Black Hole Lab's stars still come from the processor (they need its tables)."},
    {"0.67.0", "Speed: the Orbit Lab quiet at rest and without the far-date freeze; the nucleus' Density look on the graphics card; a crash-safe helper for the card's first steps",
     "The Orbit Lab keeps its picture while nothing in it changes (time stopped, nothing moving): it is no longer made again every frame, and anything that "
     "changes it (a click, a setting, the window) draws it anew. A jump to a far date in Real physics (1850 from today) no longer freezes the window for "
     "about two seconds: the planets' path there is worked out on a worker while the lab stays at the date shown and the note says \"Going to <date> ...\"; "
     "the planets land where they did before, to the bit. On Vulkan the nucleus' Density look is worked out and drawn on the graphics card (checked against "
     "the processor's sums when it starts: within 1e-4 of the brightest cell), and only when the view changes. The Atom Lab's nucleon close-up keeps each "
     "quark dot's place for its life instead of working it out every frame (the same picture, about a third of the work). The Black Hole Lab's panel works "
     "out a spinning hole's shadow size on a worker while you drag. The benchmark now counts the new pictures a second over the time the view moved (a "
     "picture at rest needs no new one). New: a helper process (the same program, no window) makes the card programs' risky first pipelines (the light "
     "paths, the Classic dots, the nucleus' Density look) before the app does; if the graphics driver crashes there, only the helper ends, the app runs on "
     "and that program stays off (Settings, Performance: Try again). While the light paths are off, helpers try five cut-down versions of their shader once "
     "per driver and the log says which the driver can make."},
    {"0.66.0", "Speed: the Slice of a Dirac state without stalls; the nucleus close up draws a quarter of the pixels; less processor work in five heavy views",
     "The Slice's new dots now get a time budget a frame (1.5 ms): the steps that make them only owe them, and they are made in order after the steps, until "
     "the budget is spent. The same dots in the same order, spread over more frames: while the cut is dragged the cloud fills a little later, never coarser, and "
     "no frame waits for the Dirac state's sampler any more (one rule for every Slice). The Dirac sampler's search stops where its interval no longer changes "
     "(the same numbers). In the nucleus views each nucleon's soft glows (its rest profile and the Gaussians of its quarks) are drawn in one quad instead of a "
     "quad each, and glows whose light on the picture is under half a level are left out: at full zoom the card fills about a quarter of the pixels it did, "
     "with the same picture. The Density surface's triangles are sorted by a radix sort (equal depths now always in the same order), the nucleus' Density "
     "look shares its line-of-sight sums among four threads, and the Orbit Lab's whole orbits take their sines from a table (the same numbers)."},
    {"0.65.0", "Speed: the every-electron views and the Slice's dots on the graphics card; the Slice of a Dirac state four times faster",
     "The side view with every electron (Glow and Classic) and the Slice's dots (every Slice, Glow and Classic) are now put on the picture on the graphics "
     "card (Vulkan), from the cloud's buffer, by the Classic dots' program; the processor's way stays the reference and the fallback, and the program's "
     "start-up check now compares all of them with it. The every-electron clouds' colour groups are kept between frames: only the new dots' are made and "
     "sent, as their places are. The Slice of a Dirac state makes the same dots as before, bit for bit, about four times faster (its state and cut looked up "
     "once a batch, the radius table searched once an attempt, four attempts worked out side by side); the Slice with every electron lost its per-dot "
     "table searches too. Fixed: after a Slice of every electron or of a Dirac state the Slice's card program held the guards' turn for ever, so the card "
     "programs started after it (the Classic dots in the benchmark) never started. The card-vs-processor comparison test runs again and is in the test suite."},
    {"0.64.0", "Speed: the Slice of a superposition, Saturn's rings and the Classic dots on the graphics card; six heavy views in the benchmark",
     "The Slice of a superposition (two states of different energy) now makes its dots on the graphics card, with the plain Slice's program: the card makes "
     "both states' attempts and the processor picks the state from the same random number, so the dots are the same up to single-precision rounding (the "
     "start-up check has the superposition's states). The processor's own path, the reference, also lost its waste: the planes' parts are worked out once a "
     "batch, not once an attempt (the same dots, bit for bit). Saturn's rings are drawn per pixel on the graphics card (Vulkan): the same 128 chords, bands "
     "of Cassini's profile and the planet's shadow as the processor's triangles, which stay the reference and OpenGL's way; the processor's triangles lost "
     "their waste too (bins kept while the view's angles stay the same, runs of steps drawn as one quad, nothing made beyond the picture). The every-electron "
     "Classic Cloud 3D is put on the picture, banded and ordered on the graphics card (Vulkan) from the cloud's buffer. Each new program has its own guard, "
     "its log lines and a start-up check against the processor. The benchmark's Hard list has six new scenes, the heavy views it missed: the Slice with "
     "every electron, the Slice of a Dirac state, the side view with every electron (Glow and Classic) and the Slice at Ultra (Glow and Classic); the run "
     "takes about 4 minutes 45 seconds."},
    {"0.63.1", "Vulkan fixes from the first Vulkan benchmark: the outline back on, the GPU timer, the worst frames",
     "Fixed: the Atom Lab's 90 % outline (and on some PCs the superposition groups and the Cloud 3D Glow dots) stayed switched off on Vulkan after 0.62.0: "
     "0.62.0 left a start-up marker on disk after a step that had passed (checked here: its outline marker stayed after a normal close), and the next start "
     "read it as a crash. A marker now names the app version that wrote it; a crash kept from 0.63.0 or older is tried once more, guarded (the light paths "
     "stay off: their crash was real). Fixed: on Vulkan the benchmark's GPU time per frame was nonsense on some cards (about 10^12 ms): a frame's timestamps "
     "are now read only after its fence has signalled, and a zero or backwards pair is thrown away and counted. The benchmark's CPU work no longer includes "
     "waiting for the graphics card (a line of its own), each scene lists its three worst frames with where their time went (the frame's work, waiting for "
     "the card, the present, a pipeline made, a batch read back and what it was, a guarded step deferred), and the header gives the benchmark's own start "
     "part by part; the box says \"Preparing the benchmark ...\" meanwhile. The GPU timer's check no longer copies each test picture to the processor (most "
     "of the start's time on a fast card). blackhole.film_tilt no longer compares its two pictures again in every frame after the one at rest, and the "
     "frame after a picture read back for the benchmark's own check is no longer counted. The report says when a card program's guard has it off, and the "
     "texts and Settings rows that said \"OpenGL 4.3\" name Vulkan too."},
    {"0.63.0", "Vulkan, step 5 of 5: the app starts on Vulkan; OpenGL the backup; the benchmark on Vulkan",
     "The app now draws with Vulkan by itself. OpenGL stays as the backup (Settings, Performance, Graphics: OpenGL (backup)) for an older graphics card or a "
     "driver problem, and a card without Vulkan 1.3 still starts on OpenGL as before, with Settings saying why. A settings file that chose OpenGL keeps it. "
     "The benchmark now runs on Vulkan too, with the graphics card's time per scene from Vulkan's timestamps (checked against a clock with a known load, as on "
     "OpenGL), and its report says which graphics system ran and the state of each card program's start-up guard. New Hard scene atom.slice_super_drag: the "
     "Slice of the superposition 2p + 5f of mendelevium-258, Relativity On, phase colours Wheel, the cut dragged. Fixed: the start-up guards blamed the wrong "
     "program after a driver crash (on one PC the light paths crashed and the outline was switched off too): now only one guarded step is open at a time, and "
     "a step's marker goes when it ends. Inside, the Vulkan drawing of the first steps now uses Vulkan 1.3's dynamic rendering and "
     "synchronization2, as the Black Hole Lab's does (the pictures are unchanged)."},
    {"0.62.0", "Vulkan, step 4 of 5: the Black Hole Lab on Vulkan, as a test; Vulkan 1.3 needed",
     "With Settings, Performance, Graphics: Vulkan (test), the Black Hole Lab is now drawn on Vulkan too, with the same shader text as on OpenGL: the bent sky "
     "and the disc, the spinning hole worked out for every pixel, the automatic exposure, the lower resolution while moving on a slow card, the Film look's "
     "glow, the eye's glare (its veil made on a thread of its own, the frame never waits) and the graphics card's light paths. The pictures are the same as "
     "on OpenGL (checked view by view). The large programs are made on a thread of their own, never in a frame, and kept in a pipeline cache in the settings "
     "folder, so later starts are faster. Each has its own start-up check on Vulkan: the light paths, switched off on OpenGL on some PCs, get their own try "
     "on Vulkan, and a failure on one graphics system never switches the other off. Vulkan now needs version 1.3 (1.4 is used where the card has it): with "
     "an older driver the app starts on OpenGL and Settings says why. The GPU switches in Settings name the graphics system in use. OpenGL stays the default "
     "and is unchanged."},
    {"0.61.0", "Vulkan, step 3 of 5: the graphics card's own programs on Vulkan, as a test",
     "With Settings, Performance, Graphics: Vulkan (test), the work OpenGL gives the graphics card's own programs is now done by the card on Vulkan too: the "
     "belts' rocks on their orbits, the Atom Lab's Cloud 3D Glow dots, the superpositions and the jumps' colours, the 90 % outline, the Slice's dots, a jump "
     "with every electron, and the Molecule Lab's Density cloud. They run the same shader text as on OpenGL and give the same pictures (checked against the "
     "processor's own results and against OpenGL's). Each has its own start-up check: one that fails or stops the app is switched off on Vulkan alone and "
     "the processor does its work, while OpenGL keeps its own. The belts' rocks need double precision on the card; a card without it places them on the "
     "processor and Settings says so. Only the Black Hole Lab is left (step 4). OpenGL stays the default and is unchanged."},
    {"0.60.0", "Vulkan, step 2 of 5: the Orbit, Atom and Molecule Labs drawn on Vulkan, as a test",
     "With Settings, Performance, Graphics: Vulkan (test), the Orbit, Atom and Molecule Labs now draw their pictures on Vulkan: the planets with their maps, "
     "rings, eclipses and Earth's air, the night sky, the belts, the electron clouds, the nucleus and its quarks, the chart of nuclides and the molecules, "
     "with smooth edges (4x multisampling) as on OpenGL, Monochrome too, and Save image keeps the lab's picture. They are drawn with the same shader text "
     "as on OpenGL and look the same. What OpenGL does with the graphics card's own programs (the belts' rocks, the Atom Lab's cloud dots, superpositions, "
     "jumps and outlines, the Molecule Lab's Density cloud) is done by the processor on Vulkan for now, so some scenes are slower until step 3; the Black "
     "Hole Lab comes in step 4 (its picture area says so). A new check at Vulkan's start draws a tiny known picture and checks its colours: if they are "
     "wrong, the app starts on OpenGL and Settings says why. OpenGL stays the default and is unchanged."},
    {"0.59.0", "Vulkan, step 1 of 5: the menus and panels on Vulkan, as a test",
     "The app is moving from OpenGL to Vulkan, a newer way of talking to the graphics card, in five steps. This first one lays the foundation: Settings, "
     "Performance has a new row, Graphics: OpenGL / Vulkan (test), used from the next start. On Vulkan the menus, panels, Settings, pages and guides are drawn "
     "by Vulkan, the same as on OpenGL; the labs' pictures are not drawn yet (the picture area says so), they come in the next steps. OpenGL stays the default "
     "and is unchanged. Each step of Vulkan's start is guarded: if one fails, or stops the app, the next start uses OpenGL and the row says why, with Try "
     "again. The benchmark runs on OpenGL only until step 5. About credits the two Vulkan libraries now built into the app (volk, the Khronos Vulkan headers)."},
    {"0.58.0", "The Black Hole Lab's disc reaches as far as real ones, and a new Light choice",
     "The disc used to stop at 30 times the hole's own length, with a hard pale rim. That was a cut, not physics: discs round massive black holes are believed "
     "to reach about a thousand Schwarzschild radii, where their own gravity breaks them up (Goodman 2003). The disc now goes out to 2000 GM/c\xC2\xB2 with the "
     "same physics all the way, and fades by itself as it cools outward; nothing else in the picture changed. A new Real-look choice, Light: What an eye sees "
     "(the picture as before) or All light, where each point's brightness is its light of all wavelengths together, most of it ultraviolet for this disc, "
     "with the colour the eye would see. In All light the hot inner ring and the side coming towards you stand out strongly, as in Luminet's 1979 picture: "
     "the total light changes with the 4th power of the shift. It is drawn on the graphics card, whose start-up check now also compares the disc's light in "
     "both modes with the processor's. The spin card no longer says that nothing can stand still anywhere near a spinning hole: that is true inside the "
     "ergosphere, and the lab's camera turns with the dragged space wherever it is."},
    {"0.57.2", "The spinning hole is quick again",
     "Since 0.56.4 every light ray of the spinning hole also worked out where and when it crossed the disc's plane, for the moving gas still to come. Nothing "
     "used those numbers yet, and they made each ray about three times as slow, so the tables behind the picture took five times as long and dragging the "
     "view stuttered. They are now worked out only where they are asked for, and the tables are as fast as before. The card's start-up check now does its "
     "processor part on a thread of its own instead of holding up a picture, and the panel no longer works out the same shadow numbers several times a "
     "picture. What you see is the same."},
    {"0.57.1", "The Molecule Lab's Density cloud moves to the graphics card",
     "The Density style with its 100,000 dots was among the slowest scenes of the benchmark, the vibrating water most of all. The cloud is now made and drawn "
     "by the graphics card: the dots stay on the card, and each picture a small program there places, sizes and lights every dot as the processor did. The "
     "processor's way stays as the exact reference: the card's program is checked against it when first used and switched off safely if the card or its driver "
     "fails (Settings, Performance says which ran and why). Work the processor did again and again for nothing was also taken out: while a vibration played, "
     "the still surface and a moment's whole cloud were made again in every picture. What you see is the same."},
    {"0.57.0", "The Atom Lab's two slowest pictures move to the graphics card",
     "Dragging the cut in the Slice view and a jump with every electron were the slowest scenes of the benchmark (about 12 and 20 pictures a second on a fast "
     "PC). The Slice's new dots on a moved cut are now made by the graphics card from the same random numbers as before, and a jump with every electron is "
     "drawn from the cloud's buffer on the card, which also decides each picture which dots are shown. The processor's way stays as the exact reference: each "
     "card program is checked against it when first used and switched off safely if the card or its driver fails (Settings, Performance says which ran and "
     "why). Work the processor did again and again for nothing was also taken out. What you see is the same; in the Slice view the new dots arrive one "
     "picture after the cut moved, as the outline already did."},
    {"0.56.4", "Moving gas, step 1: the physics behind the scenes; no change on screen",
     "The Black Hole Lab now knows, for every point where a light ray meets the disc, where on the disc that point is and how long the light took from there "
     "to the camera, worked out exactly from formulas instead of step by step. Also new inside: the clocks of the gas and of the camera, the bright clump "
     "astronomers use to model flares, and the moving bright and dark patches of a simulated disc, each checked against published numbers. Nothing is drawn with "
     "them yet: that is the next version."},
    {"0.56.3", "Research for moving gas in the Black Hole Lab; no change to the app",
     "A plan for gas that moves at its true speed round the hole, a bright clump circling it, a time control with two clocks, and structure in the disc, with "
     "every number from saved papers (the GRAVITY flares near Sagittarius A*, ALMA's loops, the classic hot-spot models, a simulated magnetised disc). The time "
     "light takes along each path was added to the processor's reference and checked against published values. The app itself does not change yet. The "
     "version number in the program, which still said 0.56.1 in 0.56.2, is right again."},
    {"0.56.2", "The Black Hole Lab stays sharp while you move it; a row of wrong pixels beside the shadow is gone",
     "The spinning hole is now drawn at every pixel while you drag, too. Only on a graphics card that cannot draw 60 sharp pictures a second does it drop to a "
     "lower resolution while the view moves; Settings, Performance then says so and has the switch Lower resolution while moving (Auto or Off). The row of dark "
     "specks (or a grey streak) to the right of the shadow, near the disc's plane, is gone: on one line of the sky, where the light's angular momentum is twice "
     "the horizon's radius over the spin, two terms of a formula cancelled in the card's single precision and some rays were lost. The formula is now written "
     "without that cancellation, on the card and in the processor's version; in the views tested the card's far directions are now about 50 times closer to the "
     "double-precision reference. The card's pass is about a fifth faster (integrals that were not used are no longer made). Switching to the Black Hole Lab after "
     "the eye's glare had been on no longer waits 15 seconds in the benchmark: a glare half made is let go when the glare is switched off."},
    {"0.56.1", "The benchmark tests only what is hard, and says plainly what is slow",
     "The Benchmark now runs, by default, only scenes that are hard to draw, each moving as you would move it: Saturn and Earth close up (Earth on the 12 August "
     "2026 eclipse, with the Moon's shadow), Real planet physics at the fastest time, the turning night sky, the Journey, every-electron clouds that fill the "
     "picture, a jump with every electron, the nucleus in its Density look, the Molecule Lab's Density style and vibrations, the Black Hole Lab's Film look tilted "
     "through the disc and its top spin at the closest distance, a long guide scrolled, the Data panel and Settings over the heaviest Orbit view. The easy scenes "
     "and the old processor paths are under All scenes. Every scene has a short id that stays the same in every version. The report starts with one table, slowest "
     "first: the frames a second, the 1 % low, the new pictures a second, whether it reaches your screen's rate, and whether the processor or the graphics card "
     "holds it back. Then how long you wait for a finished picture (switching labs, a new element or density, another molecule, a new spin, Film and back, a far "
     "date), and whether each lab is quiet when left alone. The head names the processor, the memory, the app's peak memory, the start-up time and the worst "
     "frame. About four minutes. The Controls panel now says the true number of dots under Cloud density: with every electron it is 48,000 per electron at Ultra, "
     "at most 240,000."},
    {"0.56.0", "The Black Hole Lab: the graphics card draws the spinning hole's sky and disc for every pixel",
     "Round the spinning hole the graphics card now works out each pixel's light path itself from the closed formulas (Gralla and Lupsasca 2020) in "
     "single precision: the bent sky and the disc are drawn anew in every frame while you turn, tilt or change the distance, with no table in between, so "
     "the moving picture and the picture at rest are the same picture. When the card is slow, the picture is drawn at a lower resolution while you move "
     "and always at full resolution at rest. The picture's exposure is found on the card in the same frame (at spin 0 too). Before it is used, a check "
     "at the first spinning picture compares the card's rays with the processor's double precision; it has its own guard, its own steps in the log, and "
     "Settings, Performance says which path draws the spinning hole and why. The stars still come from the processor and may trail the sky by a few "
     "frames while you move; the disc and the shadow hide them. At spin 0 the disc's table is made three times faster while the distance changes."},
    {"0.55.9", "The Black Hole Lab: the spinning hole's light paths from closed formulas",
     "Round the spinning hole every light path is now worked out from its closed formula (elliptic integrals and Jacobi functions, from Gralla and Lupsasca "
     "2020) instead of being followed step by step: the sharp picture after you stop comes about ten times sooner, and the picture made while you tilt or "
     "change the distance about three times sooner while it has more than twice the rays. The step-by-step integrator stays as the reference the tests check every "
     "formula against, in every case the paper distinguishes. Seen nearly edge on, the picture made while you dragged showed the disc's thin near side "
     "wrongly, so the disc changed shape when you stopped: that picture now has more rows, also where the disc's image changes fast. If you stand exactly in the "
     "disc's plane, the card says that a disc with no thickness is then a line. And the graphics card's light paths, once switched off on a PC because "
     "they stopped the app, are no longer tried again just because the app was updated: only when their own shader or the driver changes, or you press Try again."},
    {"0.55.8", "The Black Hole Lab: the spinning hole's light paths about 3.5 times faster on the processor",
     "Round the spinning hole the processor now follows each light path in a second, equivalent form of the same equations (the time usually called Mino time: the motion "
     "away from the hole and the motion round its axis separate, and both become plain polynomials without sines), and follows each escaping path all the way out "
     "to infinity instead of stopping far out. The picture made while you tilt or change the distance is ready in about a quarter of the time, so it follows "
     "the view nearly every frame; the sharp picture at rest also comes about 3.5 times sooner. Its numbers are tested against the earlier form at its highest "
     "precision; the paths that pass close to the spin axis keep the earlier form. The benchmark's Black Hole scene now counts its last movement too, and reports "
     "the fine table's time and how long after a stop the sharp picture came."},
    {"0.55.7", "The Black Hole Lab: the graphics card's light paths can no longer stop the app",
     "On one PC the app closed by itself a few seconds after it opened, every time, when its saved settings opened the Black Hole Lab with a spinning hole. "
     "The graphics card's light paths now start only after the window is on screen, a step at a time without the window ever waiting, and until they have worked once "
     "with your graphics driver the app writes down each step before it begins. If the app stops inside one of them, the next start leaves them switched off for that "
     "driver (a new driver or a new version of the app tries once more), the processor draws the picture as before, and Settings, Performance says so, names the step "
     "and has a button Try again. A step that does not finish within 10 seconds counts the same way. The steps, the driver's messages and their times go into a "
     "log file beside the settings file (Settings, Performance opens its folder); the benchmark report ends with its last 40 lines."},
    {"0.55.6", "The Black Hole Lab: the graphics card works out the light paths",
     "Round the spinning hole the graphics card now follows the light paths itself, with the same equations and steps as the processor and in double "
     "precision, so the picture is the fine one even while the view moves: no coarse picture first, and no seconds of processor work after you stop. The card "
     "is checked against the processor when the lab first draws a spinning hole; if it differs, or the PC has no OpenGL 4.3, the processor does it as before, "
     "and Settings, Performance says which and why (the switch GPU light paths compares the two). The stars' images are found on all the processor's cores, "
     "for the spinning and the non-spinning hole, each image where it was and as bright. The benchmark's Black Hole scene also moves at spin 0 and reports "
     "the card's time and how many new pictures came a second."},
    {"0.55.5", "The Black Hole Lab: the window no longer waits for the picture",
     "The Black Hole Lab no longer makes the window wait while it works out a picture. Turning, tilting, changing the distance or dragging the spin now "
     "moves the controls and readouts at once; the light paths, the stars' images and the exposure are worked out on a worker thread below normal priority, "
     "and until a newer picture is finished the last finished one stays on screen, all of one view. While the picture is behind, the card says it is being "
     "worked out. The eye's glare and the fine table are made the same way once the view rests. The Film look's exposure no longer works out the film's "
     "colour anew at every point (it took seconds each time the view moved). The pictures at rest are the same as before. The benchmark's Black Hole scene "
     "now turns, tilts and changes distance in turn, and its report gives the time of each stage and how long the picture was behind."},
    {"0.55.4", "The Black Hole Lab: real light (true brightness, the eye's glare)",
     "In the Real look with the gas disc on, two new controls. Sky beside the disc: Shown is the picture as before; True brightness puts the stars on the disc's "
     "own scale, from each star's measured brightness against the Sun's (Willmer 2018), and the sky goes black: next to such a disc every star is billions of "
     "times too faint to show. The card says by how much the brightest star is fainter and at which exposure it would first appear; turn the exposure down far "
     "enough and the stars come back while the disc burns out. Glare: As an eye sees it lays the veil of light a very bright source makes inside an average eye "
     "(the CIE's disability glare equation, 35 years, brown eyes, 0.1° to 100°); it moves light, it makes none. Most realistic picks both. In the Film look the "
     "glow now comes from the disc's light only, so the Milky Way beside it is no longer washed out."},
    {"0.55.3", "The Black Hole Lab: the Real and Film looks",
     "In the Black Hole Lab's Controls, Look offers Real and Film. Real, with the closest-to-reality dot, is the picture as before: your spin and the standard "
     "thin disc with the colour and brightness changes of the gas's speed and gravity, real for quasars and small black holes feeding from a star. Film shows "
     "the choices the makers of the film Interstellar made for its black hole, as James, von Tunzelmann, Franklin and Thorne (2015) describe them, each one "
     "labelled: the spin the film drew (0.6, lower than its story needs, so the picture would not confuse), a thin disc at one temperature, 4,500 K all over, with "
     "the edges of the paper's disc figure, no colour or brightness change from the gas's motion, the paper's white balance, and a soft glow. The film's lens "
     "flare was never published, so the glow is the app's own and says so. While Film is chosen the spin and the disc are the film's and locked; your own "
     "settings come back with Real. \"The paper's view\" puts the camera where the paper's disc figure has it. This copies the paper's stated choices, not the "
     "film's pictures: its textures, lens flare and colour grading are not published and are not in the app. The paper gives no mass for the film's black hole, "
     "so there is no preset for it. Most realistic picks Real; the look is kept between runs. The guide has a new part, What the film changed."},
    {"0.55.2", "The Black Hole Lab: a spinning black hole",
     "In the Black Hole Lab's Controls, Spin gives the hole angular momentum, from 0 to 0.99 of the most a black hole can have (a slider and a typed value). "
     "The shadow loses its round shape: seen from the plane of the spin it is flattened on the side that moves towards you and shifted to the other side; seen "
     "along the axis it stays round. The sky near it is dragged round, and the gas disc reaches further in, gets hotter there and turns more of the infalling "
     "mass into light. The card shows the spin, the horizon's radius at that spin, the innermost stable orbit, the share of the infalling mass turned into "
     "light, and that close to a spinning hole nothing can stand still: space is dragged round and you with it, once every so many hours or days. The real "
     "holes' spin is not measured: the presets all start at 0 and say what the telescope papers do say; the control is yours. A coarse picture appears at once "
     "while you change the spin, the distance or the latitude, the fine one a moment after you stop; going round the axis needs no wait. The top, 0.99, is "
     "where the lab's tests stop. The light paths are followed step by step on the processor (James et al. 2015), the shadow's edge comes from their closed "
     "test, and everything is checked against Gralla and Lupsasca 2020, Perlick and Tsupko 2022, Bardeen, Press and Teukolsky 1972 and Cunningham 1975. "
     "The guide has a new part on spin."},
    {"0.55.1", "The Black Hole Lab: a glowing disc of gas",
     "In the Black Hole Lab's Controls, Gas disc (model) switches on the standard thin disc of gas round the hole: gas circling it, heated by its own friction "
     "as it spirals in, shining as a black body from both faces. It is drawn as you would see it: the disc's far side bent up over the hole and its underside "
     "under it, thin rings closer to the shadow, the side coming towards you brighter, in its true colour (blue-white), and stars and sky hidden behind it. "
     "Drag to look onto it from above or edge-on. Feeding rate (as a share of the Eddington limit, only where the thin-disc model applies), Outer edge and "
     "Exposure (automatic, or your own) change it. A real disc is far brighter than the Sun's surface: the card says how many times, with the disc's inner "
     "edge, its hottest ring and the exposure in use. A third preset, A feeding black hole (model), opens with the disc on; Sagittarius A* and M87* open "
     "without it, and switching it on there says that they have no such disc. The guide has a new part on the disc and what the model leaves out; About "
     "lists its papers. Every number comes from Page and Thorne 1974, Luminet 1979, Abramowicz and Fragile 2013, Yuan and Narayan 2014 and Cunningham 1975, "
     "and the picture is checked pixel by pixel against the double-precision reference."},
    {"0.55.0", "The Black Hole Lab: a fourth lab, a black hole in front of our own sky",
     "A new tab, Black Hole, opens the fourth lab. It shows a black hole that does not spin, in front of our own sky (ESA's Gaia map and the brightest "
     "stars), as a camera hovering near it would see it: the black shadow with its exact edge, the ring of light where the sky behind the hole is spread into a "
     "circle, and every star doubled, each image a point at its real place and brightness. Drag to go round the hole, use the wheel to move closer or farther, "
     "down to 1.1 times the horizon's radius, inside the orbit where light can circle the hole: there the shadow covers more than half of the sky. In Controls: Sagittarius A* and M87* with "
     "their measured mass and distance, a mass of your own (the picture does not change with the mass, only the sizes do), the distance, the field of view, "
     "gravity's bending on or off to compare, and a black sky. A card gives the hole's size in km and AU, your distance, the shadow's size on your sky, how "
     "slowly your clock runs, how much bluer and brighter the starlight arrives (not drawn), and for the two real holes the shadow seen from Earth beside the "
     "telescope's measured ring. The guide What is real in this picture says what is exact, what is soft and what is left out (spin, gas, the sky as seen "
     "from there). The Orbit Lab's card of Sagittarius A* opens it. The pictures are drawn by the graphics card from tables the processor fills, so they run "
     "on the same computers as the rest of the app."},
    {"0.54.4", "Housekeeping: the saved papers moved to a private place (nothing changed in the app)",
     "The project's source code is public now, so the saved papers, journal scans and saved web pages behind the app's numbers moved to a separate private "
     "place. The code keeps a list of each one (what it is, where it came from, when it was read) and, for every check that needs numbers from a paper, "
     "a small copy of just those lines. Nothing you see or calculate in the app changed."},
    {"0.54.3", "Groundwork for the Black Hole Lab: its physics, tested (nothing new on screen yet)",
     "The fourth lab's calculations are written and tested, without a screen yet. They cover the special radii of a black hole of any spin (the horizon, the "
     "edge of the region where nothing can stand still, the orbit of light, the innermost stable orbit), the exact path of light past a hole that does not "
     "spin, the black shadow's size seen from any distance, how a camera hovering near the hole sees the whole sky bent (the shadow, the ring of folded "
     "sky, the doubled stars and how much brighter or fainter each image is), a step-by-step light-ray calculator for spinning holes, and the measured "
     "mass and distance of Sagittarius A* and M87*. Every formula comes from a saved paper and is checked against the numbers printed there. The lab "
     "itself, with its screen and controls, comes next (0.55.0)."},
    {"0.54.2", "Small matrices in the calculator: up to 4x4, the Pauli matrices, the eigenvalues of Hermitian matrices",
     "The calculator now takes matrices: [[1, 2], [3, 4]], each row in its own brackets, from 1x1 to 4x4 (rows and columns too, so a matrix times a vector works). "
     "The entries are any expressions, complex ones included, and one unit belongs to the whole matrix: [[0, 1], [1, 0]] eV. You can add, subtract and multiply "
     "them, multiply or divide by a number, raise a square one to a whole power, and use transpose, dagger (the conjugate transpose), conj, det, tr, identity(n), "
     "kron(A, B) and eig(M), which gives the eigenvalues of a Hermitian matrix, smallest first, with the matrix's unit. The Pauli matrices are built in as sigmax, "
     "sigmay, sigmaz (or σx, σy, σz); sigma alone is still the Stefan-Boltzmann constant. A variable or ans can hold a matrix. The answer is drawn as a grid, with "
     "fewer digits when it would not fit (the panel says how many). "
     "What it refuses, saying what to write instead: sizes that do not fit, more than 4 rows or columns, a matrix plus a number, entries of different kinds of "
     "quantity, eig of a matrix that is not Hermitian, and sqrt, exp, sin ... of a matrix. "
     "Limits: eigenvalues only, and only for Hermitian matrices (no eigenvectors, no inverse, no matrix exponential); ordinary 64-bit arithmetic, so trust an "
     "eigenvalue to about 13 digits of the largest one; no uncertainties. Checked against the Pauli algebra, the two-level formula ±√(Δ² + Ω²)/2, spin 1 from the "
     "ladder formula, two spins (−3, 1, 1, 1), determinants by cofactors and 60 matrices built with known eigenvalues."},
    {"0.54.1", "HCl's lines closer to the measured ones, the nucleus drawn exactly, the outer electron at its true share",
     "Molecule Lab: HCl's rotation lines now use the further constants Huber & Herzberg give in their footnotes (γe, a third term of B, and how the centrifugal "
     "term D changes with the vibration). Against the 31 lines Rank and co-workers measured in 1962 the largest gap fell from 1.5 cm⁻¹ to 0.09 cm⁻¹ (R(15)). The other "
     "isotopes have no measured constants here: all of theirs, the new ones included, are scaled from H³⁵Cl by mass, and the text says so. Still left out: the "
     "Herman-Wallis change of the strengths, hot bands and the terms beyond the footnotes. Atom Lab: the nucleus's Density look is now the exact sum of the charge "
     "density along each line of sight, with no random points and no blur; it is worked out again only when the nucleus or the view's angle changes, so a still "
     "picture costs almost nothing. New choice for a whole atom with jumps (Cloud and Cloud 3D): Outer electron's dots, Easier to see (as before: a quarter of "
     "the dots more than its share) or True share (each electron its own share: sodium's outer electron is 1 in 11 of the dots, so its cloud and jumps are faint). "
     "Most realistic picks True share; the readout says which is drawn. The Slice view is unchanged."},
    {"0.54.0", "Atoms in a magnetic field",
     "The Atom Lab has a new section, Controls › Magnetic field: a field from 1 mT to 100 T on a log slider (or typed in tesla), off at the start, and where you look "
     "from: across the field or along it. The data sheet's Spectrum tab then draws the chosen line split into its Zeeman components, π and σ told apart by marks and "
     "a legend, each with its shift in cm⁻¹, nm and Lorentz units on hover, and a small diagram of the line's two levels with their M sublevels and the allowed "
     "jumps. One sentence says whether the field is weak, strong (Paschen-Back) or in between, with both numbers. The lab solves each LS term's levels in the field "
     "exactly, from the weak field through the region between to the strong field; the strengths come from the dipole matrix elements. The readout gives the "
     "level's Landé g from LS coupling beside NIST's measured one, where NIST lists it (newly read: potassium, zinc, barium, ytterbium, mercury). The Data panel "
     "has the components as a table, a component's shift against the field, and a worked analysis: e/m from the normal Zeeman triplet. With the field off nothing "
     "in the lab changes."},
    {"0.53.1", "Bonds that stretch unevenly and break",
     "Hydrogen, nitrogen, oxygen and hydrogen chloride have a new tick box under their stretch: Anharmonic (real bond). The atoms then swing as they really do in a "
     "bond, along a Morse curve: hard when they push together, soft as they pull apart. Choose the level v: the swing grows, slows and leans outwards, and the "
     "readout gives its two turning points and its average length. One step above the last bound level the atoms fly apart; Reset brings them back. The new "
     "Bond energy card draws the curve with every level (the measured ones solid, the model's dashed) and puts the model's dissociation energy beside the "
     "measured one: the two-constant curve is 4 to 24 % too deep. It also compares the curve's rotation-vibration constant αe with the measured one. The Data "
     "panel's Vibration levels table goes up to the last bound level, and its Birge–Sponer analysis fits HCl's measured band origins (Rank et al. 1962)."},
    {"0.53.0", "Molecules turn: rotation levels, populations and HCl's spectrum",
     "The Molecule Lab's seven straight molecules can now turn. Controls › Rotation picks a level J (by default the one most molecules are in at the lab's "
     "temperature) and Turn spins the picture at the classical rate of that level's energy, slowed down five trillion times. The new Rotation card shows the levels "
     "to scale with the share of molecules in each, and the lines light makes: hydrogen chloride's pure-rotation lines and its infrared band near 2886 cm⁻¹, with "
     "its R and P branches and the gap between them. Point at a line for its name, wavenumber and levels; HCl's own lines also show the measured value from 1962. "
     "Choose HCl's isotope (H³⁵Cl, H³⁷Cl, D³⁵Cl, D³⁷Cl or the natural mix, which draws both chlorines). Hydrogen shows its 3-to-1 ortho and para, oxygen and carbon "
     "dioxide their missing levels. The Data panel has tables of the levels and lines and a worked analysis: HCl's bond length from its spectrum. The constants are "
     "the measured ones (Huber & Herzberg, NIST CCCBDB); the isotopes other than H³⁵Cl are scaled by the Born–Oppenheimer rules, and the guide's Rotation section "
     "says what the card leaves out."},
    {"0.52.1", "Spectral colours from the CIE observer, and smaller fixes",
     "Spectral lines are now coloured from the CIE 1931 standard observer, the colour science the app already uses for the stars and Earth's air, instead of a "
     "rough rule of thumb. A screen cannot show the pure colour of one wavelength, so each line gets just enough white mixed in to fit, keeping its hue: lines look a "
     "little paler and the red end slightly pinker than before, and the spectrum card says so. Atom Lab: the quark mass bar names the nucleon you are in (a neutron's "
     "quarks are 1.2 % of its mass, a proton's 1.0 %); the phase ring hides inside a nucleon, where it labels nothing; the level atoms say how much denser their outer "
     "electrons are drawn and why; the Glow's bright core is no longer clipped white. Molecule Lab: water opens side-on and its O no longer glares through the "
     "Density surface; methanol opens with its carbon in view. Orbit Lab: in Simple, Earth and the Moon sit on either side of their balance point, as in Real; a "
     "planet smaller than a pixel keeps its measured colour; Titania's and Oberon's orbits were fitted again to JPL Horizons and are now a little closer."},
    {"0.52.0", "A tidy-up: no game parts, credits put right",
     "The game parts are gone: the Achievements page and its trophy button, the pop-ups, the hidden \"You are awesome\" line among the facts, the clickable word in the "
     "Atom Lab guide and Settings' Progress and Developer rows. Monochrome is now an ordinary theme in the picker, looking as before. Journey stays. A settings file from "
     "an earlier version still opens; its old lines are skipped. About now credits SDL3 and Dear ImGui (version, licence, link) beside stb_image, and their licence texts "
     "come in the zip's licences folder. Element jumps with every electron shown are paused after radium: the atoms without them say why, and no longer promise "
     "more. No physics changed."},
    {"0.51.3", "Radium's jumps, with every electron shown",
     "Radium gets level jumps like calcium and barium: 17 levels with clouds computed with GRASP2018 (7s2, 7s7p, 7s6d, 6d7p, 7s7d), every energy within 4.7 % of NIST's "
     "(the resonance line 7s7p 1P1: -1.3 %), NIST's wavelengths and rates, and the app's own rate where NIST has none (marked as computed, not measured). The calculation "
     "failed before because its orbitals were not quite orthogonal (a long Rydberg orbital was cut short when the others were made orthogonal to it); this is fixed. "
     "7s8s, 7s9s, 7s8p and 7s5f stay without a cloud (more than 5 % off, or the 8p orbital did not converge). Ytterbium is not ready yet: its ground state now converges, "
     "but only two of its levels come within 5 %."},
    {"0.51.2", "Real eclipse shadows, Earth's air and Saturn's rings from Cassini",
     "Eclipses are drawn where they really happen. Every point of a planet or moon with a map gets the share of the Sun's disc that a moon or planet in front hides "
     "(the overlap of the two discs, worked out from the real places): the Moon's shadow on Earth (try 8 April 2024, 18:17 UT), the dots Jupiter's and Saturn's moons cast on "
     "their planet, Earth's shadow on the Moon and a planet's shadow on its moons. The moons are still drawn farther out than they are, so a shadow is where it really falls, "
     "not under the drawn moon. Left out (said in Learn): the Sun's limb darkening, the planets' flattening in their shadows, the red light Earth's air bends onto the "
     "eclipsed Moon, and the grey moons without a map. "
     "Earth is now seen through its air: Rayleigh scattering in single scattering, with the air's optical depth from Bodhaine et al. (1999) (through colour-science's "
     "implementation, checked against its own example to 7 digits), sunlight as D65 through the CIE 1931 observer, and the path through a spherical shell (finite at the "
     "horizon). So Earth looks bluer and hazier towards its edge and the light near the terminator is redder, as from space. "
     "Saturn's rings are drawn from Cassini's UVIS occultation of beta Centauri (18 Aug 2008, NASA PDS): the optical depth every 10 km, averaged over each drawn pixel so the "
     "total light is kept; the rings' shadow on Saturn uses the same profile (32 bands, was 4). The Cassini Division, the Encke gap and the C ring's ringlets are now where they "
     "are. Tests: the overlap of two discs against the textbook lens area, the length of Earth's umbra by similar triangles, the air's optical depth against colour-science, "
     "the C and A rings' mean optical depth against NASA's fact sheet and the A ring's edge against it (17 km off; 50 allowed)."},
    {"0.51.1", "Data panel: fixes from an outside review, a held parameter, log axes, correlations, and Mercury's perihelion from a recording",
     "An outside review of 0.51.0 (as a physics professor would use it) checked the fitting against independent references: York's straight line with errors in both variables "
     "(Pearson's data, every digit the same), the χ² probability against scipy (12 digits), and the worked analyses against published values. It found these, now fixed: "
     "the planets' \"Mass against radius (log-log)\" was fitted on linear axes (Jupiter alone decided it: n = 7); it is now a straight line of log₁₀ M against log₁₀ R (n = 2.4: "
     "the rocky planets and the giants mixed, as the text says). Jupiter's GM from its moons came out 0.3 % high, because with n free the fitted A moves with n "
     "(their correlation is −0.9998): the worked analysis now holds n at 3/2 (GM within 0.03 % of the card's), and a second one fits T² against a³ through the origin. "
     "The dwarf planets' and comets' Kepler fits give n = 3/2 exactly because JPL works their periods out from a with Gauss's constant: the text now says the fit is circular. "
     "The hydrogen and quantum-defect texts now explain why the error bars are far smaller than the distance from the textbook value (fine structure; two Ritz terms at low n). "
     "A Gaussian or Lorentzian fit that ended on a negative width now flips its covariances too (the 1σ band was wrong then). Headers like \"dV\", \"ΔT\" or \"δ x\" after their column "
     "are read as its uncertainty. The residual plot's scale is no longer cut off at the left, and it shows +, 0 and −. The ionisation limit is written to NIST's digits. "
     "New: hold any parameter at a value you type (the others are fitted with it fixed); x³ as an axis transform; logarithmic axes (the drawing only); the parameters' correlations "
     "under the fit and in Copy results and the CSV; a weighted fit's errors scaled by √(χ²/dof) on request. The Orbit Lab's recording now also keeps the energy, the angular momentum, "
     "a, e and the longitude of perihelion ϖ, with worked analyses for energy conservation and the perihelion's turning; a long recording keeps every other sample when it fills up, "
     "so it can run for centuries. Measured in Real mode over 40 years, Mercury's perihelion turns 528″ per century from the planets (Clemence 1947: about 532) and 42.98″ faster "
     "with relativity on, Einstein's value."},
    {"0.51.0", "The Data panel: every lab's data as tables you can plot, fit and export, your own data too",
     "A new button in the top bar (a small graph; or Ctrl+D) opens the Data panel. It turns the labs from something to look at into something to measure with. "
     "Tables: each lab hands over its own data, every value with its unit and its source. Orbit Lab: the planets, the dwarf planets, the big moons and the comets (orbits, sizes, masses). "
     "Atom Lab: NIST's measured lines of the chosen element (its one-electron ion and the neutral atom), the quantum defects of Li, Na, K, Rb, Cs and Fr, the NIST levels of the "
     "two-electron atoms, the element's isotopes (AME2020 binding energies, NUBASE2020 half-lives) and the binding energy of all 2,549 measured nuclides. "
     "Molecule Lab: the vibrations of the molecule (measured and calculated), of all 15 molecules together, the vibrational ladder of the diatomics and the rotational constants. "
     "Plot any column against any other, as it is or as 1/x, 1/x², x², √x, ln x or log₁₀ x, with error bars; pick the rows of one planet's moons or one l. "
     "Fit a straight line, a line through the origin, a polynomial, a power law, an exponential (with or without a constant), a rise to a limit, a Gaussian or a Lorentzian peak. "
     "The fit gives every parameter with its 1σ error and unit, χ² and χ²/dof with the chance of a χ² that big (with uncertainties), or the scatter (without), the residuals and the "
     "curve's 1σ band; uncertainties in x are carried too. The fitting was tested against the certified answers of NIST's Statistical Reference Datasets: 26 problems, each from NIST's two starting points, "
     "to at least 6 digits (from the far start of the two hardest the fit stops and says so instead). "
     "Worked analyses come with each table, with the answer a textbook expects: Kepler's third law (n = 3/2 for the planets; the GM of Jupiter from its moons), the Rydberg constant "
     "from hydrogen's lines (about 1 part in 10^5 from CODATA's, the fine structure the Bohr formula leaves out), the Rydberg-Ritz quantum defects, B3LYP's frequency scale factor (0.966 from 78 vibrations; "
     "CCCBDB: 0.9675 ± 0.019), a Birge-Sponer plot, the binding-energy curve. "
     "Your own data: type or paste a table (commas, tabs or spaces; a first line like \"L (m), T (s), σT (s)\" gives the units and uncertainties). "
     "Orbit Lab: record the selected body as time runs, then check vis-viva (v² against 1/r is a straight line with slope 2GM☉). "
     "Tap a number in a table to put it into the calculator with its unit; Send to calculator keeps the fitted parameters there as fit_… variables. "
     "Save as CSV writes the table with its units, sources and the fit to the Documents folder; Copy table puts it on the clipboard for a spreadsheet. "
     "Menu: Copy setup and Paste setup keep or share the whole state of the labs (every setting, the date and the selected body) as a short text."},
    {"0.50.3", "Fixes after an outside review of the whole app: settings that were lost, number formats, the calculator, texts that were wrong",
     "An outside review of 0.50.2 listed 59 points; each was checked against the code first, and only the real ones were changed. "
     "Settings: an Atom Lab level n of 5 or 6 and an orbital from 5s to 6f are kept after a restart (they came back as n = 4 and 1s). "
     "Calculator: whole numbers below 10^15 are shown in full (3^21 = 10460353203); 1 − 0.999999999999999 is no longer rounded to 0 (only 4 rounding steps count as noise); "
     "(−8)^(1/3) is −2 on every line, also on one that uses i; a complex variable after \"to\" is refused; the 1000-character limit counts characters, not bytes. "
     "Numbers like 9.97 × 10^29 kg are shown as 1.0 × 10^30 (they showed as 10.0 × 10^29). "
     "Planets: the map's sharpness now follows each pixel (it was set for the middle of the disc, so the poles and the edge were too sharp and could shimmer). "
     "Texts: the proton's and deuteron's charge radius says CODATA 2022 (it said IAEA table); the light nuclei are not all Gaussians (deuteron, C-12, O-16 have measured shapes); "
     "the atom speed says it is the rms speed; hair to quark is about 80 trillion (not over 100 trillion); the 90 % outline's Off tooltip; Mercury's map is tinted; ice is 8 % less dense in both places; "
     "methanol's twist explains why its calculated wavenumber is off (the calculation is at the measured shape, not its own minimum). "
     "Also: Esc closes Settings without folding the Legend, and closes the calculator as its button says; the benchmark's Start is greyed with the reason when it cannot start; "
     "a slider under the pointer no longer moves the chart of nuclides at the same time; VSync says it is for this run only."},
    {"0.50.2", "Calculator fixes after a second outside review: ω and f by the powers of ħ and h, exact Clebsch-Gordan, tiny numbers, complex numbers, variables",
     "An outside review of 0.50.1 found wrong answers and missing pieces in the calculator. "
     "(1) Whether a frequency is angular (ω) now follows the physics: the powers of ħ and h in the line are carried like units. E/ħ is ω and gets the note with f = ω/2π; "
     "a line that divides by h, like the proton's Larmor frequency gp e ħ/(2 mp) B / (2h) = 21.29 MHz, is cyclic and gets no note (0.50.1 offered 3.39 MHz). "
     "With no \"to\" the note says first that a rate has no 2π. "
     "(2) 1 eV to rad/s is E/ħ = 1.519 × 10¹⁵ rad/s (it was E/h); rad/s to eV goes through ħω, and 1 GHz to rad/s multiplies by 2π. "
     "(3) Clebsch-Gordan and 3j symbols are summed exactly: about 14 digits for every j up to 200 (0.50.1 lost digits from j = 20 and failed at j = 50). "
     "(4) A value too small for the computer (under 2.2 × 10⁻³⁰⁸) is 0 with a note, so a partition function's far terms no longer stop the sum; erfc(27) is 0. "
     "(5) The Atom Lab's fine-structure gap is the exact Dirac difference (the old leading order was 29 % low for uranium); the Zeeman shift uses gJ with the measured electron g-factor; "
     "hydrogen-1 no longer gets a binding energy per nucleon of 0. "
     "(6) binom, assoclegendre, cg and wigner3j give 0 with a note where 0 is the standard value, instead of refusing; cg's messages say J, not j3. "
     "(7) New: 1 eV to nm and 500 nm to eV (a photon's vacuum wavelength, E = hc/λ); complex numbers with i (re, im, conj, abs, arg, exp(iπ) = -1); "
     "named variables: x = 2 m on one line, x on the next. Matrices are not in yet."},
    {"0.50.1", "The calculator for quantum mechanics: ω and f, spectroscopy conversions, QM constants and functions",
     "An outside review worked 40 quantum-mechanics calculations through the calculator. Every closed-form number was right (they are now tests), but a few things were wrong or missing. "
     "(1) A line with ħ that gives a frequency, like the Rabi frequency e a0 E / ħ, is an angular frequency ω in rad/s; it used to be shown in Hz without a word, 2π too big for a cyclic frequency. "
     "Now a note says so and gives f = ω/2π (a line with h, like 1 eV / h, is cyclic and gets no note; a rate written \"to 1/s\" neither). "
     "(2) 1/(4 π ε0) e^2 / r is read as (1/(4 π ε0)) × e^2 / r, the textbook way, with a note; other unclear lines quote your own brackets. "
     "(3) After \"to\", energies, wavenumbers, frequencies and temperatures convert into each other, and masses into energies: 1 eV to cm^-1, 1 eV to K, 1 u to MeV, each through one "
     "exact factor named in a note (hc, h, k, c²). Adding them still refuses. "
     "(4) New constants from CODATA 2022: μB, μN, the Hartree energy (and the unit hartree), R∞, ge, ae, gp, the proton and neutron moments, the muon, deuteron and alpha masses, "
     "the Compton wavelength, the flux quantum, the von Klitzing and Josephson constants, the Fermi constant and the atomic units of length, time, field and dipole moment; new units: "
     "debye (D), gauss, erg and barn. μN is now the nuclear magneton, not a micronewton. "
     "(5) New functions on plain numbers, with commas between their arguments (commas stay refused everywhere else): fact and n!, gamma, lngamma, erf, erfc, besselj, sphbessel, "
     "legendre, assoclegendre (with the Condon-Shortley phase), laguerre, assoclaguerre, hermite, binom, Clebsch-Gordan cg(...) and wigner3j(...), sum(expr, k, a, b) and solve(expr, x, a, b). "
     "(6) The Atom Lab's formulas add the Dirac level, the fine-structure gap, the Zeeman shift with Landé's g, the mean radius, the 2p → 1s rate and, for hydrogen, Fermi's 21-cm "
     "line, each checked against the lab's own functions and NIST. The help and a folded \"QM examples\" list show how. Complex numbers and matrices are not in yet."},
    {"0.50.0", "Light in linear light, real star colours and brightness, the Moon's libration, eight moon maps",
     "An outside review of 0.49.4 found ten things to fix in the pictures and the records. Orbit Lab: (1) the blue rim round Earth (about 1,200 km thick on screen, lit on the night side too) had no source; "
     "the air's scale height is 8.5 km (NASA's Earth fact sheet), far under a pixel, so it is gone. (2) Planets and moons are now lit in linear light: the colours are decoded from sRGB, multiplied by the cosine of "
     "the Sun's angle (Lambert's law) and encoded again, so the day-night edge is no longer far too sharp; the 12 % light on the night side and the white highlight (both without a source) are gone, and a night "
     "side is the colour of the sky. The Moon follows McEwen's lunar photometric function, fitted to Galileo pictures (at full Moon its edge is nearly as bright as its middle). (3) A star's colour is now a black "
     "body at the temperature of its B−V (Ballesteros 2012) through the CIE 1931 observer, without the 30 % white that was mixed in; the far-away stars use their own measured B−V (HYG v4.1), not a guess "
     "from the spectral letter. (4) Every star's brightness follows one stated rule: the drawn light grows as the square root of its real light (10^(−0.4 m)), so 5 magnitudes are a factor of 10 on screen; the three "
     "size classes and the hand-tuned glow are gone. (5) Our Moon turns by the IAU rotation model from 1800 to 2100: its tilted axis and its libration, within 0.006° of JPL Horizons. (6) Mercury's map is tinted "
     "to Mercury's measured colour (Mallama 2017). (7) Io, Europa, Ganymede, Callisto, Rhea, Triton, Charon and Phobos wear black-and-white USGS maps and turn by their IAU models; the other moons stay grey and "
     "their cards say their colour is not measured here. Still the same drawn tint for all ring systems (no table of measured ring colours was found), and Pluto stays black and white (no colour map of all of it). "
     "Atom Lab: the Orbit view's shell rings no longer turn; the turning was only for the eye. Molecule Lab: every bond length and angle is now checked by the tests against CCCBDB's own saved pages."},
    {"0.49.4", "The Sun in its measured colour, no made-up glow, a sourced habitable zone",
     "An outside review found three things in the Orbit Lab that had no source. (1) The Sun was drawn orange-yellow from a typed colour table. It is now drawn in its measured colour: its spectrum above the atmosphere (ASTM G173-03) seen by the CIE 1931 standard observer and shown in sRGB, which is nearly white (x = 0.323, y = 0.333, about 5,930 K). (2) A yellow glow out to three times the Sun's radius is gone: the real corona is a millionth to a billionth as bright as the disc (NASA) and cannot show next to it. (3) The green habitable-zone band is now 0.95 to 1.68 AU from Kopparapu et al. 2014 (an Earth-mass planet: the runaway and the maximum greenhouse limits); it was 0.95 to 1.67 AU with no source. Still not sourced, and now said on the Sun's card: its darker edge (limb darkening) is the app's own rule; the measured law could not be opened yet."},
    {"0.49.3", "Exact photon jumps, Dirac clouds for heavy ions, the Lamb shift, true scale",
     "An outside review asked for four fixes in the Atom Lab. (1) A photon jump is now the exact one-photon process, slowed down: after an emission the upper level's share of the atom falls "
     "as e^(−t/τ) (Weisskopf and Wigner), and the photon's wave has a sharp front with a tail that fades the same way; an absorption is drawn as its time reverse. The photon carries the phase away, "
     "so during a jump the atom is a mixture of the two levels: the cloud no longer sloshes (that is what a classical light wave does, and Superposition still shows it). A jump lasts 2.2 s. "
     "(2) The whole atom is drawn at its true scale by default; Shells enlarged is still a choice. (3) A one-electron ion is drawn from the Dirac equation from cobalt (Z = 27) on, or at any Z "
     "(new setting Cloud model): the exact state n, l, j with its large and small components, lined up (m = j) or round. Uranium's 1s is 17 % smaller than the non-relativistic one. Jumps "
     "between Dirac states use the Dirac lines and pick j by the line strengths. (4) The spectrum cards use the Dirac levels by default, with the Lamb shift of the levels n = 1 and 2 from "
     "Yerokhin and Shabaev (2015) for Z up to 110: hydrogen's Lyman-alpha now agrees with NIST to 2 parts in a billion, and its 2s and 2p levels are split by 1058 MHz, the Lamb shift itself."},
    {"0.49.2", "Photons leave and arrive during the jump; the Oort cloud at NASA's distances",
     "A photon is now drawn as a wave train as long as its jump: an emitted photon streams out of the atom while the electron jumps, and an absorbed one streams in while it jumps (before, the "
     "emitted photon appeared only after the jump and the absorbed one vanished before it). A ring at the nucleus glows in the photon's colour while the light passes it, and the drawn wavelength grows "
     "with the real one (shorter for ultraviolet, longer for infrared; not to scale, as the label says). The same for hydrogen, the outer electron of Li to Fr and the level jumps of He, Be, Mg, Ca, Sr, Ba, Zn, Cd and Hg. "
     "The note \"How a jump is drawn\" explains it, and for the relativistic model it now describes the correlated GRASP2018 clouds it really draws. The Oort cloud is a round shell from 2,000 to 100,000 AU "
     "(NASA's edges: inner 2,000 to 5,000 AU, outer 10,000 to 100,000 AU); its dots are placed exactly on the graphics card now. The unused Estonian language entry was removed."},
    {"0.49.1", "The Windows build of 0.49 fixed; the Oort cloud as before",
     "0.49.0 did not build on Windows (a name in the calculator's panel was given twice); fixed. The Oort cloud's new edges were taken out again: they are kept for a later "
     "version (the made-up dots must first pass a graphics precision check), so the Oort cloud is as in 0.48."},
    {"0.49.0", "A physics calculator that keeps track of units, with formulas for what you pick in the labs; the Voyagers' real places before 2026",
     "New: a calculator (the calculator button at the right of the top bar, or Ctrl+=). Type, or tap its keypad (numbers, signs, common units and constants, ⌫). It works out a line as you go and keeps track of units: 100 km / 2 hr gives a speed, "
     "5 km to mi changes the unit, and 5 km + 3 s is refused with a reason instead of a wrong number. It knows the physical constants (CODATA 2022, with their sources and "
     "uncertainties: c, hP, ħ, e, G, me, mp, kB, NA, ε0, μ0, a0, α ...) and units from fm to light years. It shows how it read the line, so you can check it; where a line "
     "could be read two ways (4/3 π r^3, 1/2 km) it asks for brackets instead of guessing. Pick a planet, an atom or a molecule and its formulas appear (escape speed, "
     "the light from an electron jump, molecules in a gram ...), filled in with its numbers and worked out in the open. Enter keeps a line in the history (remembered between runs) "
     "and makes it \"ans\". "
     "Orbit Lab: before 30 Sep 2026 the Voyagers are now at JPL Horizons' places (once a year from 1990); before, their 2026 straight line was also run backwards, which put "
     "Voyager 2 inside Neptune's orbit in 1990, 20 AU from its real place. Voyager 1 is no longer called the most distant spacecraft before it passed Pioneer 10 (17 Feb 1998), "
     "and before 2012 the text says it will leave the Sun's wind. About no longer shows a wrong version number."},
    {"0.48.0", "Review fixes: drawings labelled as drawings, sources credited correctly, superheavy families marked \"expected\"",
     "Orbit Lab: the comet tails are now labelled \"drawn for show, not to scale\" (legend and card); the card no longer says a comet beyond 4 AU is \"frozen and dark, no tail\", "
     "which is not true of every comet. The patches on the Sun are labelled as markers that only show the spin (they turn as one body; the real Sun does not). Halley's 2061 return and "
     "Swift-Tuttle's 1992 and 2126 perihelia are now taken from JPL Horizons (the same dates; before, they were credited to JPL but taken from Wikipedia). Neptune's rings use the "
     "planet's own radius, 24,764 km (they had 24,766). Atom Lab: rutherfordium to oganesson show their family as \"expected\": it comes from their column of the table, as their "
     "chemistry has been tested with single atoms or not at all (calculations expect copernicium to act like a noble gas and oganesson possibly to be a solid semiconductor: Smits et al. 2023). "
     "With every electron shown, an alkali atom's readout now says how much denser its outer electron is drawn (sodium 3.5 times, caesium 14.5 times its true share). "
     "Molecule Lab: ozone's note now matches its drawing (two 1.5 bonds); the app no longer calls every shape an equilibrium one (CCCBDB marks 6 of the 15 so), and the data sheet "
     "says why measured and shape-derived rotational constants differ (oxygen's measured value is for its lowest vibrational level). About: the hydrogen-1 mass is credited to NIST and "
     "the proton radius given as 0.84075 fm, the value the app uses."},
    {"0.47.4", "Atom Lab: calcium, strontium, barium and mercury jump between levels; magnesium, zinc and cadmium recomputed; the app's own rates where NIST has none",
     "Atom Lab, with every electron shown: calcium, strontium, barium and mercury now jump between levels too (33, 31, 29 and 6 levels with clouds; the resonance line of each "
     "within 1.5 % of the measured energy). Each atom's clouds now come from one set of orbitals shared by all its levels, which keeps a term's J levels in the measured order "
     "(calcium's 4s4p ³P: 52 cm⁻¹ between J=0 and J=1, as measured) and made magnesium far closer to the measured levels (worst 0.5 %, was 4.8 %, and its 3p² levels now have "
     "clouds); zinc and cadmium were recomputed the same way. Levels whose calculation is more than 5 % off stay listed with NIST's energy and the reason, among them calcium, "
     "strontium and barium's lowest d levels (6-10 % off: the correlation of the d electron with the core that this calculation leaves out). Where NIST gives no rate for a jump, "
     "the app now shows its own, marked \"computed by this app (GRASP2018), not measured\", with how far this app's rates are from NIST's for that atom (the median over the lines "
     "both have: 4 % for magnesium to 41 % for strontium). Radium and ytterbium are not computed yet: their calculations did not converge (next version)."},
    {"0.47.3", "Atom Lab: jumps between whole levels for helium, beryllium, magnesium, zinc and cadmium; every atom says whether it has jumps",
     "Atom Lab, with every electron shown: helium, beryllium, magnesium, zinc and cadmium now jump between LEVELS of the whole atom, as textbooks name them: configuration, "
     "term and J (for example 3s3p ¹P° J=1). The Photons section lists the levels that have a cloud (tap one to put the atom there) and the jumps from the level the atom is in, "
     "strongest first, each with NIST's measured wavelength and its rate A with NIST's accuracy grade, or \"rate not known\" where NIST has none. Every level's cloud is computed "
     "for this app with GRASP2018 (106 levels); its calculated energy is within 0.12 % of the measured one for helium, 1.9 % for beryllium and 3.8 to 4.8 % for zinc, cadmium and "
     "magnesium, shown as the cloud's quality, while the energies shown stay NIST's. A level whose calculation is more than 5 % off is listed without a cloud, as are the "
     "other measured levels, with NIST's energy. A jump is offered only between two levels a lab has measured. A new switch draws the atom lined up (the state M_J = J, as in a "
     "magnetic field; the default) or round (the free atom, every direction alike); it works for lithium to francium too. Atoms with no jumps to show say why, under greyed "
     "Emit and Absorb buttons: nothing measured yet (mendelevium and the superheavy elements), levels measured but not matched to lines in NIST (selenium, zirconium, niobium, "
     "praseodymium, promethium, rhenium, osmium), measured but not in NIST (protactinium to einsteinium; fermium and nobelium with a few laser lines), or clouds not computed yet. "
     "Calcium, strontium, barium, radium, mercury and ytterbium were computed too, but too far from the measured levels (their d and f shells mix in): they follow in 0.47.4."},
    {"0.47.2", "Atom Lab: lithium to francium with every electron from a correlated calculation; a J = 1/2 level is round",
     "Atom Lab, with every electron shown for lithium, sodium, potassium, rubidium, caesium and now francium: every level's cloud is new, computed for this app with GRASP2018, "
     "a relativistic many-electron program. The outer electron and the core now act on each other (the core is polarized), so each level's calculated energy is within 0.1 to 3.4 % "
     "of the measured one (0.1 to 0.4 % for sodium); the readout gives this as the cloud's quality, while the energies shown stay NIST's. Every electron is drawn as the level's "
     "natural orbitals with their occupations, which are no longer whole numbers (sodium's 3p electron is 0.999 of an electron in 3p). The outer electron has the shape of its "
     "level aligned along z (M_J = J, as in a magnetic field): a level with J = 1/2 is round in every state, so sodium's 3p 2P1/2 is now round (0.47 drew it as a pz dumbbell) "
     "and 3p 2P3/2 is a ring around z; a free atom is an even mix of all directions and its average is round, as the readout says. The lab now knows which J level the electron "
     "is in: from 3p 2P3/2 it gives only the D2 line. Francium joins the jumps (10 levels, its 7 NIST lines; 9s is left out, its calculation was 11 to 20 % off). The "
     "Non-relativistic model keeps the 0.47 pictures (frozen core, m = 0 shapes); francium has no jumps there."},
    {"0.47", "Atom Lab: tap a shell and the outer electron goes there, in its real shape",
     "Atom Lab, with every electron shown for lithium, sodium, potassium, rubidium or caesium: a new row in Photons, \"Put the outer electron in\", has one button for each shell the lab "
     "has an orbital for. Tap one and the outer electron moves there with the jump's cross-fade, up, down or between levels that no line joins, with no photon. A real atom changes level "
     "only by taking in or giving out energy (light or a collision); here you place it to see its shape. The outer electron is now drawn in its real shape in Cloud 3D, Cloud and Slice: "
     "its m = 0 orbital (a p as pz, a d as dz²) pointed along z, as One orbital already showed it, so 2s to 2p turns a round cloud into a dumbbell. A free atom has no preferred direction, "
     "so its true average picture is round; the readout says so. The \"Check a jump\" rows are now \"Look up a jump (NIST)\": they look up a jump's rate and never moved the electron. The atom no longer jumps up when the Element picker opens."},
    {"0.46.7", "Planets lit from the real Sun, Saturn's ring shadow, ozone's equal bonds",
     "Orbit Lab: planets and moons are lit from the real direction of the Sun in space, not from its direction on the screen plus a fixed tilt towards you, so in 3D a planet on the far side "
     "of the Sun is a thin crescent and seen from above every planet is half lit, as it should be. Saturn's shadow on its rings uses the same real light, so its length and shape are right, and "
     "its edge is now sharp and smooth (it was a staircase). The Simple mode uses JPL's GM of the Sun (DE440), as the Real mode does: until now it was 0.004 % too large (Kepler's law with a "
     "365.25-day year), so its speeds were 0.002 % too fast. The Sun's label moves below the Sun when it would sit on the barycentre's crosshair. "
     "Molecule Lab: ozone's two bonds are drawn alike, each of order 1.5 (one solid and one dashed stick, as benzene's), since both are measured 1.278 Å long; a bond card says "
     "\"1.5 bond (delocalised)\" (it said aromatic). Atom Lab: zoomed into one nucleon, the info box no longer starts with the electron's orbital, which is far outside the view; "
     "the gluons' 40 % share of the momentum names its probing scale (2 GeV) and says it grows slowly with energy."},
    {"0.46.6", "Fixes from an outside review: pictures that contradicted the physics, and the greenhouse notes",
     "Molecule Lab: the Density surface is lit from the viewer, so CO₂'s two oxygens and water's two hydrogens get the same colour (the light came from the upper left and bleached one side). "
     "Every bond had stripes, because its two halves put their highlight on opposite sides; benzene's aromatic bonds are now one solid and one dashed stick, the usual drawing. "
     "The greenhouse note names the bands that do the work: CO₂'s bend at 15 µm (its tallest stick, 4.3 µm, lies where Earth gives off about 1 % as much light) and water's bend and "
     "far-infrared rotation band (its stretches near 2.7 µm hardly matter). CO₂'s symmetric stretch says that 1333 cm⁻¹ is never seen: Fermi resonance splits it into the lines at "
     "1285 and 1388 cm⁻¹. N₂ and O₂ have no allowed infrared band, but absorb a little in collisions (it matters on Titan). Each molecule's note quotes its own fastest vibration, not water's. "
     "Atom Lab: with Shells enlarged the scale bar says it is true only at the outer edge; zoomed into the nucleus the notes no longer speak of spread-out shells; hydrogen's Electrons All "
     "no longer says Dirac-Hartree-Fock (hydrogen is the exact one-electron solution). Orbit Lab: zoomed in on Earth there were two paths about 10,000 km apart; both were drawing errors "
     "(straight pieces of a curve), and the orbit and the trail are now drawn as smooth curves near the picture. In Easy to see the Legend says the Sun is drawn about 32 times too big. "
     "A near-circle orbit gives its closest and farthest distance (Earth 0.983 to 1.017 AU)."},
    {"0.46.5", "Atom Lab: more realistic protons, neutrons and light nuclei",
     "The nucleus is see-through: the nucleons' glows add up, as their densities do, instead of each covering the ones behind it (that made dark holes), and they are no longer blurred. "
     "The deuteron comes from a real wave function (CD-Bonn, Machleidt 2001): each snapshot draws a new distance between its proton and neutron (often more than 3 fm), and its charge has "
     "a dense middle and a long halo. A new choice, Controls › Nucleus › Deuteron spin, shows each spin state: a little longer along the spin axis (M = ±1) or a little flatter (M = 0). "
     "Carbon-12 and oxygen-16 take their measured charge densities (de Vries 1987): flat in the middle, oxygen slightly lower right at its centre. The info line gives each nucleus's own "
     "edge (it said 2.4 fm for all). A lone proton's charge cloud uses the CODATA radius, 0.84075 fm, as everywhere else (it used an older 0.878 fm). More info says that a deformed "
     "nucleus's shape is its own frame's: slow measurements see it in all directions at once (STAR, Nature 2024)."},
    {"0.46.4", "Atom Lab: an outside review's fixes: nucleus shapes, the 2s decay, unbound nuclei and more",
     "Carbon-12 was drawn stretched against its own measured quadrupole moment (+0.06 b, flattened; the data tool had missed it): its flattening is now known, and it is too strong for the lab's shape model (|β₂| up to 0.6), so it is drawn round and the card says why. Shapes from a measured B(E2) now keep its intrinsic moment exactly "
     "(they were drawn 7 to 30 % too deformed; neon-20 was past the lab's own limit). The 2s state: besides two photons at once it can give off one magnetic photon, which is the faster way "
     "in heavy ions (NIST ASD: 3.6 times faster for barium). Unbound nuclei give NUBASE2020's own time (boron-9: 8×10⁻¹⁹ s), not one 10⁻²² s for all. The one-electron atom's sizes and "
     "times now use the reduced mass, like its energies (hydrogen 0.05 % larger). Gas speeds use the real atomic mass. The nucleons no longer wobble (a made-up motion). Texts: "
     "the charge-radius note, the proton's mass split, the bond comparison (H₂, 4.48 eV, NIST-JANAF), the 1919 step, round nuclei's shape line, and that only β₂ is drawn."},
    {"0.46.3", "An outside review's fixes: the clock, gravity, the Moon, nucleus shapes and more",
     "Orbit Lab: the date shown is Greenwich time (UT), and the JPL data run on their own clock (TDB), 69 seconds ahead in 2026; the lab now converts between them (IERS leap seconds, "
     "NASA's Delta T before 1972). Before, Earth was drawn about 2,000 km from its real place at the time shown. Earth's Moon is now at its real place in Simple too, as the other 13 big "
     "moons already were. A planet card gives the pull you would feel at the equator, the spin's outward push included (NASA): Saturn 0.91 g, lighter than Earth, Jupiter 2.36 g. The "
     "guide no longer says the planets feel only the Sun's pull when Planet physics is Real. Haumea's size is now the same-volume size of its measured shape, 1,544 km. Atom Lab: "
     "nucleus shapes come from published data: a measured B(E2) for 167 nuclei found in nature (was 11), else the FRDM 2012 model, instead of the lab's own rule. A measured moment is "
     "turned into a shape only where that model gives a deformed nucleus, so oxygen-17 and bismuth-209 are no longer drawn from a rule that does not hold for them. Hot two-atom gases "
     "split into atoms at the real temperatures (NIST-JANAF), and the speed readout follows. The hyperfine text names its source and gives the lab's own 6.7 % gap for holmium. Molecule "
     "Lab: the density surface caption names its 2024 study and no longer calls 0.002 Bader's convention. History: Thomson measured about 1/1,000 in 1897, and Geiger and Marsden's "
     "particles turned back by more than 90 degrees."},
    {"0.46.2", "The project's documents brought up to date",
     "No change in how the app works. The roadmap was checked against the app and rewritten, one list per lab, and the project's notes on its decisions and rules were corrected where they no longer matched the app."},
    {"0.46.1", "Atom Lab: an outside review's fixes",
     "A photon given off now goes to each lower level with the odds of NIST's measured rates (hydrogen 3p: 1s about 88 % of the time, 2s about 12 %), not with equal odds; the last-photon line "
     "says how likely the way taken was. The same holds for the outer electron of Li, Na, K, Rb and Cs. 2s no longer \"stays excited\": the message says it decays by giving off two photons "
     "at once, after about 0.12 s in hydrogen (0.1215 s divided by Z to the 6th, Jentschura 2004). The texts about relativity now agree with what is shown: from cobalt on the energy over the "
     "picture is the Dirac value, and with every electron the Dirac-Hartree-Fock model is relativistic. The notes say that the alkali ground atom and its excited outer electron come from two "
     "related calculations. In the whole atom the outer electron of an alkali atom is drawn denser (the readout says by how much), so it and its jumps can be seen. The readout no longer "
     "covers the top of the picture, and the many-electron readout is shorter (details under More info). The shells legend is hidden when zoomed into the nucleus. A deformed nucleus "
     "such as uranium-238 lies across the first view, so its stretched shape shows. Smaller fixes: heavy one-electron ions are made in accelerators and ion traps, not seen in stars; "
     "two-atom gases (H2, N2, O2 ...) show the speed of their molecules; the SLAC line no longer says the experiments saw three points; one bracket in \"330.393 nm (vacuum, UV; ...)\"."},
    {"0.46", "Atom Lab: a review's fixes, jumps with every electron, a faster cloud",
     "Electrons now starts on All, and the one-electron choice is called Hydrogen-like (old saves keep their choice). For Li, Na, K, Rb and Cs the outer electron can jump "
     "with every electron shown: Emit and Absorb, or a click on a line of the spectrum strip, move it between ns, np, nd and nf orbitals while the inner electrons stay as they are "
     "(a model: the frozen core, Dirac-Hartree-Fock). The photon is a measured line with its transition rate (NIST ASD). The one-electron jumps reach n = 6, the orbitals go up to 6f, "
     "and each emitted line names the route the electron takes, the one NIST gives the largest rate. From an outside review of the Atom Lab: True scale in Most realistic, the Dirac "
     "energy first for Z of 27 or more, no charge cloud for a lone neutron, the deuteron's CODATA 2022 radius, the nucleons' edge from a published diffuseness, the shape rule called "
     "the lab's own estimate, and corrected texts on the nucleus. The Glow cloud is now kept and drawn on the graphics card; the picture is the same."},
    {"0.45", "Quarks: an outside review, corrected",
     "An outside review of 0.44 found eleven places where the quark texts or pictures said more than the data do; all are fixed. The sizes now given are those of the clouds "
     "drawn (about 0.42 fm for up, 0.47 fm for down, weighted by momentum); the 0.63 fm of the papers counts every quark the same. The sea and the fast gluons are measured only "
     "in part, and the texts now say so; fast gluons and sea partons are drawn closer to the centre, where the extrapolation puts them. The glow under the dots is now the same "
     "density as the dots. Gluons are brighter, the antiquark rings fainter. The proton mass split is 32 % / 36 % (the paper's text), the gluon spin at small x is called "
     "unmeasured, and the Simple style's tubes have the measured 0.38 fm radius, with a note that at the drawn distances the real field is a bubble. Zoomed in past the quarks, "
     "a note replaces the legend."},
    {"0.44", "Nucleon tomography: where the momentum is",
     "The Accurate quarks now show what the measurements say about how a proton looks from inside when it flies past very fast. Each dot is a piece of its momentum, carried there by an "
     "up quark (warm), a down quark (cool) or a gluon (grey). Fast quarks sit near the centre and slow ones spread out: the widths come from Diehl and Kroll's fit to all the measured form "
     "factors, the shares from the ABM11 parton densities (gluons about 40 % of the momentum). + Sea now draws the sea's quark and antiquark pairs (the antiquarks as rings), placed as "
     "far out as the COMPASS and H1 measurements say; 0.43 left them out and said where they sit had not been measured, which was too strong. The gluons' spread comes from J/ψ data; "
     "lattice QCD finds a narrower one, and More info says so."},
    {"0.43", "Quark clouds from measured data",
     "The Accurate quarks are now drawn the way the electrons are: as clouds of dots, each dot one place an up quark (warm) or a down quark (cool) may be found, not a quark. "
     "The clouds are not guessed: they are the transverse densities computed from the measured form factors of the proton and neutron (Bradford et al. 2006, Atac et al. 2021, "
     "the method of Miller 2007), seen along the path of a very fast nucleon, the only view in which they are known without a model. The down quark's cloud is wider than the "
     "up quarks', and the neutron's charge is negative at its centre, as measured. + Gluons adds a faint grey cloud of the gluons from lattice QCD; with + Sea the clouds stay the "
     "same, because quark and antiquark pairs cancel in them, and where the sea itself sits has not been measured. Fewer styles: the quark style is Accurate or Simple (balls and "
     "flux tubes, for learning; Cinematic is gone), and the nucleus look is Density or Nucleons (Fuzzy and Hard balls were merged). Saved choices move to the new names. "
     "Smaller fixes: the nuclear density is given as the measured 0.164 per fm³, the Simple quarks are said to be about 150 times too big, the neutron skin names the range the "
     "antiprotonic-atom data allow, and the mass bar and legend no longer hide under the Controls panel."},
    {"0.42", "Quarks as real as science allows",
     "The deepest zoom of the Atom Lab, Nucleus to Nucleon to Quarks, was checked line by line against the research literature (every source is saved with the app and listed in About). "
     "The Accurate style no longer draws a colour charge on the quarks: a real proton or neutron is a colour singlet, so no quark has one definite colour. Its quarks are now coloured by "
     "flavour (up warm, down cool), and the gluon field and the sea pairs are colourless. The gluon field takes the shape lattice QCD finds: a round bubble when the quarks are close (the usual case), a "
     "filled triangle at about 0.4 fm, and the Y of three flux tubes only beyond 0.5 fm (it was always a Y). The texts give the CODATA 2022 proton radius 0.84075 fm, the neutron's "
     "negative mean square radius with the open disagreement between the measurement and the deuteron calculation, the magnetic moments, quark sizes below 4.3×10⁻¹⁹ m, the anti-down "
     "over anti-up excess of the sea, how little of the spin the quarks carry, and why the proton's mass split depends on the scheme. Nuclei: the protons' radius now takes off both "
     "nucleons' own sizes and the Darwin-Foldy term (Ong et al. 2010), the neutron skin follows the antiprotonic-atom fit (lead-208: 0.16 fm, below PREX-II's 0.28 ± 0.07; calcium-48: 0.12 fm, as CREX measured), and nucleons move at up to 0.27 c."},
    {"0.41", "Relativistic electron clouds for every element",
     "Electrons All now draws every neutral atom from helium to oganesson (118) with relativistic orbitals: Dirac-Hartree-Fock, computed for the app, with a finite nucleus. "
     "In heavy atoms this matters: gold's outer 6s electron sits 17 % closer to the nucleus than without relativity, which is part of why gold is yellow. Each orbital "
     "is split by j where the physics splits it (2p1/2 and 2p3/2 have different sizes), the open shells are in their ground level as NIST gives it (for the newest "
     "superheavy elements, the calculated one of Smits et al. 2023), and a new Model setting shows the same calculation without relativity, for comparison. Every "
     "orbital was checked against the published Dirac-Fock tables of Visscher and Dyall (1997): all but one of their 109 atoms agree to 0.002 %, and every element's ground-level J against NIST's (Smits et al.'s for the newest). Not "
     "included, and the app says so: electron correlation, the Breit interaction and QED. Elements 104 to 118 now have their electron clouds too. "
     "Also fixed: the ice fact says 8 % lighter than water (it said 9 %; the source's own densities give 8.3 %), Europa's card says there is strong evidence of an ocean, "
     "HCN's CN stretch shows the gas value 2096.85 cm⁻¹ (it showed the liquid's 2089), the many-electron readout lists only the atom's own shells, the Show One orbital caption no longer offers orbitals beyond 4f that the list cannot show, and the spectrum "
     "strip says why an element has no lines."},
    {"0.40", "A real 3D chart, true spheres, more electron dots",
     "The chart of nuclides has a 2D | 3D switch in place of the Tilt slider. In 3D, drag to turn the chart all the way round and to tip it, from straight down to just above "
     "its floor (never under it); Shift or the right button moves it. Space-filling molecules are true spheres: each point of a ball knows how near it is, so two balls meet "
     "along their real curve and no ball jumps in front of its neighbour as the molecule turns. With Electrons All the cloud keeps dots per electron (12,000, 24,000 or "
     "48,000, up to 60,000, 120,000 or 240,000), not 12,000 for every atom, and the light of its dots adds up precisely, so heavy atoms are no longer thin. The Atom Lab's "
     "Photons section has a spectrum strip: click a line and the electron makes that jump (with every electron shown it lists the neutral atom's measured NIST lines). "
     "Every Did-you-know fact names its source, and Show in About takes you to it; the nucleus fact now says 27,000 to 60,000 times smaller than the atom, not 100,000. "
     "Most realistic also sets Electrons to All. Fixed: with Real planet physics the Orbit Lab fell to 1 frame a second after about the year 3126 at the fastest time speeds."},
    {"0.39.3", "Night bug hunt, part 2",
     "Facts corrected: the oganesson step says a handful of atoms (three or four by 2006, one more chain in 2018); Galileo saw Saturn's \"ears\" 45 years before Titan, not fifty; "
     "Charon's orbit is drawn on Pluto's equator plane at 119.5° (NASA), not 122.5°; the periodic table says bismuth counts as practically stable; Halley's, Hale–Bopp's and "
     "Swift–Tuttle's notes give the real perihelion dates (9 Feb 1986, 1 Apr 1997, 12 Dec 1992 and 12 July 2126) beside the dates of the lab's simple fixed orbits; methanol's "
     "200 cm⁻¹ mode is a twist (torsion), not a bend; the Venus transit jump lands at its middle, 01:30 UT (NASA: 01:29:36), not 01:26; Uranus and Pluto are tipped past their sides, not upside down like Venus. Glitches fixed: the Journey caption and the far-out notes wrap in the free part of the window, clear of the panels, at every "
     "text size; the All values sheet keeps its full height below the Legend; the Facts panel no longer says a sourced fact has no source; Pick a body says when nothing matches; the search and Pick a body find Hale–Bopp and Swift–Tuttle when you type a space or a hyphen, Sagittarius A* by its name, and \"co2\" finds CO₂; "
     "a number keeps its unit and a key its words when a line wraps (\"0.79 Å\", \"12 Dec 1992\", \"R reset view\"); moon radii have thousands commas (2,575 km); √(5/3) "
     "is written out; 10⁻²³ s has real superscripts."},
    {"0.39.2", "A night bug hunt",
     "Facts corrected: Phobos falls to Mars or breaks up in about 50 million years (NASA; it said 100 million with no source); Dirac predicted antimatter in 1931, not 1928; ice floats "
     "because it is about 9 % lighter than water (the 4 °C maximum is why lakes freeze from the top); tungsten melts at 3,414 °C (CRC Handbook, as its card says); the Journey's Orion "
     "Nebula is 1,265 light years, as in the sky table; uranium-238's chance to decay in the next minute is 2.9×10⁻¹⁴ %, not 3.3×10⁻¹⁴ % (a rounding error in the sum); the Scale "
     "guide says the 1s orbit, not the whole atom, for elements past hydrogen; a moon card's big distance says it is the mean; the molecule chip says its mass is for the main isotopes. "
     "Glitches fixed: a short fact no longer drops its last word to a second line (in any theme); the scale bar keeps two figures for the light time (1.4 hours, not 1 hour); the Atom "
     "Lab's axes sit left of the Controls panel; the Molecule Lab's bottom bar fits at Extra large text; the chart's status line fits beside the panel; star names are never written "
     "on top of each other (Rigil Kentaurus and Toliman); a far comet's return year has no thousands comma."},
    {"0.39.1", "Every electron in every view, and a bug hunt",
     "With Electrons All, every view of the Atom Lab now shows the whole neutral atom (helium to lawrencium). The Slice view makes its dots on the cut plane with the whole atom's "
     "density there. The Orbit view, Modern and Bohr 1913, draws each shell as a ring at the average distance of its electrons, with its electrons on it (uranium: K 2, L 8, M 18, "
     "N 32, O 21, P 9, Q 2): a picture of the shells, not of real paths, as the line over the picture says. The Radial card shows the whole atom's distribution of distances, one "
     "peak per shell. The Learn guide and the 1925 Pauli step no longer say the lab draws only one electron; the Pauli button opens neon with all ten electrons. "
     "A search for bugs and glitches across the app also fixed: the whole-atom cloud now fills about thirty times faster; changing the element while zoomed in no longer throws the "
     "view far out; a complex orbital (m = +1 ...) shown alone gets its phase colours; the 90% outline is no longer drawn over the whole atom; the photon and Superposition rows, "
     "which did nothing with every electron, are replaced by notes; the readout, the view hint and the guide describe what is really shown; a long page title cut in a narrow "
     "window could split a character; a decay walk step could say \"about inf times slower\"; an unknown orbital name is no longer accepted."},
    {"0.39", "Every electron",
     "The Atom Lab can now show every electron of a neutral atom, from helium to lawrencium: in Cloud or Cloud 3D, set Electrons to All. Each orbital is a published Hartree-Fock "
     "orbital (Koga et al., 1999 and 2000), and the dots are exact, independent samples of the whole atom's electron density, coloured by shell (K, L, M, N ...). Because almost all "
     "electrons are close to the nucleus, the inner shells are spread out by default so each can be seen (Radius: True scale shows real distances). Show: One orbital draws one of "
     "the atom's filled orbitals, for example iron's 3d, with the sign of its wave. The line over the picture gives the configuration, the ground level and the first ionization "
     "energy from the NIST Atomic Spectra Database, marked as measured, semi-empirical or theoretical as NIST marks it. The model is said plainly: each electron in the average "
     "field of the others, one configuration, not relativistic. One electron round the bare nucleus is still the start, and still exact."},
    {"0.38.1", "Fixes to the new look",
     "Guide and About pages, the search list and the guides' contents are now solid: in High contrast, Light and Paper the Controls panel's text showed through a guide where the two "
     "overlapped. The picture now dims behind Settings as it does behind About and the guides, and a click on the dimmed picture closes Settings. The atomic number on the "
     "Atom Lab chip's element tile is now the size of the other small text (it was about 10 pixels, under the app's 12-pixel minimum). A font name too long for its Settings tile "
     "(\"Theme default\" in Paper) now goes onto two lines instead of touching the tile's edges. The Atom Lab's information plate, its \"i\" box and the chosen belt's name in the "
     "Orbit Lab follow the theme's corners (square in Monochrome; the belt name was still a round pill). The \"closest to reality\" mark on a chosen option sits a pixel further "
     "from the option's edge. Monochrome and High contrast are now named as the two outline themes instead of being recognised by their colours."},
    {"0.38", "A new look",
     "The whole app has a new, sharper look, in every theme. Corners are small and square-ish (no more round pills), panels are flat with a thin edge, and a chosen option "
     "is a soft tint of the theme's accent with a thin accent edge instead of a heavy grey block. Ticked boxes and the filled part of a slider are the accent colour, and a "
     "slider's thumb is a small upright bar. The labs are now tabs at the left of the top bar, and in the Atom Lab the chip in the middle shows the element's tile in its "
     "periodic-table colour. The Controls sections have their names in capitals with a thin rule, and the Settings headings lost their small emoji. The theme button is now a painter's palette. Inter is now the font of "
     "every theme with the theme's own font, except Paper (its book font) and Monochrome (its monospace). "
     "Also fixed: the + half of an orbital's wave took the theme's accent colour, so in Paper both halves were red-brown and in High contrast yellow against orange; it is now "
     "always blue, as the text says."},
    {"0.37.1", "A smooth Slice drag, and cleaner cards",
     "Dragging the Slice picture (which slides the cut) lagged badly in 0.37, because every frame of the drag threw all the dots away and made up to 6,000 new ones. "
     "Now the new plane's dots replace the oldest, at most 1,500 a frame: the picture never empties, and a moment after the drag stops every dot lies on the plane again. "
     "Also fixed: a faint box with square corners showed inside every card and panel, in every theme. It was the soft top-lit shading, drawn as a rectangle set in from the "
     "sides; it now follows each card's rounded outline."},
    {"0.37", "Honest electron clouds",
     "The Atom Lab's dots are now each drawn on their own from the exact probability, instead of one long chain of small steps. The chain was right on average, but "
     "neighbouring dots were close together, so the 12,000 dots on screen were worth only a few hundred: one lobe of a p or d orbital often held 60 % of the dots and the "
     "other 40 %. Now the two lobes match to within about 1 %, as they should, and the sampled average radius is within 1 % (it was within 10 %). "
     "The Slice view is now a true cut: its dots lie on the cut plane itself, spread like the probability on that plane. Before, it showed a thick slab of the 3D cloud "
     "that filled in the inner nodes of 3s and 4s; now every node is empty. Moving the cut makes the dots again, in a frame or two."},
    {"0.36", "Every dwarf planet in Real",
     "In Real, Ceres, Haumea, Makemake and Eris now move too: the Sun, the planets and Pluto pull on them from JPL's 2026 start, and they are too light to pull back. "
     "Over 1800-2100 they stay within 78 to 163 km of JPL Horizons with relativity on (on their fixed orbits they were 88 to 175 million km off), except Ceres, "
     "9,324 km, because the other big asteroids that pull on it are not in the model. Each card quotes its own error. The planets and Pluto move exactly as before. "
     "Real does about twice the work it did, still done ahead in the background. Also fixed: between whole days Real drew the planets on a curve that put Mercury up to "
     "20 km off its computed path (its speed 0.7 m/s off); the curve now also uses the pulls, and between days Mercury is as close to JPL as on them (0.02 km)."},
    {"0.35", "A faster Moon, a closer Earth",
     "The Moon's position (the ELP2000-82B series) is now worked out 5 to 9 times faster, with the same result to a millimetre. That made room for Real to use "
     "every term of it when it pulls the Earth and the Moon apart, instead of only the 100 largest: Earth is now within 14 km of JPL over 1800-2100 with "
     "relativity on (it was 75 km), and Real costs a little less work than before, not more. The Real note's numbers are re-measured."},
    {"0.34", "The real ladder of a bond",
     "For the four molecules with two atoms (hydrogen, nitrogen, oxygen, hydrogen chloride), a vibration now shows its first three steps up the measured "
     "ladder of levels: for hydrogen 4,161, 3,926 and 3,695 cm⁻¹. Each step is smaller than the one below, because a real bond softens as it stretches, until "
     "it breaks; the constants are Huber and Herzberg's, as the NIST WebBook gives them. The swing drawn is still the simple harmonic one, and the panel says so. "
     "Also fixed: the infrared spectrum's heading was cut off at the panel's edge, and for a molecule with no infrared band it said the tallest stick was 0 km/mol."},
    {"0.33", "Hyperfine structure, relativistic",
     "The hyperfine splitting of the ground level now has Breit's relativistic factor: tiny for hydrogen (0.008 %), but it doubles the splitting of heavy "
     "one-electron ions (2.1 times for bismuth). Hydrogen-like holmium, measured at 2.165 eV, was 30 % too low and is now 7 % too high; what is still left out "
     "(mostly the nucleus's size) makes up most of the rest, by a 1997 calculation, and the tab says so. Every number is checked against Eides et al. 2001 and "
     "Shabaev et al. 1997. 0.31's notes gave a wrong form of this factor; that is corrected."},
    {"0.32", "Pluto joins the real planets",
     "In Planet physics: Real, Pluto (with its moons, as one body) now pulls on the Sun and planets and is pulled by them, starting from JPL's 2026 position "
     "like the planets. Before, it rode a fixed orbit, up to 160 million km off JPL; now its worst error over 1800-2100 is 1,908 km. Its small pull "
     "also brings Uranus and Neptune closer to JPL (with relativity on, Uranus from 515 to 134 km at worst, Neptune from 2,582 to 1,436 km). The big asteroids "
     "were tried and left out: only a few of them pull the Sun unevenly and made Earth and Uranus worse. In Simple, Pluto keeps its fixed orbit."},
    {"0.31", "Hyperfine structure",
     "The Atom Lab's Spectrum tab now says how the nucleus's own magnetism splits the ground level: for hydrogen, into F = 0 and F = 1, 5.9 millionths of an "
     "electronvolt apart, which is the famous 21 cm radio line (the lab gives 1,420.5 MHz; measured 1,420.4 MHz). Every nucleus's spin and magnetic moment comes "
     "from the IAEA's table of recommended values (914 nuclei); a nucleus with even numbers of protons and neutrons has spin 0 and no splitting, and one the "
     "table does not have says so. The formula is the textbook one (magnetic dipole, non-relativistic, point nucleus), so it is close for light nuclei and "
     "less so for heavy ones; the tab says this."},
    {"0.30", "Molecules, live and warm",
     "The Molecule Lab's third stage: in Vibrations, \"All at once, warm\" moves every vibration of the molecule together, each at its true speed compared with "
     "the others and as big as it is at the temperature you choose (0 to 3,000 K; buttons for 0 K, room temperature and 1,500 K). Each vibration is a quantum "
     "oscillator: at 0 K it keeps its zero-point motion, and as the molecule warms the slow vibrations take up energy first (Bose-Einstein occupation). The "
     "panel gives the vibration energy above the lowest level, how many quanta the slowest vibration holds, and the biggest swing. What is simplified is said "
     "next to it: each vibration keeps one size, the springs are perfect, the molecule does not turn, and real atoms are smeared out, not points."},
    {"0.29", "Real planets, more accurate",
     "Planet physics: Real now moves the Sun and the eight planets with a much more accurate method (Gragg-Bulirsch-Stoer extrapolation, Hairer, N\u00f8rsett "
     "and Wanner 1993) and keeps Earth and the Moon apart, so the Moon's pull on Earth is in the sums. General relativity now uses the standard first-order "
     "(1PN) Schwarzschild field of the Sun from the IERS Conventions 2010, and it is on by default: with it, Mercury stays within 24 km of JPL Horizons over "
     "1800-2100 (0.28: 29,231 km with relativity off, 70,045 km on) and Venus within 5 km (0.28: 4,762 off). The old method's own error at its 1-day step was larger than the effect of relativity, which "
     "is why turning relativity on used to make the planets worse. Saturn is the one planet that is closer with relativity off (150 km against 336 km): the "
     "model leaves out Pluto, the asteroids and the Kuiper belt, and their missing pull happens to cancel part of the error when relativity is off. The new "
     "method costs more, so while time runs the next stretch of days is worked out ahead on a second thread. A relativity setting saved by an earlier "
     "version is not kept: it was nearly always the old default."},
    {"0.28", "Checked element and nucleus data",
     "The melting points, boiling points and densities of the elements now come from one named source, the CRC Handbook of Chemistry and Physics (97th "
     "edition, through the mendeleev database); before, they had none, and many were rounded to the nearest degree. The metals that define the ITS-90 "
     "temperature scale (gallium, tin, zinc, aluminium, silver, gold, copper) now melt at its fixed points (to 0.01 K). The melting points of astatine, francium, "
     "fermium, mendelevium, nobelium and lawrencium are called predictions, because nobody has measured them; boiling points the CRC table does not give "
     "now say so instead of showing an unsourced number. Two errors in the nucleus shapes were fixed: argon-36's measured B(E2) was a third of its real "
     "value, and argon-36 was drawn stretched though its measured quadrupole moment says flattened. All 11 B(E2) values now come from ENSDF. Zinc-54 "
     "throws out two protons in 87 % of its decays (NUBASE2020), not 100 %. Unused code for the old glass themes was removed: no look changes."},
    {"0.27", "Real shapes, real moons, real asteroids",
     "The planets are no longer perfect balls: each is drawn with its real equatorial and polar radius from NASA's fact sheets, so Saturn is 9.8 % flatter at the "
     "poles and Jupiter 6.5 %, round its real spin axis, with the map, the lighting and the rings' shadows following the real shape. Planet cards give the polar "
     "radius; Haumea's card gives its egg shape (two measurements that disagree). The 13 big moons other than ours (Phobos, Deimos, Io, Europa, Ganymede, Callisto, "
     "Enceladus, Rhea, Titan, Titania, Oberon, Triton, Charon) are now at their real places from 1800 to 2100: each follows an orbit fitted to JPL Horizons, and its "
     "card says how close (15 km for Charon to 17,700 km for Europa); before, they started at made-up places. The asteroid belt, Jupiter's Trojans and the Kuiper "
     "belt are now 3,000 real asteroids, the largest known of each kind from JPL's Small-Body Database, each on the orbit of its record: the Kirkwood gap at 2.5 AU "
     "and the two Trojan camps 60 degrees ahead of and behind Jupiter come from the data itself. The Oort cloud's dots are still made up (nobody has seen it)."},
    {"0.26 stage 3", "An accuracy audit of all three labs",
     "Every number the app shows was checked again against its source, with the sources opened this time. Fixed: platinum-182 decays by alpha 0.038 % of the time, "
     "not 3.8 % (NUBASE2020's own line has a slip); ammonia's rotational constants now come from JPL's spectroscopy catalogue (9.947 cm⁻¹; CCCBDB lists a slip, 9.444); "
     "Neptune's Lassell ring now runs from 53,200 to 57,200 km (it was drawn 2,000 km too far in); Mercury's radius is its equatorial 2,440.5 km; Phobos, Deimos and Charon "
     "follow NASA's fact sheets; Haumea, Makemake and Eris get JPL's sizes and masses; Proxima Centauri is 4.25 light years away (Gaia DR3), Alpha Centauri 4.39, the "
     "Orion Nebula 1,265; the Voyagers move at their real speed away from the Sun (JPL Horizons). Said more clearly: the moons other than ours start at made-up places "
     "(their cards now say so), and the colours of spectral lines are approximate. The guides' facts were checked too: Neptune leans 28.3°, the 2026 June solstice and September equinox are a minute earlier (US Naval Observatory), Parker Solar Probe passed 6.1 million km from the Sun, the down quark is 4.70 MeV (PDG 2024), and Shapley's 1918 distance to the galaxy's centre was too large, not close. Checked and right: the planets' orbits, the 2,874 stars, the CODATA constants, the "
     "nuclear tables, the 7,917 spectral lines, the molecules' shapes and vibrations. Still open (roadmap): the elements' melting and boiling points need a named source."},
    {"0.26 stage 2", "Rings for all four giant planets, and Saturn's rings in real sunlight",
     "Jupiter, Uranus and Neptune now have their rings, from NASA's ring fact sheets, drawn as thin and dark as they really are: Uranus's narrow rings "
     "show as fine lines across the planet, Jupiter's and Neptune's are too faint to see in ordinary light. Saturn's rings now shine by the real "
     "Sun: bright from the sunlit side, dark from the other (where the thin C ring and the Cassini Division glow), almost dark at equinox (May 2025); "
     "Saturn's shadow falls on its rings and the rings' shadow on Saturn. Found: NASA's Uranus ring sheet gives three radius ratios that disagree with "
     "its own kilometres (eta, delta, epsilon); the kilometres are used."},
    {"0.26", "Orbit Lab look, part 2: real planet maps, the Gaia sky and Saturn's measured rings",
     "Mercury, Earth, Mars, Jupiter and Pluto now wear real maps from spacecraft pictures, turned the way each really is on the date shown: Earth from NASA's Blue Marble "
     "(January 2004, no clouds), Mercury from MESSENGER and Pluto from New Horizons (black and white; Pluto's south, in darkness in 2015, is plain grey), Mars from Viking "
     "and Jupiter from Cassini (December 2000; its clouds have moved since). Our Moon has its LRO map and keeps its near side towards Earth. Saturn, Uranus and "
     "Neptune have Hubble's OPAL maps of 2025, made into true colour from the light measured through each filter (checked against the planets' measured colours). "
     "Venus, whose clouds have no markings, is a smooth ball in its measured colour. No planet has the grid any more. Saturn's rings have NASA's measured edges, the Cassini Division and the "
     "F ring, and each lets through as much light as its measured optical depth allows. Behind everything, the smooth drawn Milky Way is replaced by ESA's Gaia map of "
     "the whole sky (1.8 billion stars, dust lanes, the Magellanic Clouds); it turns with the 3D view exactly as the stars do. Every picture's source is in About."},
    {"0.25 stage 2", "A guide for the Molecule Lab, and a bug hunt",
     "The Molecule Lab has its own \"What am I even looking at?\" guide (Learn, or search): what the picture shows in each style, what the colours mean, the vibrations, "
     "and what is measured, calculated or only drawn; it follows the molecule and style on screen. Fixed: with a small window, opening the Atom Lab's card squeezed the info "
     "text into a thin column that ran under the toolbar (now it goes below the card when there is room, and folds above the scale bar when not); long tooltips were one "
     "line that could run off the window (they wrap now); search offered \"Drag moves the view\" in 2D, where it does nothing, and \"Follow\" with nothing selected (now "
     "only where they work, and each says turn on or off); closing a guide opened from Learn did not take you back when another lab's guide was still open; an open guide "
     "in the Molecule Lab did not count as an open page. Two facts: carbon-14's half-life now says about 5,700 years (NUBASE2020), as the Atom Lab shows (it said 5,730), "
     "and Venus's 243 days is how long it takes to spin once; its sunrise-to-sunrise day is about 117 Earth days. A dwarf planet's estimated temperature now says it is an "
     "estimate (it said \"no atmosphere\", which read as if Pluto had none). The guides named controls of the old layout: the Orbit guide's Follow and ✥ Drag "
     "buttons are now the ticks in Controls › View options, and the Atom guide's Nucleus and Nucleon buttons are Controls › View › Zoom to."},
    {"0.25", "Closest to reality: a dot on the most realistic choice, and a Most realistic button",
     "Every setting where one choice is clearly closer to the real thing now has a small dot on that choice (a ring next to a box best left unticked); point at it to see why. "
     "A Most realistic button at the top of each lab's Controls (and in search) picks all of them at once: in the Orbit Lab 3D, true scale, Real physics, all the real bodies, the "
     "full sky and no constellation lines; in the Atom Lab the 3D cloud, fixed scale, point quarks with the sea, the measured nucleus density, the smooth cloud, every phase and "
     "the Dirac levels; in the Molecule Lab the computed density, the most dots, the 0.0016 surface and vibrations at their real size. General relativity is left off on purpose: "
     "the app's simple version of it moves the planets further from NASA/JPL's positions. Settings where no choice is more real (trails, labels, spin, outlines, colours) have "
     "no dot. The defaults are unchanged."},
    {"0.24", "The cloud moves with its atoms; the builder is gone; the whole Solar System at the start",
     "Molecule Lab: in the Density style water's three vibrations now move its electron cloud and surface, not only its atoms: at each moment the picture is the calculated "
     "density for the atoms where they are (the electrons follow the nuclei at once). It is calculated at both ends of the swing with the same method as the still density "
     "(B3LYP / aug-cc-pVTZ) and blended smoothly between; the cloud keeps its 10 electrons at every moment, and the dipole change across each swing matches the infrared "
     "intensity of the separate vibration calculation. The other molecules follow if this looks right. The builder (Build mode) is removed: the Molecule Lab is the 15 "
     "measured molecules. The Density style's zoom fits the electron surface itself. Orbit Lab: every start now shows the whole Solar System, Neptune's orbit filling the free "
     "part of the window (before, a fixed zoom left Neptune off the picture, and a zoom from the last run could open it far out)."},
    {"0.23", "Orbit Lab look, part 1: orbits, planets and calmer labels",
     "Every planet and dwarf planet now shows its whole orbit as a faint line in its own colour (Controls › Show › Orbit paths; on by default): in Simple physics it is the "
     "NASA/JPL Kepler ellipse itself, in Real the orbit it is on at that moment. A planet smaller than a pixel at true scale is a dot in its colour with a soft glow, not a hollow ring; "
     "so is a small moon (Earth's Moon showed only its name). Labels keep a clear order when they crowd (the planets first, then dwarf planets, comets and moons; a belt's name no "
     "longer hides a planet's), and a label near an edge or the Controls panel slides in instead of being cut off (\"ptune\"). The selection ring is thinner. Zoomed in on Earth, the "
     "habitable-zone band no longer tints the whole picture green: it fades out once it is far wider than the picture. Molecule Lab: benzene's vibrations are in too "
     "(all 20 within 3.2 % of the measured bands)."},
    {"0.23", "New: molecules that vibrate, with their infrared spectrum",
     "Molecule Lab, Controls › Vibrations: pick one of a molecule's vibrations and watch its atoms swing (Ball-and-stick and Space-filling). Each mode is listed by its wavenumber "
     "and what it does (\"O–H stretch, in turn\", \"bend\"), with a small infrared spectrum: a stick for every band, as tall as its absorption, and a circle for a mode that "
     "absorbs no infrared because its swing does not change the molecule's dipole (CO₂'s symmetric stretch, N₂ and O₂). The swing is slowed down 6 × 10¹³ times, every mode at its "
     "true speed relative to the others, and is as big as the spread of the lowest vibrational level (Real size) or three times that. The wavenumbers, the symmetry and (where listed) "
     "the infrared intensities are MEASURED (NIST CCCBDB); which way each atom moves is CALCULATED (B3LYP / aug-cc-pVTZ with PySCF at the measured shapes, harmonic), and every "
     "calculated wavenumber, times CCCBDB's factor for the method, lands within three times its stated spread of the measured one, except two motions a harmonic calculation cannot fit, "
     "which the panel names: methanol's twist (on CCCBDB's list of big anharmonicities) and ammonia's umbrella (measured as two levels: the nitrogen tunnels through). The panel says that a "
     "real molecule does not swing like a ball on a spring: in its lowest level the atoms are smeared out and every mode is there at once. Also: the Molecule Lab's auto-spin no longer "
     "stops when another lab is paused (this lab has no clock of its own), and search finds the vibrations."},
    {"0.22", "Fixes from a bug hunt through every option",
     "Every option of the three labs was clicked in the real app and the picture compared before and after. Orbit Lab: the scale bar's label is its real length. It was a round number of "
     "kilometres or miles written in rounded AU, so a bar of 3.34 AU said \"3 AU\" and one of 1.34 AU said \"1 AU\" (a quarter short; the light time under it was right for the bar, not for the label). "
     "Enter in the date box now jumps, as Go does. \"Full orbit trails\" says that a trail grows as time runs (a new picture has no whole orbit yet). Turning general relativity or "
     "the Sun's wobble on from search also turns Real planet physics on, where they work. Atom Lab: \"Check a jump\" with the same shell twice asks for two shells, and two shells of one n "
     "no longer print the same note twice. \"Put this lab's settings back\" says it goes back to the Cloud 3D view, which it does (it said the Orbit view). On the chart the labels keep clear "
     "of the element card (\"heaviest known\" ran under it). Molecule Lab: the fact line is shown (it was always hidden under the style bar), and Space no longer plays or pauses the other "
     "labs unseen (this lab has no clock). Search: it now finds the Atom Lab's 118 elements, famous isotopes, pictures and guides, and the Molecule Lab's molecules, styles and modes, and "
     "offers every other lab (before: only Orbit and Atom, and \"Go to the Orbit Lab\" in the Molecule Lab)."},
    {"0.22", "Fixes from a second look-through and a review of the whole app",
     "Atom Lab: with Reduced motion the lab starts paused, and the cloud was empty until Play: a paused cloud now fills once. The picture is sized to the free part of the window, so the "
     "orbit no longer runs under the top bar and the card. The view buttons are two clear rows, Picture (Orbit, Cloud, Cloud 3D, Slice, Chart; the toolbar's Atom button is now called "
     "Orbit, which is what it shows) and Zoom to (Atom, Nucleus, Nucleon). The info text is four short lines; the rest is under More info. For every element above hydrogen it says that the "
     "picture is the one-electron ion (for example Fe²⁵⁺) while the card is for the ordinary element. The relativity warning for heavy atoms is always shown. The card reads \"atomic "
     "number · 26 protons\" and folds itself on a narrow window and on the chart. The chart fits between the Legend, the Controls panel and its tilt bar, and the cloud's outline and "
     "colour choices are gone from it (it has no cloud). The nucleus glow is smooth at every size. A wrapped info line no longer starts with a stray space. "
     "Orbit Lab: no label is drawn under a bar or a box; the pile of names round the Sun is one hint, \"inner planets: zoom in\"; the wide moon orbits of Saturn and Uranus wait until you "
     "zoom in; the readout draws a real scale bar (it said \"Scale 3 AU\" with no bar) and no longer repeats the zoom; planet cards say equatorial radius. The Sun's mass is "
     "332,946 Earths (IAU 2009), was 333,000, and Earth's 5.9722 × 10²⁴ kg (NASA). Molecule Lab: smaller balls so the bonds show, a bigger picture, element-only labels (H, O), the "
     "chosen molecule in grey like every other choice, Reset view only on the bar. Everywhere: the fps counter is off (Settings › Performance › Show fps), the fact line sits on a "
     "plate, the top bar lost the two-lab swap button and does not repeat the subject's name, Settings has the same header as About, notes open with \"Show notes\", Learn lists the "
     "labs in order, and Achievements' hint names the guide."},
    {"0.22", "New: the nucleus as its measured charge density, and honest names for what the pictures show",
     "Atom Lab: the whole nucleus is now drawn as its charge density by default (Controls › Nucleus › Nucleus look: Density): a smooth glow, flat in the middle with a soft edge about "
     "2.4 fm thick, as electron scattering measures it, at the measured rms charge radius (IAEA) and in the lab's measured shape (uranium-238 is stretched like a rugby ball). It is the "
     "form fitted to electron-scattering data (a two-parameter Fermi, or Woods–Saxon, distribution). Zoom in further and the nucleons appear as before, as one possible snapshot; the "
     "picture text now says that real nucleons overlap, are spread through the nucleus as waves and move at about a quarter of light speed. Fuzzy and Hard balls are still there. "
     "Photons outside the visible range (UV, infrared) are drawn grey and labelled \"invisible to the eye\" (they were violet and red, colours the eye cannot see there). "
     "The cloud style Glow is now called Density glow, and both cloud styles say that each dot is one place the electron may be, not an electron. Molecule Lab: the Realistic style is now called Density, \"Electron density (computed)\" in its section, as it is a calculation and not a photo; the panel "
     "says that the molecule is shown standing still at its equilibrium shape while a real one vibrates all the time (water's O–H stretch every 9 femtoseconds), and that its "
     "colours and ball sizes are conventions. The quark view already showed the Y-shaped gluon field of lattice QCD, the colour charge as a label and the proton's mass budget. "
     "Fixed: the Fuzzy and Hard balls buttons of Nucleus look showed their whole note, too long to read, and the 2p orientation button said \"pz · along z\" instead of \"pz\". About now says the lab shows real orbits, atoms and molecules (it left out molecules). What's new lists the app's own history only, and the separate Orbit and Atom Lab changelog windows are gone."},
    {"0.22", "Fixes from a look through the app: easier to use in all three labs",
     "Molecule Lab: the bottom bar now has all three styles, Realistic too (in the Viewer), and Reset view; Element labels and Auto-spin are in the Controls panel only, so the bar is a shortcut "
     "and not a copy of the panel. Formulas are written with small numbers below the line: H₂O, CO₂, C₆H₆. "
     "Orbit Lab: the Sun sits in the middle of the part of the window the Controls panel leaves free (before, it sat at the panel's edge); it slides back to the window's middle when the "
     "panel is closed. The lab starts closer in (zoom ×0.16 instead of ×0.06), so Earth and Mars are no longer hidden in the Sun's glow and Saturn and Uranus are still in view, and slower "
     "(1 s = 7 days instead of 29 days). The Sun's label is just \"Sun\" (its mass is on its card), so it no longer pushes Saturn's label away. Ring labels such as \"20 AU\" are no longer drawn "
     "on the x y z axes at the bottom right. "
     "Atom Lab: the five view buttons at the bottom say what they are (Atom, Cloud, Cloud 3D, Slice, Chart) next to their icons, and the isotope card is narrower (17 em instead of 20), so "
     "the picture has more room. For 1s the picture's text now says one colour: the 1s wave has no node, so it has the same sign everywhere (it said \"two colours\" before, as for every "
     "other orbital). "
     "Everywhere: the Controls panel has one way to close it, the handle on its left edge (the ✕ is gone). New keys: 1, 2 and 3 open the Orbit, Atom and Molecule Lab, + and − zoom, R "
     "(or Home) resets the view; the Controls panel lists them. The fact line at the bottom shows only the facts that belong to the lab you are in (and the general ones); a fact found with "
     "search is still shown anywhere."},
    {"0.21", "New: the Molecule Lab's Realistic style: the electron cloud and a charge-coloured surface",
     "A third style next to Ball-and-stick and Space-filling, in the Viewer: Realistic shows the CALCULATED electron density of each of the 15 molecules at its measured shape. "
     "The cloud is made of dots drawn at random from the density (20,000 to 100,000, 60,000 by default), drawn like the Atom Lab's Cloud 3D; the dots show where the electrons are likely "
     "to be, not electrons as little balls. Over it lies a see-through surface where the density falls to a chosen value (0.001, 0.0016, 0.002 or 0.01 electrons per cubic bohr; 0.002 is "
     "the usual convention): a molecule has no hard edge, so the surface is a chosen cut-off. The surface can be coloured by the molecule's own electrostatic potential, red where it is "
     "negative and blue where it is positive, with a colour bar in atomic units and kJ/mol per elementary charge. The numbers come from a quantum chemistry calculation (density "
     "functional theory, B3LYP with the aug-cc-pVTZ basis, made with PySCF) and are checked against measurements: the computed dipole moments agree with NIST CCCBDB's measured ones "
     "within 0.13 debye (ozone, a hard case, differs most), the stored grids hold the right number of electrons within 0.3 %, and the symmetric molecules come out symmetric. "
     "Ozone is calculated in a simpler, closed-shell approximation and says so. Real pictures of molecules by atomic force microscopy show forces, not this density. Bonds are not "
     "drawn in this style; clicking a nucleus still shows the atom's details. Not measured: speed and look on a real graphics card and Windows."},
    {"0.20", "New: the Molecule Lab, part 2: build your own molecule",
     "The Molecule Lab has a Build mode next to the Viewer. In the Sketch you click to add H, C, N, O or Cl, drag an atom next to another one with an open valence and let go "
     "to bond them, click a bond to make it single, double or triple, and undo, redo, delete or clear. Valences are strict (H 1, C 4, N 3, O 2, Cl 1); a bond that does not fit, or "
     "a bond type with no measured length in our sources, is refused and the reason is shown. The panel shows the Hill formula, the mass with its uncertainty, how many valences are "
     "still open, and a name when the molecule is one of 23 we know (the 15 of the Viewer, which then also offer their measured shape, and eight more). A complete molecule gets a 3D "
     "shape, and it is an ESTIMATE, labelled so everywhere: ideal VSEPR angles (180, 120 and 109.47 degrees) and one CCCBDB bond length per bond type, relaxed by a simple spring "
     "model so rings close. Real angles are smaller where lone pairs sit (water: 104.48 measured, 109.47 here), and a built benzene has alternating bonds where the real one has six "
     "equal bonds. Also: the Atom Lab's info text folds in the Nucleus view instead of covering the nucleus (a click shows the rest); the Orbit Lab's All values sheet no longer "
     "covers the Legend; the Molecule Lab's picture can no longer be dragged off screen. Not measured: how dragging feels with a real mouse on Windows."},
    {"0.19", "Defense Lab removed, clean-up and audit",
     "The Defense Lab (the factory game of 0.17) is removed: the app has three labs again, Orbit Lab, Atom Lab and Molecule Lab. A settings file from 0.18 still loads: if it was left on the Defense Lab, "
     "the app opens the Orbit Lab, and every other setting is kept (the Defense Lab's save file is left untouched on the disk). Fixed after a full check of the app: the clickable word \"monochrome\" in the "
     "Atom Lab guide (What am I even looking at?, \"What is to scale\") did nothing since 0.16, so the Monochrome theme and the Sharp Eyes achievement could not be found; it works again. A year typed far "
     "in the future in Jump to a date could overflow and was then called a date before 1800. A long search result or fact could be cut in the middle of a character. Numbers that are not numbers "
     "(NaN, Infinity) or are very large are written in the standard way (NaN, Infinity, 1e+300) instead of garbage. In the Molecule Lab, search no longer offers a changelog, Defaults and a Legend it does not have, and its "
     "bottom bar lights its buttons like every other toolbar (not in the accent colour). The settings file is written to a side file first and only then replaces the old one, so a full disk or a crash "
     "can no longer leave a cut file. Unused code left over from the old interface was deleted. Ammonia's listed rotational constants were checked on CCCBDB: CCCBDB itself lists 9.4443 next to a "
     "geometry that gives 9.97, so the value stays marked as doubtful. Not measured: speed on a real graphics card, and how the mouse feels on Windows."},
    {"0.18", "New: the Molecule Lab, part 1: a viewer",
     "A new lab, third in the lab list: 15 small molecules (H2, N2, O2, HCl, CO2, H2O, O3, NH3, CH4, HCN, C2H2, H2CO, C2H4, CH3OH, C6H6) in 3D, in their MEASURED shape: every bond length and angle "
     "is NIST CCCBDB's experimental value with its reference code, and the 3D shape is built from them (where the shape needs a symmetry, the panel says which). Methanol's H-C-O angle is not in CCCBDB: "
     "it is derived from H-C-H and marked so. Ball-and-stick or space-filling (van der Waals radii), element labels, drag to turn, the wheel zooms, Shift or right drag moves, Reset view, and an optional slow spin "
     "(off when Motion is Reduced). Click an atom or a bond: the measured value with its source next to the value worked out from the 3D shape. The data sheet has the mass of the most abundant isotopes "
     "(NIST) and the measured rotational constants next to the ones the shape gives. The shape is a fixed average (an equilibrium structure); real molecules vibrate. Colours, ball sizes and bond orders are "
     "drawing conventions, and the app says so. Not yet: building molecules, vibration, reactions, orbitals. Not measured: speed on a real graphics card, and how the mouse feels on Windows."},
    {"0.17", "Defense Lab, part 2: power from the core, a new look, a fixed map",
     "Power no longer dead-ends: the core has a free emergency generator, enough for about 2 miners, that reaches the network through a pole next to it, and a new planet's core starts with 50 coal; "
     "click a coal generator to fill it from the core (hold to keep filling). A new look drawn in code: one invented red rocky planet (Mars-like, not real Mars) with dust, pits, cracks and stones; "
     "chunky outlined machines, each with its own colour: a yellow miner with a spinning drill, a round copper furnace that glows while it works, a red-orange generator that pulses while it burns, a chest with a lid and latch, "
     "poles with glowing nodes, and the core as a landmark with a pulsing ring light; belts with moving chevrons. Far away, an arrow at the edge points to the core with the distance; H or Home jumps back. "
     "The map is now fixed: 512 x 512 tiles with a dark border band; ore never runs out, and patches are bigger the farther out; no more water or sand. "
     "Building: drag to lay a line of belts (corners made for you), right drag removes, a green or red ghost shows where a building can go, WASD or the arrow keys move the map, keys 1-8, Q picks, Esc puts down. "
     "A planet saved by part 1 cannot be loaded (a copy is kept as .old): start a new planet. Not measured: speed on a real graphics card, and how the mouse and keys feel on Windows."},
    {"0.17", "New: the Defense Lab, part 1: the factory",
     "A third lab, next to the Orbit Lab and the Atom Lab: a factory game on an endless planet, seen from above, with soft shadows for depth. Each planet has a seed (shown, and you can type one); "
     "the same seed always makes the same map: deep and shallow water, sand, grass, rock, and patches of iron, copper, coal and stone, richer and bigger the farther you go. The start always has all four ores close by. "
     "Build miners, belts (corners form by themselves), furnaces, chests, coal generators and power poles; plates belted into the core are counted. Drag to move, the wheel zooms, left click places, R turns, right click removes. "
     "Pollution is kept for every chunk of the map and spreads slowly (an overlay shows it); it does nothing yet. The game saves every 60 s, with the Save button and when the app closes. "
     "All its speeds, costs, sizes and power numbers are game design choices, not measured values (About says so); only the element names and symbols come from real data. Aliens and defences come in a later stage. "
     "Not measured: speed on a real graphics card, and how it feels on Windows with a real mouse."},
    {"0.16", "Atom Lab: the Accurate quark style is closer to what is measured",
     "Only the Accurate quark style changes its look; Classic and Cinematic look as before, except that Classic's sea now has more down than up pairs. "
     "In Accurate each valence quark wanders on its own round the middle instead of riding a tumbling triangle; down quarks keep a little farther out than up quarks (a heuristic from Miller 2007), "
     "and none goes past 0.6 fm. The gluon flux tubes are as wide as lattice QCD finds (radius 0.38 fm, Bissey et al. 2007, computed for heavy static quarks) and the gluon halo is wider "
     "(rms 0.65 fm, a choice between published values that disagree). The Learn card on the proton's weight now gives the lattice-QCD split (Yang et al. 2018), the quark note says how much "
     "bigger the dots are than a quark can be, and the Fuzzy edge text says the neutron uses the proton's profile only as a size. Sources: docs/QUARKS.md and About. Not measured: speed on a real graphics card."},
    {"0.16", "Atom Lab: the nucleus with sea quarks draws in far fewer steps",
     "With \"+ Sea\" each antiquark ring used to be drawn as 24 or more separate line pieces and broke the graphics card's batch; the Classic quark style's flux tubes did the same. "
     "Both are now drawn in the same batch as the dots round them: darmstadtium-281 with \"+ Sea\" in Accurate, turning, went from 449 draw calls and 33,323 corners a frame to 5 and 30. "
     "This was also why light themes were slower than dark ones with Cinematic quarks: on a light theme the rings took the slow way. The picture is the same (only the edge pixels of the rings differ slightly). "
     "Not measured: speed on a real graphics card (the cloud has none)."},
    {"0.16", "Benchmark: 11 scenes instead of 34, about 2 minutes",
     "The Benchmark now runs 11 scenes of 10 seconds each (about 2 minutes instead of about 6): the heaviest Orbit Lab view, 8 times the rocks, the oganesson-294 nucleus, the nucleus at full zoom with the camera moving, one nucleon, "
     "phase colours with 48,000 dots, a cloud jump in flight, a superposition that beats, an outline's first use, the outline with the cut dragged, and the chart with the decay chain. "
     "The scenes that only repeated another one were removed. The two old boxes (stress scenes, speed-risk scenes) are replaced by one: \"Compare old CPU paths\" adds 7 scenes (12 to 18) that run scenes of the list again with the processor doing the work, the way older versions did. "
     "Every scene number in the texts and in the report is the new one."},
    {"0.16", "Atom Lab: less work at full zoom on the nucleus; benchmark counts slow frames",
     "At full zoom on a nucleus (x10,000,000) the Atom Lab no longer makes the parts of nearby nucleons that are wholly outside the picture: their gluon cloud, sea quarks, rings and lines. "
     "The picture is the same pixel for pixel. With \"+ Sea\" the antiquark rings alone were up to 311,076 corners a frame in Accurate, Classic and every light theme; now 36. "
     "The benchmark has a new last scene, \"Nucleus, full zoom, camera moving\" (lawrencium-266, Cinematic, + Sea, the camera turning), and every scene now says how many frames took longer than 16.7 ms, "
     "so a one-time hitch can be told from a repeating one. The outline's graphics-card programs now run once at start, so a driver's first-use work does not fall on the first outline. "
     "Not measured: speed (the cloud has no graphics card)."},
    {"0.16", "Four fixes: About scrollbar, Benchmark window, Contents rail, Atom Lab Nucleus",
     "The About page keeps its scrollbar after you close and reopen it (it was drawn behind the reopened page). The Benchmark window with both extra-scene boxes ticked never grows taller than the app window: its text scrolls and Start and Cancel stay in view. The Contents list beside a page is wider, so labels like \"Stars and far-away things\" are shown in full. In the Atom Lab, Controls now always has a Nucleus section with Quark style, Nucleon detail and Nucleon edge, at any zoom; they show in the picture when zoomed in close."},
    {"0.16", "The old interface is gone: the New UI is the only one",
     "The old side panels, the old header and the Atom Lab's Old panel button were removed, and so was Settings -> New UI (preview): the new layout is always on. Nothing it showed was lost: every old control, card and note has a home in the new layout. A settings file that still holds the old New UI choice opens without a problem (the choice is ignored). The benchmark's result no longer has a New UI line. The old frames-per-second counter is gone; the fps is in the readout at the bottom right and in Settings -> Performance. No new facts or numbers; no physics changed. Not tested: Windows, a real mouse, a real GPU, speed."},
    {"0.15", "Full UI rework, stage 1a of the 0.16 clean-up: the Atom Lab's last old-panel features",
     "With New UI On, the Atom Lab's Controls panel now has the Dirac levels switch (Relativity, with the same words as before) and a Restart atom button (a fresh atom, the same settings). The data sheet's Spectrum tab shows the lab's own light spectrum, with the Dirac level rows when Dirac is On, below the NIST spectrum. Settings, Performance shows the line \"Picture: N draw calls, M instanced shapes\" in the Atom Lab. Nothing was removed."},
    {"0.15", "Full UI rework, stage 5t: grey segments and check boxes",
     "In the Controls panel the chosen part of a segmented button and a ticked check box are now grey, like the sliders, in every theme. The label and the tick are white or black, whichever reads better. The bottom toolbar and the Settings tiles are unchanged."},
    {"0.15", "Full UI rework, stage 5r: the toolbar pills are flat",
     "The thin lighter rectangle behind the buttons of the bottom toolbar (and the 2D / 3D pill and the small chips) is gone: those pills now have one flat fill and no thin outline. Monochrome and High contrast keep their outline."},
    {"0.15", "Full UI rework, stage 5q: lit buttons in every theme, theme list, About scrollbar, End key, Element picker, grey sliders",
     "The button that is switched on in the bottom toolbars and in the top bar looked light blue, pink, green or brown in every theme except Dark. Now it is the same quiet look everywhere: a fill a little different from the bar, a strong icon (white on dark themes, the text colour on light ones), no outline and no glow; icons that are off are dimmer so the difference also shows without colour. The hover colour of those buttons is grey in every theme, not tinted with the accent. In Contrast and Monochrome the switched-on button no longer has a box edge. "
     "The theme list under the palette button now opens below the top bar instead of covering its icons. The scrollbar of About, Learn and the other pages keeps its 12 px width when you point at it or drag it (it used to grow). Pressing End on a page now goes all the way to the bottom in one press (the page grows a little while its bottom is first drawn, so it stopped about 1 % short). "
     "In the Atom Lab the Element picker now just floats over the picture: opening it no longer moves the atom, the toolbar, the FPS number or the scale bar (they stay exactly where they were); the picker stops above the scale bar's text, and the subject card is left out while the picker would cover it. The sliders in the Controls panel are grey in every theme, no longer blue. "
     "Not tested: Windows, a real mouse, a real graphics card, speed."},
    {"0.15", "Full UI rework, stage 5p: wheel after About, About scrollbar, zoom out further, lit buttons, Element picker",
     "Fixed: after you opened About (or History, or the log) and closed it again, the mouse wheel no longer zoomed the Orbit Lab or the Atom Lab until you restarted the app. The app kept thinking a page was still open. "
     "The scrollbar of About, Learn and the other pages is now 12 px wide with a faint track, and still drags; the wheel, PageUp, PageDown, Home and End already scrolled them. In the Atom Lab you can now zoom out twice as far in every view (Atom, Nucleus, Nucleon, Cloud, Cloud 3D, Slice and the Chart of nuclides): the picture can be half its old smallest size. "
     "The button that is switched on in the bottom toolbars and in the top bar (for example the gear while Settings is open) is no longer a light grey box: it is a dark fill a little lighter than the bar with a white icon, with no outline and no glow; the icons that are off are a little dimmer so the difference shows without colour. "
     "With the Element picker open in the Atom Lab: the picture, the toolbar, the FPS number and the scale bar no longer jump between two places from one frame to the next (the toolbar was centred on the free space, and the free space was worked out with the toolbar in it; I could reproduce the jump in the test at 880 x 560, not the exact look on your PC, so please test it). The Atom info text is not shown while the picker is open, and the picker no longer reaches the fact line, the toolbar or the FPS number. "
     "Not tested: Windows, a real mouse, a real graphics card, speed, 125 and 150 % scaling as a picture."},
    {"0.15", "Full UI rework, stage 5o: three small layout fixes",
     "With New UI (preview) On and the Dark theme: in a small window (880 x 560) the Orbit Lab's subject card no longer runs into the readout at the bottom right; it stops above it and scrolls inside itself (use the mouse wheel to reach \"All values\"). In the Atom Lab a long Did-you-know fact that did not fit beside the toolbar (for example with the Controls panel open) is no longer cut with \"...\"; it moves to the row above the toolbar and the scale bar moves up with it. Three paragraphs of the About page are worded a little shorter so that no line of text is too long to read comfortably. Not tested: Windows, a real mouse, a real graphics card, speed, 125 and 150 % scaling."},
    {"0.15", "Full UI rework, stage 5m part 2 and 5n: bigger handle, grey instead of blue, M1 windows, chart tilt slider",
     "With New UI (preview) On and the Dark theme: the handle on the picture's edge is bigger (32 x 72 points); hovered and pressed buttons are light grey, not blue; a lit toolbar button is the same light-grey pill as a chosen segment; the selected element in the periodic table has a light ring; the subject card steps aside while the Legend is open, so it never covers it; Settings, About, Learn, the history pages, the Benchmark windows and the pop-up lists use the same flat dark cards, rounded corners and grey selected pills. In the Atom Lab's Chart view a slider \"flat <-> 3D\" above the toolbar tilts the chart (the same tilt as the Controls panel). Not tested: Windows, a real mouse, a real GPU, how the slide feels."},
    {"0.15", "Full UI rework, stage 5m: three themes archived, the M1 look is the look",
     "Frutiger Aero, Y2K Night and Synthwave were taken out of the app (their code is kept in native/archive/themes). A saved choice of one of them now opens as Dark, with no message. The Settings row Theme effects went with them (only those themes used it), and so did the 🎁 surprise in the facts, which only unlocked Synthwave. With New UI (preview) On and the Dark theme, the look M1 (Inter font, flat dark cards, a light-grey selected pill) is now always on; looks M2 and M3 and the PL_LOOK switch were removed. With New UI Off nothing else changed. Not tested: Windows, a real mouse, a real graphics card, speed."},
    {"0.15", "Full UI rework, stage 5l: search finds every setting, Legend closes, toolbar centred",
     "With New UI (preview) On, search now finds every row of Settings (Benchmark, Font, Units, Motion, Theme effects, Did you know facts and the rest) and opens Settings at that row. An empty search box lists nothing, and Ctrl+K no longer brings back the old words. The Legend folds when you click anywhere else or press Esc. The bottom toolbar is centred on the picture in each lab, lights only one of Cloud, Cloud 3D and Slice, and has no Nucleus button (Controls, View has it). Small dropdowns fit their content without a scrollbar, and the Labs dropdown is opaque and in front. Not tested: Windows, a real mouse, speed."},
    {"0.15", "Full UI rework, stage 5k: pages scroll, Settings toggles, new gear icon",
     "With New UI (preview) On, About, Learn and the other pages now really scroll: the text used to stay in place while the scroll position changed. The mouse wheel and PageUp, PageDown, Home, End and the arrow keys work whichever window has the focus, and a slim scrollbar shows on the page. The Contents list jumps to each section. The Settings button closes Settings on a second click and has a new gear icon. Floating windows stay inside the app window, and the Benchmark Start button is always in view. Ctrl+Shift+F12 writes a small text file about the mouse and keys for finding problems. Not tested: Windows, a real mouse, speed."},
    {"0.15", "Full UI rework, stage 5j: look options M1-M3 for testing",
     "Three modern looks, M1, M2 and M3, were added for testing; they are not visible by default and nothing in the normal app changed. They keep the layout and change the details and the text: a clean modern font, flat dark cards with a thin edge, one solid selected pill, quiet muted section titles and thin icons. The old looks A, B and C were removed. "
     "To try one on a PC: set PL_LOOK to m1, m2 or m3 before starting the app, and turn New UI on with the Dark theme. M1 and M2 use the Inter font and M3 uses IBM Plex Sans (both SIL Open Font License 1.1, built into the app, listed in About). Not tested: Windows, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5i: the data sheet stays above the toolbar",
     "With New UI (preview) On, the data sheet (All values) now ends above the bottom toolbar and scrolls inside itself when it is taller. Look options added for testing (not visible by default). No new facts or sources. Not tested: a real PC, a real mouse."},
    {"0.15", "Full UI rework, stage 5h: the Atom data sheet gets the last cards, and the atom is centred in the free space",
     "New UI (preview) On: in the Atom Lab, All values now opens a data sheet with a second row of tabs: Spectrum (the real NIST spectrum), Lifetimes (lifetimes and jumps), Nuclear size, Temperature and, in the cloud views, Radial (the radial graph). Each tab is the Old panel's own card, drawn by the same code with the same words and the same numbers, and a tab only appears when the Old panel shows that card for the same atom and view. The sheet scrolls inside itself when a tab is taller than the window (mouse wheel, Page Up, Page Down). "
     "The atom and its label now sit in the middle of the space the subject card, the Controls panel and the bars leave free, so the card no longer covers them; the atom's info text at the top left is 14 px (it was 13). The Old panel button stays. No new facts or sources. Not tested: a real PC, a real mouse or wheel."},
    {"0.15", "Full UI rework, stage 5g part 2: panel sections scroll into view, Atom text and data sheet",
     "With New UI (preview) On: a Controls panel section you open now scrolls up so you can see what is in it (Pick a body used to open below the edge, out of sight). "
     "The Atom Lab's info text at the top left now stops before the subject card and the phase ring, and goes under the ring in a narrow window. "
     "The Atom data sheet has a solid background in the plain themes, and nothing in it is cut off with an ellipsis; the one \"…\" you may see in the isotope line (for example Uranium-238) is part of that line's own words: it means the nuclide has more decay modes than the two listed. "
     "Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stage 5g part 1: scrolling, Contents, a fixed toolbar, Cloud 3D, a flat handle",
     "Fixed: the Contents list in a Learn or About page worked once and then did nothing (the page had come in front of the list); it now works every time. The Page Up, Page Down, Home, End and arrow keys now scroll a Learn, About or history page, and in the new UI the wheel also scrolls it when the pointer is over the dimmed picture beside the page. "
     "I could not reproduce \"the wheel does nothing\" with a scripted wheel (it scrolled in every window, at three sizes, with the new UI On and Off), so a real mouse on a real PC still has to confirm it. "
     "With New UI (preview) On: the toolbar's left edge now stays in one place in every view and in both labs, every button keeps its slot and the optional ones (Drag, Centre) join at the right end; the Atom toolbar has Cloud 3D and Slice buttons beside Cloud; "
     "the first time the Atom Lab opens (no saved view) it shows Cloud 3D, which can be turned (the old layout still starts in Orbit); the colour ring of the phase Wheel no longer sits under the subject card; the Controls panel's handle is a flat, slim pill with one chevron in every theme. "
     "The light Y2K Ice theme was removed (a saved setting that names it now gives Dark); Y2K Night stays. Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stage 5f part 2: the Atom Lab's subject card, data sheet and legend",
     "With New UI (preview) On, the Atom Lab now has a card at the top right for the chosen isotope: its name, kind, the proton number, neutrons, nucleons, the isotope line, a More info / Less info button and All values. All values opens a data sheet with three tabs: Values (where you meet it, melting and boiling point, density), Decay (the decay history) and This view (the hint line of the picture). They are the very words the old panel showed, moved, not rewritten; the hint line and the More / Less info button are no longer at the top of the Old panel. "
     "On the Chart of nuclides the colour legend and the notes about the valley and the island now live in the Legend chip at the top left (closed at first); the picture no longer draws its own copy, which the Controls panel used to cover. "
     "The small text in the Orbit and Atom cards and in the data sheets is now at least 14 pixels (it was 13). Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stage 5f part 1: a tidier Atom Controls panel",
     "With New UI (preview) On, the Atom Lab's View section is now one compact choice (Atom, Nucleus, Nucleon, Chart, Cloud, Cloud 3D, Slice) with the current view highlighted, doing what the old rows and the toolbar buttons do. "
     "The long notes (the energy level, the cloud, the phase colours, the chart, the decay chain, the photon jump) now sit behind a row \"Notes about this choice (show)\", closed at first, with the same words. "
     "On the Chart of nuclides the Cloud section is left out, because its settings do nothing there (the old panel still shows them). Nothing about the physics changed. Not tested: a real PC or a real mouse."},
    {"0.15", "Full UI rework, stages 5d and 5e: the Atom Lab's options, marks and photons in the Controls panel",
     "With New UI (preview) On, the Atom Lab's Controls panel now has, after View: Orbital (the orbital, its orientation, the energy level n), Cloud (style, density, superposition, the slice), Orbit (Modern or Bohr 1913), Scale and motion (spin, scale, zoom, playback speed), Nucleus (quark style, nucleon detail and edge), Chart (tilt, what the height means), Marks (the 90 % outline, the phase colours, the decay chain and its walk) and Photons (emit, absorb, auto photons, check a jump). "
     "Each section is shown only in the views where the old panel showed those controls; every control changes the same setting, with the same words and notes (one electron, non-relativistic: as before). "
     "The fact line at the bottom left now wraps to two lines before it is cut, and the Atom Lab shows it too (the picture's scale bar moved above it). The benchmark's result now says whether the New UI was on."},
    {"0.15", "Full UI rework, stage 5c.1: the Controls panel opens from a handle, and tidier panels",
     "With New UI (preview) On: the Controls button is gone from the bottom toolbar. A small tab with an arrow on the right edge of the picture opens the Controls panel (arrow pointing left) and closes it (arrow pointing right); the panel slides in and out in about a fifth of a second, or at once when Settings, Motion is reduced. The panel's own close button still works. "
     "The glow and shadow round the new panels, the Legend and the toolbar now hug their edge (at most 2 pixels; in Aero the glow used to spread over whatever was next to them). The Controls panel is solid (the picture's labels showed through it), its section headers, rows and controls line up on one left and one right edge, its sections are evenly spaced and its small grey text is now normal text size. Not tested: a real PC, a real graphics card, speed, how smooth the slide looks. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5c of about 7: the Controls panel on the right, and two picture fixes",
     "With New UI (preview) On, in both labs, all the simulation options now live in ONE panel docked on the right side, called Controls (the owner's decision of 6 Oct 2026). It floats over the full-window picture under the top bar, about 22 em wide, has sections that fold (click a section's name) and scrolls inside. It is open at first; the Controls button at the right end of the bottom toolbar, or its own cross, closes and opens it, and the choice is kept like the cards'. The toolbar and the readout move left of it, the selected body is kept out from under it, and the Old panel (Atom Lab) opens left of it.\n"
     "Orbit Lab: Time speed, Show, Pick a body, Zoom, sizes and trails, and View options (Drag moves the view, Spin, Follow, Reset view) are sections of the panel, with the same controls, words and tooltips as before; the toolbar keeps the 2D / 3D switch, a Reset view button and the Controls button, and no carets. Atom Lab: the Nucleus level (Nucleus or Nucleon), Cloud, Cloud 3D and Slice, Build another and Reshuffle nucleons are in a View section; the Nucleus and Cloud buttons stay one click each. The rest of the Atom controls are still in the Old panel (next stages).\n"
     "Two fixes you reported: the distance-ring labels (for example \"6,500,000,000 AU · 103,000 light years\") are no longer drawn over the top bar, the toolbar or the Controls panel at a far zoom-out (the rings are still drawn); and the readout at the bottom right (zoom, scale, light travel time, frames per second) is now the size of the normal text on a soft dark rounded backing, so it reads over lines and stars in every theme. Not done yet: the Atom view options, marks and photon panels, the subject card, the legend and the data sheet. Not tested: a real PC, Segoe UI, Windows scale 125 / 150 %, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5b of about 7: the Atom Lab's element chip and periodic table",
     "With New UI (preview) On, the Atom Lab's chip at the top centre now reads, for example, \"U · Uranium-238\" over \"92 protons · 146 neutrons\" (with the right singular: 1 proton, 1 neutron, 0 neutrons). A click on it opens a panel under the chip with the periodic table (18 columns, every cell at least 28 units wide, the chosen element ringed, dashed edges for the elements with no stable isotope, the lanthanides and actinides in two rows below), the Protons arrows and slider, the Neutrons arrows and slider with a \"usual\" button for the element's most common isotope, and the list of famous isotopes. Click outside, press Esc or click the chip again to close it. This table is the big table: the old Bigger table button still works in the Old panel.\n"
     "Build another and Reshuffle nucleons moved from the Old panel into the toolbar's Nucleus panel (the small arrow next to Nucleus); they show only while the picture is zoomed to the nucleus. The hint line and More / Less info stay at the top of the Old panel for now. Still to come in stage 5c / 5d: view options, marks, photon panels, the subject card, the data sheet and the legend. Not tested: a real PC, Segoe UI, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 5a of about 7: the Atom Lab's toolbar and view buttons",
     "With New UI (preview) On, the Atom Lab now has the same floating toolbar as the Orbit Lab, at the bottom centre: Atom, Nucleus, Cloud, Chart of nuclides, a thin separator and Reset view. Each does exactly what the old control did. Atom shows the whole atom in the Orbit view; Nucleus zooms to the nucleus, and its small arrow lets you choose Nucleus or Nucleon (one proton or neutron); Cloud shows the electron as a cloud, and its arrow chooses Cloud, Cloud 3D or Slice; Chart shows the chart of nuclides; Reset view turns the picture back (on the chart: tilt, zoom and position). Centre and the Drag button (rotate or move) appear in the toolbar only when the old bar showed them. The old bar under the picture and the picture's own More / Less info button are gone in the new layout, and the picture now fills the whole window like the Orbit Lab, with its text lines and the chart kept clear of the top bar and the toolbar. Play and pause stays in the element chip (Space too), not on the toolbar.\n"
     "Not done yet (stage 5b): the element chip's periodic table, the view options, marks and photon panels, the subject card and the legend. Until then the Atom Lab keeps its Old panel button, and the Old panel now also holds the More / Less info button and the hint line that used to sit under the picture. Not tested: a real PC, a real graphics card, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 4 of about 7: the whole Orbit Lab in the new layout",
     "With New UI (preview) On, the Orbit Lab no longer needs the Old panel. Everything has a home: play and pause is a small button at the left of the date chip (Space still does it), Save picture is in the menu (and in search), the toolbar holds only what changes the view: speed, show, pick a body, zoom and sizes, 2D / 3D. The Time panel has Live (from the real date and time now, running) and Set manually (the date, the 24-hour dial, Go), the first four history jumps with More dates, and an (i) that explains the date range. Show now also holds the stars, the constellations and the far-away things. The Pick list has a filter box. The card for the selected body shows its name, what kind of thing it is, one big number (for a planet, its distance from the Sun) and four to six key values, with All values opening a sheet of every line the old card had, word for word. The Legend lists what the picture really draws, in the colours it draws them.\n"
     "The Did you know? button is gone: a fact now sits at the bottom left of the picture, fades to the next about every 20 seconds, stands still while the pointer is on it, and a click opens the whole fact (Settings, Facts: On or Off). Search (Ctrl+K) now finds bodies, dates, every Orbit Lab setting, menu actions, guides and the facts, takes Enter and the arrow keys, and lists your recent choices first. Links in the reading view are underlined as one line. The readout's outline is now black round light text and white round dark text in every theme. Performance and Benchmark are in Settings (with New UI On).\n"
     "Found on the way: the plan said 45 body tiles, the list has 47; none of the 27 facts has a named source in the app yet (the full fact says so). Not tested: a real PC, a real graphics card, Segoe UI, speed on Windows. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 3 of about 7: the new shell (a preview you can switch on)",
     "Settings has a new last section, New UI (preview), Off by default. Off is exactly the layout you know. On, the lab picture fills the whole window and a new frame floats over it: a slim top bar (menu, breadcrumbs with a Labs drop-down, the date and speed in a centre chip with a Today button, search, and small icons for the other lab, Learn, Achievements, Did you know?, Theme and Settings), a toolbar at the bottom (play, speed, show, pick a body, zoom and sizes, 2D / 3D, save picture, each with a small panel above it), the zoom, scale and frame rate in a small readout at the bottom right, a Legend that starts collapsed, and a card for the selected body that collapses to its title. If a card or panel would cover the body you picked, the picture slides so the body stays in the clear. Keys: Esc closes the open panel, Ctrl+K opens search, Space plays and pauses.\n"
     "About, History, the changelogs and the guides open in a reading view: a centred column about 70 characters wide over the dimmed picture, with a contents list. Text sizes follow one scale (13, 15, 17 and 28 units, never under 12, 14, 15 and 22 pixels, times Windows' scale and your Text size). Panels are nearly solid so their text passes the WCAG 4.5:1 rule over a white or a black picture behind them. Only the Orbit Lab's main controls are moved so far; everything else, in both labs, is behind the Old panel button at the top right until stages 4 and 5 move it.\n"
     "Found on the way: in Dark and Black the text on a chosen button (white on #6c9cff) is only 2.7:1; the new UI uses the page colour there, the old layout is unchanged. Text size Extra large was 19 pixels instead of 20 in the first build of this stage; the test caught it. Not tested: a real PC, a real graphics card, Segoe UI, speed. Nothing about the physics changed."},
    {"0.15", "Full UI rework, stage 2 of about 7: the plan, and four small fixes",
     "0.15 is the rework of every screen. Stage 2 is mostly a plan: a list of every control in the app and where each one will live (INVENTORY.md in the project), and the final design, chosen by the owner: the lab picture fills the whole window, with a slim top bar, a centre chip, small floating panels, a toolbar at the bottom and a reading view for long text. The earlier layouts A, B and C were rejected. "
     "Four fixes came with it. (1) The About page showed raw codes like \"&rsquo;\" instead of the characters; they are now decoded (all 254 of them, and the whole HTML list is understood). (2) The page effects of Frutiger Aero, Y2K Ice and Y2K Night (clouds, bubbles, grain, grid, halftone, light streak) are drawn once into a picture and reused, instead of being rebuilt every frame; the pictures stay the same within a few levels of rounding, and the saving is expected to be about 0.7 ms per frame (measured by the owner's PC, not here). "
     "(3) Windows is asked not to slow the app down when its window loses focus, and the benchmark report now flags any test during which the window lost focus or was minimised. (4) The app now notices when the window moves to another monitor or when Windows' display scale changes, and resizes its text; there is also a smallest window size. "
     "Not tested: anything on a real Windows PC (power behaviour, scale changes, speed): the project's tests use stand-ins for those. Nothing about the physics changed."},
    {"0.14", "Themes (complete): richer styles, Frutiger Aero, Y2K Ice, Y2K Night, a Theme effects switch (all four were taken out again, see 0.15 stage 5g and 5m)",
     "0.14 is finished (it was planned in five stages; the owner ended it after three, and the stage entries below say what each did). What you get: themes are no longer only seven flat colours but can have a gradient page, glass, gloss, a rim, a glow, glossy title bars, their own fonts and decoration. Frutiger Aero (light) is glossy and saturated, aqua and leaf green with jelly buttons. Y2K Ice (light) and Y2K Night (dark) are the cold techno-print looks with chrome buttons, grain, halftone dots, grid lines, corner brackets and the free Michroma font for titles. Light's blue text is a little darker so it reads (4.57:1). Windows and pop-ups are solid, so nothing behind them shows through the text. "
     "New in the last stage: Settings has Theme effects On / Off (saved). Off removes the clouds, bubbles, grain, halftone dots, grid lines, light streaks, glints, reflections, corner brackets and HUD marks and keeps the colours, the gloss and the fonts; the benchmark report says which it was. The other themes have none of this decoration, so nothing changes for them. "
     "Dropped by the owner, not postponed: glass and chrome planets and nucleons, glossy chart tiles, theme-aware Cloud dots and animated backgrounds. The pictures of the labs keep their own colours in every theme. Next is 0.15, a full rework of the screens to make the app clean. Not tested: a real PC, a real graphics card, speed."},
    {"0.14", "Themes, stage 2 of 5: Frutiger Aero's real look, a readable Light, and two Y2K themes",
     "Frutiger Aero is more saturated now: the page goes from sky blue through aqua to leaf green all the way down, buttons are aqua jelly (a strong white highlight over the top, a deep colour below, a thin darker rim, a glow when you point at one) and chosen buttons and tabs are lime jelly, windows have glossy deep-blue title bars, a bright rim and a soft glow so they read over the dark space picture, a faint reflection sits under windows and cards, and the page has soft clouds and glass bubbles (still, not animated yet). Combo boxes, their lists and the tooltips follow the same style. "
     "Two new themes, from one style: Y2K Ice (light) and Y2K Night (dark). The cold techno-print look: icy blue, cold white, black and brushed silver (violet only as a light streak in Night), fine film grain, halftone dots and thin grid lines on the page, thin HUD rules, corner brackets and a tiny SYS label on windows, chrome buttons with a cold highlight line and a glint when you point at them, a soft blue glow on the chosen tab, and window titles and the app name in the free wide font Michroma (SIL Open Font License, built in; the body text keeps its normal font). Night's buttons are dark gunmetal on purpose: text must stay readable at the brightest point of the gloss. "
     "Light: its blue text was 4.24:1 on its page, under the 4.5 that text needs; it is a little darker now (4.57:1, the same hue), so the pictures of Light changed on purpose. "
     "Tests: every text pair of Aero and Y2K passes WCAG 2.x over the page's textures at their strongest, over every button state and over every title bar; decoration stays in a 7 point band at the window edges (picture test); the title font is only on titles; the 50 test pictures of the unchanged themes are identical to 0.13. Not tested: a real PC, a real graphics card, speed."},
    {"0.14", "Themes, stage 1.1: a fix, Frutiger Aero's floating windows are solid now",
     "The owner found on the real app that in Frutiger Aero the guides (\"What am I even looking at?\"), the changelog and some other windows were see-through: the Learn cards and the Solar System readout showed through their text. Cause: stage 1 made window backgrounds see-through for glass themes but did not paint every window. Now every window and pop-up that holds text is solid (the glass look is the sky gradient, a bright line along the top, the rim and a soft shadow), only the docked panels stay see-through, and the scroll bars show again. The older themes are unchanged (the 72 test pictures are identical). New tests: every window opened by the app must be painted by the theme; the inside of a window must be the same picture over two different backdrops (Aero, Dark, Light); every theme's text passes 4.5:1 over any backdrop colour. Found, not changed: the Light theme's accent text on its page colour is 4.24:1, a little under 4.5. NOT TESTED: a real PC and a real GPU."},
    {"0.14", "Themes, stage 1 of 5: richer theme styles and the Frutiger Aero theme",
     "A theme is no longer only seven flat colours. It can now also have a page that fades from one colour to another, glass panels (see-through, with a bright line along the top and a soft shadow under floating windows), glossy buttons, tabs, check boxes and sliders (a bright top half, a tinted lower half, a pressed look), its own corner roundness and its own font. All of this is data in the theme, not code for one theme, and every theme that existed before draws exactly the same pixels as in 0.13 (checked by pictures of six screens in all 12 versions, dark and light). "
     "New theme: Frutiger Aero (light): a sky-blue page, frosted glass, glossy aqua and fresh-green buttons, rounder corners. It uses Segoe UI when Windows has it; the font is Microsoft's and is not shipped. Every text and background pair in it passes the W3C's WCAG 2.x contrast rule (4.5:1 for text, 3:1 for large text and interface parts), glass over the lightest and darkest part of the page included; the tests check the formula against values worked by hand. "
     "Colours that used to be written inside drawing code (error red, the red button, the faint illustration tint, the grey of tracks, the fill colour of chosen buttons) now come from the theme with the same values. The pictures of the labs (space, cloud, chart) keep their colours in this stage. "
     "NOT TESTED: the look on a real PC with Segoe UI and a real graphics card, and speed. Combo boxes keep the flat frame. The owner judges the look."},
    {"0.13", "GPU update, finished: the Orbit Lab's rocks and the chart of nuclides on the graphics card, and what 0.13 is in all (stage 4 of 4)",
     "0.13 \"GPU update\" in one place. WHAT MOVED to the graphics card (OpenGL 4.3, with OpenGL 3.3 as the fallback that keeps the old processor code): the phase colours of the electron cloud's dots (stage 1); the colour groups of a superposition and of a photon jump (stage 2); the 90 % outline's density grid and level, and the Slice contour (stage 3); and now the places of the Orbit Lab's belt rocks and the chart of nuclides' tiles and blocks (stage 4). Stage 4: "
     "(1) The 6,000 rocks (48,000 in the stress scene) used to be placed one by one by the processor every frame, Kepler's equation included; now a compute shader does it, with the time kept in double precision on the graphics card so a rock is right after 10,000 years at the fastest speed too (tested against Kepler's equation solved separately, at times up to ten million years, to 0.003 of a point). While checking it I found that the belt code solved Kepler's equation with only three Newton steps: exact for every rock the app makes, wrong for orbits with e of 0.8 and more (none in the app); it now iterates until converged, and the app's rocks do not move. "
     "(2) The chart's 3,557 squares or blocks are made once and kept in a graphics-card buffer instead of being rebuilt and sent every frame (a landscape frame sent 3.2 MB); tilt, zoom and pan are applied on the graphics card; a rebuild happens only when the picture size, the theme colours or the height meaning changes. The hover and click still answer exactly as before (tested against the drawn blocks). "
     "(3) The Atom Lab's Cloud 2D view cost more than Cloud 3D for a reason that was not the colours: its list of 48,000 dots was built again from nothing every frame, grown about 17 times; it is now kept, and the renderer takes it without copying (measured here: scene 10 from about 3.9 to 2.6 ms of processor time). "
     "THE SWITCHES (none is saved; all start On): GPU colours, GPU superposition and jumps, GPU outline, GPU rocks (Orbit Lab) and GPU chart cache, in the Performance card; the benchmark report says which way each ran, and scenes 23 to 34 run the matching scenes again with the old way (31 and 32: the Orbit Lab; 33 and 34: the chart). "
     "ALSO IN 0.13: the orbitals' normalisation is exact now (stage 3), a physics fix of up to 4.4e-5 in amplitude. "
     "MEASURED BY YOU (5 Oct 2026, stage 2 build): scene 1 2.24 ms, scene 6 5.34 ms, scene 10 4.43 ms, scenes 19 to 22 1.3 to 2.1 ms. NOT MEASURED: every speed-up in 0.13 is EXPECTED, not measured, because there was no graphics card or Windows PC in the sessions that wrote it; the tests compare the graphics card's results with the processor's on a software OpenGL (Mesa llvmpipe), where a real driver may round 32-bit numbers differently. Run the benchmark with the extra scenes and compare each scene with its Off twin."},
    {"0.13", "GPU update: the 90 % outline on the graphics card, and exact orbital normalisation (stage 3 of 4)",
     "The first time a 90 % outline is needed for an orbital, the processor used to make a 64 x 64 x 64 grid of densities and sort it to find the 90 % level: about 30 ms in a measurement here (13 ms for the densities, 17 ms for the sort), the only real hitch left in the app. "
     "Now, on a PC with OpenGL 4.3, compute shaders on the graphics card make the grid and find the level (by a search over the values, no sort), and the picture does not wait for them: the outline appears a frame or two after it is switched on. In the Slice view the cut's contour values are made the same way, so the contour is a frame behind the cut while you drag it. "
     "The 90 % mark of the radial graph no longer makes the panel wait either. A PC with only OpenGL 3.3 keeps the old way. The Performance card has a switch (GPU outline) and says which way your PC runs; the benchmark report prints it; scenes 29 and 30 run scenes 16 and 17 again with the old way. "
     "The outline is the same as before: the graphics card's level agrees with the processor's to 1e-4 and the cells to float rounding, and the tests compare them cell by cell. "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. "
     "A physics fix on purpose: the orbitals' normalisation was a numerical sum that left the wave function too big by 3e-6 (2p) up to 4.4e-5 (4f z^3) in amplitude; it is now exact (closed form), checked for every orbital the app offers to 1e-10. The only numbers that move are the recorded jump amplitude sums in the tests; nothing you see changes."},
    {"0.13", "GPU update: superposition and photon jumps on the graphics card (stage 2 of 4)",
     "In a superposition (and while a photon jump is in flight) the processor used to work out, for every one of the 48,000 dots and every frame, whether the dot is shown, and its colour. This was the slowest part of the app (the overlapping superposition was the slowest scene of 0.12). "
     "Now, on a PC with OpenGL 4.3, a compute shader on the graphics card does that from the amplitudes stored with each dot, and the processor only sends the beat phase and the weights. A PC with only OpenGL 3.3 keeps the old way and nothing else changes: the app asks for 4.3 first and falls back to 3.3. "
     "The Performance card says which way your PC runs (\"OpenGL 4.3: GPU superposition on\" or \"OpenGL 3.3: CPU path\"), has a switch to compare the two, and the benchmark report prints it; scenes 25 to 28 run the jump and the three superpositions again with the old way. "
     "The physics is the same: the formulas are ported line by line and tested against the processor's for every dot. The graphics card works in 32-bit numbers, so a dot within rounding of a threshold (a few in a million) can come out differently; each such dot is tested to be that close. "
     "The tests also check the superposition against the textbook hydrogen formulas written out separately, and found that the lab's orbital normalisation is off by 3e-6 to 1.6e-5 in amplitude (not changed, reported). "
     "Not changed: the building of the dots of a jump (the random sampling is a chain that cannot run in parallel). "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. Scene 14 should drop from about 10 ms towards the 4 ms of one plain orbital."},
    {"0.13", "GPU update: phase colours on the graphics card (stage 1 of 4)",
     "The electron cloud's dots used to be coloured by the processor: for the 24 Wheel colours it went over all 48,000 dots once for each colour, in each of three depth bands. Now each dot carries only the number of its colour and the graphics card picks the colour. "
     "The picture is meant to be exactly the same (the same colours, the same depth bands, the same drawing order); the tests compare the old way and the new way dot by dot and, with a software OpenGL, pixel by pixel, and found no difference. "
     "This applies to every cloud view (Cloud, Cloud 3D, Slice), Classic and Glow dots, Sign and Wheel, every orbital and density. "
     "The speed-up is EXPECTED, not measured: it could not be tried on a real graphics card or on Windows here. The benchmark's scene 8 should drop towards scene 9, and scene 10 likewise; scenes 23 and 24 run them again with the old way for comparison. "
     "A switch in the Performance card, GPU colours (Atom Lab cloud dots), goes back to the old way (it is On again at every start). "
     "Two texts in the benchmark's scene list pointed at the wrong scene (scene 19 said to compare with scene 8, it is scene 2; scene 22 said scenes 16 to 18, it is 19 to 21) and the tick box said 11 extra scenes (it is 15 now): fixed. "
     "The next stages plan OpenGL 4.3, which needs a graphics chip of about 2013 or newer (Intel HD 4600, GeForce 600, Radeon HD 7000); older PCs will not run those versions. The app's version is now 0.13."},
    {"0.12", "Atom Lab: the accuracy clean-up (stage 2 of 2)",
     "This stage fixes data and wording that earlier versions found doubtful and only reported. Nothing about how the atom or the chart works has changed; some numbers have, and each is listed here. "
     "Nuclides with no half-life number were all described as \"seen only fleetingly\". The data does not say that. They are now described by what NUBASE2020 says: 369 only have an extrapolation from systematics, 84 have none, 11 only a limit, 6 an approximate value, 3 are only known to be unstable to emitting a particle. "
     "The one-line text and the decay readout no longer show an extrapolation or a limit as if it were a half-life (fluorine-31 used to say \"half-life 2 ms\", boron-16 \"half-life 4.6e-21 s\" for a limit), and the decay clock no longer makes such a nucleus decay on a timer: it stays as you chose it, and the readout says why. "
     "Krypton-78 and xenon-124 had half-lives that are in neither NUBASE2020 nor the mendeleev package, which both call them stable; the numbers had no source, so they are now shown as NUBASE2020 shows them (observationally stable, with its limits). "
     "113 superheavy half-lives had been rounded and marked \"approximate\" although NUBASE2020 gives plain measured values: they are now NUBASE2020's own. 16 more superheavy values came from an older evaluation and were up to 2.8 times off (seaborgium-269: 14 minutes before, 5 minutes now). "
     "For 644 nuclides NUBASE2020 lists a decay mode with no intensity (marked with a question mark); the odds leave it out, and the hover, the line in the panel and the chain now say so. "
     "The nucleus's shape estimate counts Z = 114 and N = 184 as magic numbers; they have no source, so they stay but are now labelled in the nucleus card (286 nuclides depend on them). "
     "This finishes version 0.12."},
    {"0.12", "Atom Lab: decay chains walk the chart (stage 1 of 2)",
     "On the Chart of nuclides, the chosen nuclide now shows where it goes next. Every way it can decay is drawn as a line from square to square, in the colour of the mode: an alpha jumps two squares down and two left, a beta minus moves one square up and one left, a beta plus the other way. "
     "The likeliest way is thick and solid, the other ways whose odds are measured are thinner, ways below 0.1 % are faint hairlines, and a way whose odds are not measured is marked with a question mark. Uranium-238 reaches lead-206 in 14 steps along its likeliest path; bismuth-212 splits in two paths (beta minus 64 %, alpha 36 %) that meet again at lead-208. "
     "A nuclide whose half-life is only a limit, only an extrapolation or not given is marked with a cross: the chain stops there and says why, and no time is made up for it. A chain that reaches a stable nuclide names it, and says when it is only observationally stable (nothing seen, but a limit is listed). "
     "You can also walk the chain: one step at a time, or automatically. Each step says what happened (for example: alpha, two protons and two neutrons leave; uranium-238 to thorium-234) and uses the lab's real random draws, a lifetime from the exponential law with the real half-life and a branch from the real odds, so it is different every time. "
     "The walk is NOT real time. Uranium-238 really lives billions of years; the screen shows a step every 1.6 seconds, and each step says how many times faster than real time it was shown. The nuclide you chose stays where you put it. "
     "The decay clock that runs in the other views now adds up the listed odds to 1 (before, a rare way listed after a certain one, such as fission for uranium-238, could never happen). "
     "A fourth benchmark scene measures the chain drawn on the landscape. The app's version is now 0.12."},
    {"0.11", "Atom Lab: what the height means, and the valley and the island of stability (stage 4 of 4)",
     "The Chart of nuclides has a new choice, Height means. Besides the half-life it can now be the binding energy per nucleon, the quantity behind the whole chart: it rises steeply from hydrogen, reaches its highest value in the data at nickel-62 (8,794.56 keV per nucleon, with iron-58 and iron-56 next; the app reads this from the table, it is not typed in), and falls slowly towards uranium. That is why fusing light nuclei releases energy and splitting heavy ones does. The scale is linear from 0 to 9,000 keV per nucleon, and the legend says which height is in use. "
     "The numbers are from the 2020 atomic mass evaluation (AME2020). It marks 1,008 of the 3,557 nuclides as estimates, not measurements, including every nuclide above Z = 110, so those are drawn flat and pale and never as a height. "
     "Switching the height changes nothing else: the colours, the magic-number bands, the hover text and the chosen nuclide stay exactly as they were. "
     "The chart now names the valley of stability, the band of stable nuclides that the data already shows, and says it is measured. It also marks the heaviest known nuclides (Z = 118) and says that nothing has been observed beyond them. "
     "The island of stability is a prediction: theory expects a region of superheavy nuclides beyond the heaviest known that might live much longer than their neighbours. The app says so plainly and draws nothing there, because no source for where it lies could be reached from the project's tools; it says only that where it lies is disputed. "
     "This finishes the chart of nuclides (version 0.11)."},
    {"0.11", "Atom Lab: the chart of nuclides as a landscape (stage 3 of 4)",
     "The Chart of nuclides view has a new Tilt slider. At 0 it is exactly the flat chart of the last version; as you move it up, the chart turns into a landscape seen from a fixed isometric angle, and every nuclide becomes a block whose height is its half-life. The stable nuclides stand in a ridge along the middle, and the short-lived ones fall away on both sides like a shore. "
     "The height is a chosen scale, not a measurement: the logarithm of the half-life, from 10^-23 seconds (flat) to 10^32 seconds (the top), where each step up is ten times longer; the legend says so. Stable nuclides have no half-life, so they all stand at one fixed height above the top of the scale, meaning no decay seen, not a long number. "
     "A nuclide with no measured half-life, or only an extrapolated one, has no height at all: it is drawn flat, hollow or pale. A pale block is only a limit or an approximate value. "
     "The wheel zooms about the pointer, a drag moves the picture when zoomed in, and a reset button brings back the flat chart. Hover and click work on the block you see on top, including its sides. "
     "A benchmark scene measures the landscape. The next stage is about what the height can mean."},
    {"0.11", "Atom Lab: the chart of nuclides (stage 2 of 4)",
     "A new view, \"Chart of nuclides\", is a map of every nuclide the lab knows: 3,557 squares, neutrons across and protons up. Each square is coloured by how that nuclide decays (stable, beta minus, beta plus or electron capture, alpha, fission, proton or neutron emission), and the legend says in words what each colour means. The stable nuclides run along the middle like a valley, with neutron-rich nuclei on one side and proton-rich ones on the other. "
     "The lines are the magic numbers 2, 8, 20, 28, 50, 82 and 126, where a shell of nucleons is full. The nuclide you have chosen is marked with a box and cross-hairs, so you do not lose your place. Hover a square for its half-life, how it decays and with what odds, its spin, and whether the lab has a measured charge radius and a measured quadrupole moment; where the lab has no value, it says so. "
     "Click a square to choose that nuclide: the other views then show it. The arrow keys move one proton or one neutron, which is the natural way to walk a decay chain. While the chart is shown the chosen nucleus does not decay on its own, so it stays where you put it. "
     "The spins and the kind of each half-life (a measurement, a limit, only an extrapolation, or none) come from NUBASE2020, which is now listed under Where the facts come from. The colours are a convention of this lab, not a measurement; where a decay has several branches, the colour is the kind with the largest total odds. The chart shows what has been observed: it says nothing about the island of stability or about heights, which come in the next stages. "
     "The app's version is now 0.11."},
    {"0.11", "Atom Lab: Auto photons now say why they cannot jump (stage 1 of 4)",
     "With Auto photons on, a complex orientation (m = ±1, ±2, ±3) or a superposition used to leave the atom doing nothing, with no word about it. Now the lab says why, in the same words the Emit and Absorb buttons use, once for each reason (not again at every look), and again if you change the state or switch Auto off and on. Auto stays on and starts jumping by itself as soon as you pick a real orientation (px, pz, ...). "
     "Nothing else changed: real orientations jump exactly as before (the same photons in the same test run). "
     "Still true: the lab does not do photon jumps from a complex state or a superposition. Light can in fact take such a state to another level (the rule l changes by 1 still holds; the light's polarisation decides which m it ends in); the lab refuses because it does not model the polarisation, and whether to allow it is left to the owner's decision. The next stages of 0.11 are the chart of nuclides."},
    {"0.10.9", "Atom Lab: the nucleus's shape from measured quadrupole moments (stage 2)",
     "The shape of a nucleus now comes from a measured quadrupole moment wherever one can be used: the IAEA's recommended values (fetched 2026-10-04, 598 ground-state rows). A positive moment means stretched (like a rugby ball), a negative one flattened (like a pancake), and the card and the readout line say which the nucleus on screen is. "
     "Where there is no usable moment the lab keeps what it had, and now says which: a B(E2) value (11 isotopes) or an estimate; of the neutron slider's 4,252 isotopes, 498 now take their shape from a measured moment, 11 from B(E2) and 3,743 from the estimate. "
     "The table gives the spectroscopic moment, the one a laboratory measures in the nucleus's spin state; the shape needs the intrinsic moment in the nucleus's own frame. The two are turned into each other with the strong-coupling relation (K = I), which the card calls a model assumption, not a measurement; it is poor for light nuclei and near closed shells, and where it would need |β₂| above 0.6 the moment is shown and not used. "
     "A missing moment never means round: a nucleus of spin 0 (every even-even nucleus) or spin 1/2 has no spectroscopic moment whatever its shape, and the card says so. Values without a sign, with a tentative sign or spin, or typed wrongly in the file are listed and not used. A deformed nucleus keeps its volume, and its protons still give the measured charge radius and have exactly the measured quadrupole. "
     "Doubly magic nuclei, drawn round by expectation, are now labelled an estimate. The shape is static: real nuclei also vibrate and rotate. Halo nuclei are not in this stage; they wait for data."},
    {"0.10.9", "Atom Lab: every nucleus at its measured size (stage 1)",
     "The nucleus is now drawn at its measured size wherever there is a measurement, always on (there is no setting). The numbers are the IAEA Nuclear Data Section's table of measured nuclear charge radii (fetched 2026-10-04): 908 isotopes of 90 elements. A new card, \"Nuclear size (measured)\", shows the measured radius with its uncertainty next to the usual rule's (r = 1.2 A^(1/3) fm) and the difference as a number. "
     "A charge radius is the rms size of the protons' electric charge, and the nucleus is drawn as a uniform ball of the radius that has that rms size (an assumption: real nuclei have soft edges). The nucleon cloud is scaled so its protons have exactly the measured rms radius. Light nuclei change most: helium-3 is 47 % bigger than the rule's ball, lithium-6 53 %, the deuteron 83 %; lead-208 changes by 0.1 %. "
     "Helium-4 is smaller than helium-3 and lithium's radius falls to lithium-9 and rises at lithium-11: the card shows those measured numbers and claims no explanation. Where there is no measurement (3,345 of the 4,252 isotopes the neutron slider offers) the old rule stays and the card says \"no measured radius for this isotope; size from the usual rule\"; 48 isotopes have only a preliminary value in the file, which is named and not used. Hydrogen-1 keeps the CODATA proton radius. "
     "Limits, said on the card: a charge radius is a size, not a shape, and not a matter radius (neutrons carry no charge), so halo nuclei and measured nucleus shapes are not in this stage; they wait for data. The picture is static: real nuclei also vibrate and rotate. The text \"The nucleus is ~N× smaller\" and the chance of the electron being inside the nucleus use the new size."},
    {"0.10.8", "Atom Lab: real lifetimes and selection rules, from NIST (stage 3)",
     "A new card, \"Lifetimes and jumps (NIST)\", says how long the state on screen lives and how likely each jump is. The numbers are NIST's: the rate A of each jump (the chance per second that the state makes it by itself) with the accuracy rating NIST gives it. "
     "The lifetime is 1 divided by the sum of the rates of every allowed way down, and the share of a jump is its rate over that sum. \"Check a jump\" asks about any two shells: if the jump is allowed you see its rate, rating and share; if not, the card names the rule that forbids it (parity, l or j) and, where NIST lists it as a forbidden jump, shows NIST's rate under that name. "
     "Where NIST has no rate the card says so and shows no number; the lab never makes one up. The 2s state of hydrogen has no allowed way down: the card gives no lifetime for it and says that a real 2s state still decays by emitting two photons, which NIST does not list and the lab does not model. "
     "The numbers and the rules are tested against NIST's own data (every allowed line in it is allowed in the lab, none of the forbidden ones is, rates scale as Z\u2074 across 109 ions); the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.8", "Atom Lab: the Dirac formula's arithmetic made exact, and a Windows test fixed (stage 2)",
     "The Windows build of the previous stage failed in its new tests. The cause was in the tests, not in the physics: a reference value was computed in a way that throws away about 11 of 16 digits (it subtracts 1 from 0.99997), and only passed on Linux because the Linux compiler's extra-long numbers hid it; the lab's own value was the accurate one. "
     "The tests now use a reference that is exact on every computer. The lab's own arithmetic was also looked at for the same weakness: the small relativistic correction to each level was a difference of two nearly equal energies and lost up to 5 digits for hydrogen; it is now worked out directly, as are the percentage and the 1s energy in the heavy-atom readout. "
     "Nothing you can see changes: every line, level, note and number shown is the same text as before (checked for all 118 elements), and with the setting off nothing at all changed."},
    {"0.10.8", "Atom Lab: relativity as a real option, the Dirac levels (stage 1)",
     "New setting under Light spectrum, \"Relativity (Dirac levels)\", off by default. Switched on, the two spectrum cards use the energy levels of the Dirac equation (one electron, point nucleus) instead of the non-relativistic formula: a level then depends on j, the total angular momentum, as well as n, so 2p splits into 2p 1/2 and 2p 3/2 and every line splits, more and more as Z grows. "
     "The strip shows the split lines, a short list gives the split levels, and the Real spectrum card now compares the lines the lab draws, component by component, with NIST's measured lines: the lab lands 8 times closer for hydrogen, 10 times for He\u207a and 32 times for Fe\u00b2\u2075\u207a. "
     "What it does not do, and the app says so: the pictures (clouds, orbitals, outlines, radial graph) are still non-relativistic, and so are the readouts, the photon of a jump and the quantum beats; there is no Lamb shift, no other QED effect and no size of the nucleus, so NIST's lines still differ a little (in Dirac theory 2s and 2p 1/2 have exactly the same energy). "
     "With the setting off, nothing changes (compared byte for byte with the previous version). The numbers are tested against hand arithmetic and NIST's data; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.7", "Atom Lab: the real spectrum from NIST next to the lab's (stage 2)",
     "A new card, \"Real spectrum (NIST)\", puts the measured spectrum from the NIST Atomic Spectra Database next to the lines the lab predicts, as two separate comparisons. "
     "(a) The ion the lab actually models (one electron round a nucleus of charge Z: H I, He II, ... Fe XXVI): the lab's lines should land almost on the measured ones, and what is left is mostly fine structure and relativity, which the lab leaves out. This checks the lab. "
     "(b) The neutral atom of the element (Fe I, Ne I, ...), which is what \"the spectrum of iron\" means: it has Z electrons and the lab has one, so the lines are not expected to match. This shows the lab's limit. "
     "Where NIST has no data the card says so. The lines were fetched on 4 October 2026 and are stored in the app (a selection, stated in About); intensities are NIST's rough relative values and are drawn only within one spectrum. "
     "The numbers are tested; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.7", "Atom Lab: how far from the nucleus the electron is found (stage 1)",
     "The Atom Lab's cloud views have a new card, \"Where the electron is found\": a graph of how likely the electron is to be at each distance from the nucleus, for the orbital on screen. "
     "It marks the most likely distance, the average distance (the same number as the readout's \"theory\") and how far the 90 % outline reaches, with the distance in Bohr radii and in Å, scaled for the orbital and the nucleus. "
     "Every orientation of a shell has the same curve, and the card says so. It is not drawn while a superposition is shown, and it waits for the landing during a photon jump. One electron, non-relativistic, point nucleus. "
     "The numbers are tested against textbook formulas, exact peaks and the dots themselves; the drawing could not be run in the cloud session and has to be looked at on a real screen."},
    {"0.10.6", "A test that did not hold on Windows, fixed (stage 9)",
     "A test of the faster cloud jump compared the results byte for byte with numbers recorded on another compiler; the last bits of cos, log and exp may differ between compilers, so it failed on the Windows build although nothing was wrong. "
     "It now runs the old way and the new way side by side on the same computer and compares them exactly, and compares the recorded numbers with a stated tolerance. Nothing in the app changed."},
    {"0.10.6", "Atom Lab: the cloud jump made cheaper (stage 8)",
     "The benchmark on a real PC showed the Atom Lab's cloud jump (an electron jumping between orbitals in the cloud views) was the slowest thing in the app: about 33 frames a second for the 1.2 seconds it lasts, with a long hitch at its start. "
     "The jump now does the work that is the same for every dot once per frame instead of once per dot, and builds its new dots over the first four frames instead of in one. The pictures and the random dots are exactly the same as before (tested bit for bit). "
     "It was not timed in the cloud session: the speed has to be confirmed by running the benchmark again."},
    {"0.10.6", "Atom Lab: speed scenes added to the benchmark (stage 7)",
     "The benchmark has a new tick box, \"Include Atom Lab speed-risk scenes\": 11 scenes for the Atom Lab's heaviest paths (phase colours with Classic dots, a cloud jump, a superposition, the 90 % outline's first use and with the cut dragged, the nucleus view). "
     "They were not timed yet (the cloud session has no graphics card); the numbers will come from a real PC, and nothing was made faster in this stage."},
    {"0.10.6", "Atom Lab: the nucleus's real mass, and spectrum checked against NIST (stage 6)",
     "The nucleus's mass in the Atom Lab now comes from the measured binding energies (AME2020), not from the mass number times a mass unit, so the light of deuterium and tritium is worked out with the right mass (deuterium's red line moves by 1.3 trillionths of a metre; nothing else visible changes). "
     "The tests now check the spectrum against the Rydberg formula worked out by hand and against the NIST Atomic Spectra Database, and check more basic facts about the orbitals."},
    {"0.10.6", "Atom Lab: clearer, more honest words (stage 5)",
     "The Atom Lab now says \"non-relativistic\" for every atom and warns the same way, from the same atom (cobalt, Z = 27) on, in the readout, the hint and the spectrum note. Visible light is 380 to 780 nm everywhere. "
     "The average radius from the dots appears only once the cloud is full, and says it is an estimate. Two Learn steps no longer claim more than the lab can show (the neon button, and the 2013 electron-wave picture)."},
    {"0.10.6", "Atom Lab: the right outline, a correct superposition (stage 4)",
     "The thin 90% outline of a complex state (m = +1, +2 ...) now has that state's own shape, a doughnut for 2p with m = +1 (it used to show the dumbbell of the real orbital). A superposition of two states that overlap, such as the same state twice, "
     "is now properly normalised, so its cloud is the right density and all its dots are shown; the line over the picture says when the mixture is renormalised. Other superpositions are unchanged."},
    {"0.10.6", "Atom Lab: one energy, a steadier orbit, Fixed scale (stage 3)",
     "Hydrogen's energy now reads -13.60 eV in every view (it said -13.61 in two of them): the readout uses the same level as the spectrum, with the nucleus's real mass. In the Orbit view the electron no longer breathes at the fastest playback speed "
     "(the orbit is calculated in smaller steps; the speed itself is the same). With Scale set to Fixed the biggest clouds (n = 4) now fit in the picture. A new note says the energy is the one-electron, non-relativistic level."},
    {"0.10.6", "Atom Lab: exact constants (stage 2)",
     "The Atom Lab now uses the official CODATA 2022 values (read from NIST) for the Bohr radius, the Hartree energy, the atomic unit of time, the Rydberg energy, the masses and more, instead of rounded ones. "
     "The Bohr radius is 0.03 % larger than before, so the electron's circle is a hair bigger; everything else looks the same. The About page lists the constants and where they come from."},
    {"0.10.6", "Atom Lab checked for errors", "Atom Lab checked for errors, fixes to follow; nothing changed yet."},
    {"0.10.5", "Orbit Lab: more checks and clearer sources (stage 5)",
     "Orbit Lab: tests now compare the lab with NASA/JPL Horizons for the famous dates (Mars closest, 2003; Jupiter and Saturn, 2020) and for the 2026 equinoxes (the Earth card's \"next season\" is within 12 minutes), and the texts of the jump notes, the Real and Simple notes, the date line and the hidden-selection rule are tested too. Sources: the speed of light is JPL's; the Sun's 25.38-day turning period is Horizons'; comets' nucleus sizes are the Small-Body Database's (Encke 4.9 → 4.8 km). Numbers that have no named source are now marked rough on the cards (the Sun's surface temperature and its 25- and 34-day turning, the belts' counts and masses, NEOWISE's size), and About lists them. Nothing else changed."},
    {"0.10.5", "The official astronomical unit (stage 4)",
     "The Orbit Lab now uses the official astronomical unit, 149,597,870,700 m (the IAU value, as on NASA/JPL's astrodynamic parameters page). Before, it used a rounded 149.6 million km, which is 2,129 km too long at 1 AU (64,000 km at Neptune). Every distance in the lab, the same orbits in Simple, the comets, the belts and the Oort cloud, is now 0.0014 % shorter; the times and the orbits' shapes do not change. Against NASA/JPL Horizons no planet got worse by more than that change can explain (Uranus and Neptune about 30,000 and 39,000 km farther off in their worst year, the others within 1,300 km)."},
    {"0.10.5", "Orbit Lab data fixes (stage 3)",
     "Some of the Orbit Lab's numbers were replaced by JPL's. Planet surface gravity is now JPL's value at the equator (Jupiter 2.5 g, Saturn 1.1 g, Uranus 0.9 g, Neptune 1.1 g; the old values were up to 14 % lower for Saturn) and the cards say \"at the equator\". The Sun's radius is the IAU 2015 value, 695,700 km (it was 696,000). Each of the six famous comets now takes all its elements from one JPL record (Halley's, Hale-Bopp's, NEOWISE's, Encke's and Swift-Tuttle's from the Small-Body Database, 67P's from Horizons for the same orbit at 2021) instead of a mixture; against Horizons on 1 January 2026 they are closer to the real comet (67P 62 million km before, 1.7 million now). A comet is still one fixed orbit, so it drifts from the real one away from its record's date, and Halley's next return comes out in January 2062 on this orbit (JPL's prediction is 28 July 2061). The Simple note now says how far Simple is from JPL (worst: Saturn 8 million km by 2100). About lists the new sources."},
    {"0.10.5", "Orbit Lab text fixes (stage 2)",
     "The Orbit Lab's texts were corrected after the check. Dwarf planets and comets now say that they move on a simple orbit fixed at one date, so they drift from their real places the further the date is from it; the jump note after 2050 about extrapolation is only for Simple (Real says its errors were measured for 1800 to 1 Jan 2100); the Voyager caption is right for 1977 to 1989; the Oort cloud travel times are worked out from the distances next to them; the habitable zone, frost line and lifetime are marked rough and planet cards no longer say whether they are in the habitable zone; the real Moon's range is written 1800 to 1 Jan 2100; the Venus 2012 tile jumps to the middle of the transit; the no-atmosphere temperature says it assumes 30 % reflected; the Moon card says the phase names between the four main phases are this app's own; the Real note is shorter. No number or orbit changed."},
    {"0.10.5", "Orbit Lab checked for errors", "Orbit Lab checked for errors, fixes to follow; nothing changed yet."},
    {"0.10.4", "The real Moon (Orbit Lab, Real planet physics)",
     "With Planet physics set to Real, Earth's Moon is now the real one: its direction, distance and phase on the date shown come from the lunar theory ELP2000-82B (a 1988 theory), checked against NASA/JPL "
     "Horizons (DE441): at the dates checked between 1800 and 2100 it is within 5.2 km (2.9 arcseconds as seen from the Earth) at worst. Select the Moon to see its phase (new moon, waxing crescent, first quarter and so on) "
     "and how much of it is lit. Earth is now drawn where it really is, a few thousand km off the Earth–Moon balance point, which is what moves smoothly round the Sun. The Moon is still drawn at the "
     "exaggerated distance used for moon orbits, so you can see it. The other moons, and Simple, are unchanged; outside the years 1800 to 2100 the Moon goes back to its simple ellipse. At very fast Time "
     "speeds the Moon's orbit is lit instead of the Moon (as for every moon) and Earth is held at the balance point so it does not flicker. Real is still an option, and Simple is still the default."},
    {"0.10.3", "Real planets start from JPL's data (Orbit Lab)",
     "With Planet physics set to Real, the planets now start from NASA/JPL Horizons' positions and velocities (the DE441 ephemeris) for 1 January 2026 12:00 TDB, instead of the lab's own simplified orbital "
     "elements. Measured against Horizons' tables for 1 January of every year from 1800 to 2100, the worst error of any planet's position is now under 30,000 km (Mercury) with relativity off, and under 8,000 km "
     "for the others; before, the worst errors were millions of km for the inner planets and Jupiter, and over a hundred million km for Saturn, Uranus and Neptune. The lab's calendar dates are treated as TDB (the time scale "
     "of the JPL data) with no conversion from UTC; the note under Planet physics says so, and gives the worst errors. The General relativity (1PN) tick box is now off by default: measured against JPL, the Sun-potential "
     "term gets Mercury's extra turn right (about 43 arcseconds per century) but the motion along the orbit slightly wrong, so it moves every planet's position away from JPL (Mercury, worst case 1800 to 2100: "
     "70,045 km with it, 29,231 km without). The box is still there, and a setting you saved earlier is kept (press Defaults once to put it off). Real is still an option, and Simple is still the default, "
     "until it has been tried on a PC. Still left out: moons, dwarf planets (Pluto is not in) and asteroids pulling on the planets. Simple is unchanged."},
    {"0.10.2", "Phase colours, quantum beats and smooth photon jumps (Atom Lab)",
     "The Atom Lab can now show the phase of the electron's wave. New orientations m = +1, −1 (and ±2, ±3 for d and f) are complex waves, and a new setting, \"Phase colours: Sign / Wheel\", "
     "colours each dot by the phase of the wave with a colour ring (Sign is the old two-colour look and is still the default). New: \"Superposition\", two orbitals at once, such as 1s + 2p. "
     "The cloud then changes in time at the beat frequency (E₂ − E₁)/h: 1s + 2p sloshes back and forth, 1s + 2s breathes, 2s + 2p does not change at all (same energy here; fine structure and the Lamb shift are not modelled). "
     "Real beats are far too fast to see, so the picture is slowed down and the line under it says by how much. Photon jumps are now smooth: in the cloud views the cloud slides from one shape to the other, "
     "and in the Orbit view the electron glides between the circles (only to help the eye: Bohr's model has no path between circles). An emitted photon appears when the jump ends, and an absorbed photon starts the jump when it arrives. "
     "This is the textbook picture of a transition as a mixture of two states, not a calculation of how a photon is absorbed. With Reduced motion on, jumps are instant."},
    {"0.10.1", "General relativity, the Sun's wobble, and two fixes",
     "With Planet physics set to Real there is a new tick box, \"General relativity (1PN)\" (on by default), that adds Einstein's first-order correction to the Sun's pull, so Mercury's "
     "orbit turns about 43 arcseconds per century more, as measured. It is an approximation with the Sun's field only: it leaves out relativity between the planets, the Sun's spin and "
     "shape, and higher orders. Also in Real: the picture is drawn round the true balance point (the barycentre) from the planets' motion, the Sun's own path round it is drawn as a "
     "trail, and \"Exaggerate the Sun's wobble (x50)\" (on by default) draws that path 50 times too big so you can see it (it is labelled exaggerated, not to scale). "
     "Fixed: the Milky Way band no longer stops in the middle of the sky or goes missing when you turn the 3D view (most of it had been dropped), and the two secret themes show "
     "their names once unlocked instead of \"???\"."},
    {"0.10.0", "Planet physics: Simple or Real",
     "A new setting in the Orbit Lab, \"Planet physics\", has two choices. Simple (the default) is what the lab always did: every planet moves alone round the Sun and the planets do "
     "not pull on each other. Real lets the Sun and the eight planets pull on each other with Newtonian gravity, moved by a Wisdom-Holman integrator (a method made for planets). "
     "It leaves out relativity and the pull of moons, dwarf planets and asteroids on the planets, and the picture stays centred on the Sun. The start values are the lab's own simplified "
     "orbital elements for 1 January 2026, not NASA's ephemeris, so far from that date the planets drift away from their true positions (Mercury by about 12 degrees in 1800). "
     "Dates and Time speed work in both. The benchmark always uses Simple."},
    {"0.9.9", "Theme grid, clearer history button, readable nucleus label",
     "The 🎨 Theme menu is now a grid of cards, three in a row, each with its colour dot, its name and a tick on the one in use; the short description of a theme shows when "
     "you hold the mouse over its card, and names are never cut off, even at Extra large text. The nucleus label in the "
     "Atom Lab (for example \"Cadmium-114 · 48p, 66n\") has a thin edge in the page colour, so it can be read over the electron cloud on dark and light themes. "
     "Sources added to About: the papers behind the star masses and the textbooks for the hydrogen-like orbitals."},
    {"0.9.8", "A 90% outline for the electron cloud",
     "A new setting in the Atom Lab's Cloud, Cloud 3D and Slice views, \"90% outline\" (Off by default), draws a thin line round the smallest region where the electron is found "
     "90% of the time. It is not a sphere: for p, d and f orbitals it follows the real shape (dumbbells, rings, clovers), and an orbital with nodes can have several surfaces. "
     "In Slice it is the line where the cut plane crosses that surface, in Cloud it is the region's shadow seen from the side, and in Cloud 3D it is a faint wireframe that turns "
     "with the cloud. It uses the same model as the cloud: one electron, no relativity, hydrogen-like orbitals; atoms with more electrons have different shapes. "
     "The benchmark always has it off."},
    {"0.9.7", "Reset to today keeps your view choices",
     "\"Reset to today\" (and the Today buttons in the Journey card and the Learn window) now sets only the date and time back to today, and clears the selection and Follow. "
     "It no longer changes your zoom or the Moons, Dwarf planets, Comets and Belts switches (it already kept your Time speed). Defaults still puts everything back. "
     "There is also a new roadmap file, ROADMAP.md, in the project, and the Windows download page is set up to say what the version is and how to get past Windows' warning for an app that is not code-signed yet."},
    {"0.9.6", "Quarks readable on light themes",
     "In the Atom Lab's quark zoom, the dark patch that was drawn behind the glowing quarks is gone on the light themes (Light and Paper), where it looked like a dark "
     "smudge. There the quarks, their halos, the gluon field and the sea pairs are drawn in darker, more saturated colours that read on a light page, and the red, green "
     "and blue colour charges are still easy to tell apart. On the dark themes nothing changes: the quarks glow as before. Only the drawing changed, not the physics."},
    {"0.9.5", "A smooth Milky Way, and star masses",
     "Orbit Lab: the Milky Way band in the night sky is now one smooth band with soft edges instead of three overlapping stripes, so there are no joints or "
     "stray grey streaks when you zoom far out; it is in the same place on the sky as before, and with the constellation names on it has its own label, "
     "\"The Milky Way (the plane of our galaxy, simplified)\". The far-away stars now show their mass under the name, in solar masses, taken from published "
     "papers: Alpha Centauri, Sirius, Procyon (each star of the pair), Proxima Centauri, Fomalhaut, Arcturus and Sagittarius A*. Masses worked out "
     "from stellar models say \"from models\", and a very uncertain one (Proxima) shows its range. Stars whose mass could not be checked against a "
     "published source are left without one. A line near the bottom says the masses are published estimates and some are uncertain."},
    {"0.9.4", "A glowing, denser electron cloud",
     "In the Atom Lab's Cloud, Cloud 3D and Slice views the electron cloud is drawn as soft glowing dots instead of tiny hard squares: where the electron is most likely to be "
     "the light adds up and glows, and thin places fade. In Cloud 3D, size and brightness change smoothly with depth. The nodes, where the wave is zero, stay as clear gaps, "
     "and the blue and orange phase colours are kept (on light themes the dots are darker so they show on white). Two new settings, Cloud style (Classic is the old look, "
     "Glow is new) and Cloud density (Normal 12,000 dots, High 24,000, Ultra 48,000), are remembered and put back by Defaults. More dots show the same probability more "
     "smoothly; they do not change the physics: the dots are found in exactly the same way, and the total light of the cloud is the same at every density. The benchmark's "
     "Cloud 3D scene uses Classic and Normal, so its results stay comparable with older ones, and the stress scenes got a Glow, Ultra Cloud 3D scene."},
    {"0.9.3", "Four fixes: benchmark scenes, guide buttons, benchmark input lock, Time speed on Today",
     "1) The Atom Lab benchmark scenes no longer pick up your own Atom Lab settings: they start from the lab's defaults, so \"Bohr 1913\" can no "
     "longer hide the nucleons (earlier results for the nucleus, one-nucleon and biggest-nucleus scenes taken with Bohr 1913 on are not valid). "
     "The result now says which settings each Atom Lab scene used. "
     "2) The two \"What am I even looking at?\" buttons in the Atom Lab panel no longer clash, so the red \"conflicting ID\" message is gone. "
     "3) While the benchmark runs, nothing but Cancel and Esc works: not the panels, the buttons at the top, the keys, nor the picture. "
     "The result says so. The window title also follows the lab that is on screen. "
     "4) \"Reset to today\" and the other Today buttons keep your Time speed; only Defaults puts it back."},
    {"0.9.2", "Alpha label, drop-down text fits",
     "The Orbit Lab's drop-downs (Sizes, Trails, Stars in the background, Constellations) show only the short name in the closed box, so "
     "it is never cut off at big Text sizes; the explanation is under each choice in the open list. "
     "The app is now labelled alpha: features are still being added. Beta will follow when everything planned for 1.0 is in, then the plain release."},
    {"0.9.1", "Sliders that look like sliders",
     "The Time speed and Zoom sliders have a clearly visible track and a bigger, easier-to-grab thumb in every theme, and their values "
     "are written above them instead of under the thumb. The \"Drag rotates the view\" button is no longer cut off."},
    {"0.9", "Keeps running while the window is moved or resized, a checked GPU timer, stress scenes",
     "The simulation and the drawing no longer freeze while you drag the window by its title bar or an edge (Windows pauses the app's own "
     "loop then; the app now draws from the window system's own refresh signal instead). The picture and the panels follow the new size "
     "while you drag, and after you let go nothing jumps or catches up. The Performance card counts the frames drawn during a drag. "
     "The benchmark's GPU time now covers the whole frame's drawing, and the timer is checked with a known load before the test: if it "
     "cannot be trusted, the result says \"not reliable\" instead of a number. A checkbox \"Include stress scenes\" adds two much "
     "heavier scenes (the biggest nucleus, and eight times the belt rocks) after the four normal ones."},
    {"0.8", "Benchmark, history with the updates on top",
     "A Benchmark button in the Performance card (both labs): it runs four fixed scenes (the heaviest Orbit Lab view, the uranium-238 "
     "nucleus, Cloud 3D, one nucleon) with VSync off at the window's size, 2 s of warm-up and 8 s counted each, and shows the average "
     "frame time and fps, the 1 % low, the worst frame, the CPU work and the GPU time (where the driver gives it). The result is "
     "copied to the clipboard and saved as a text file next to the saved pictures, with the graphics card, the OpenGL version and the "
     "window size on top. It gives all your settings back, and Esc stops it. The 🕰 history now lists the app's updates."},
    {"0.7", "fps counter in both labs, the window remembers itself, icon",
     "The frames-per-second counter is now in the Atom Lab too (it was only in the Orbit Lab): top right of the picture, in a "
     "small see-through rounded box, in every theme and text size, and it is drawn in one place for every lab. It is not in "
     "a saved picture. The window opens with the size and place it had last time, and maximised if it was; if the place is no "
     "longer on a screen it opens centred, and it never starts smaller than 800 x 500. F11 turns full screen on and off. "
     "PhysicsLab.exe has an icon (an atom) on the file, in the taskbar and in the window. If the window or OpenGL 3.3 cannot "
     "start, a message box now says what is wrong and suggests updating the graphics driver."},
    {"0.6", "Save image, timeline pictures, cleaner text, small windows",
     "📷 Save image, next to Defaults in both labs, saves exactly the picture you see (the readout included, in every theme, "
     "grey in Monochrome) as a PNG in your Pictures folder (Downloads if that fails), named with the lab and the date and "
     "time. The button says Saved ✓ and the path is shown under it; if saving fails it says so. The Orbit Lab's timeline "
     "now has small step pictures, like the Atom Lab's. Small windows: long button labels wrap instead of being cut off, and the "
     "Settings window and the big periodic table never get bigger than the window."},
    {"0.5", "App-wide features: settings, themes, header, history",
     "Settings: text size (Small, Normal, Large, Extra large), font (Theme default, Serif, Easy to read, "
     "Rounded, Monospace; Atkinson Hyperlegible and Nunito are built in, under the SIL Open Font License), units, motion (Auto "
     "follows Windows' animation setting; Reduced starts the labs paused, makes camera moves jump, stops Cloud 3D spinning), "
     "Reset my progress with two pop-ups, and Developer > Unlock everything. The 🎨 Theme menu has eleven "
     "themes, including the secret Synthwave and Monochrome; the panels, cards, text and pictures all follow it. Everything is "
     "remembered between runs in the app-data folder: the look, units, unlocks and every setting of both labs. A header across "
     "the top has the buttons: 🎨 Theme, 🌐 language (English; Eesti keel is coming), 👤 About (with its "
     "sources), ⚙ Settings and 🕰 history (this list). "
     "Under it, the 💡 Did you know? line with the facts (tap for the next one; it waits while the mouse is on it) "
     "and its hidden 🎁 surprise, and the tabs: 📚 Learn lists every guide and timeline of both labs and opens them in their "
     "lab; 🏆 Achievements shows Neon Nights, Sharp Eyes, Half-Life and Galactic Center, locked until earned, with a "
     "pop-up when one is earned. The \"monochrome\" word in the Atom Lab guide unlocks the Monochrome theme and Sharp Eyes. "
     "Also: the Sun has soft glowing patches that turn with it, antiquark rings in Cinematic add up their light, "
     "and the Atom Lab readout lost faint grey marks it had in light themes."},
    {"0.4", "Atom Lab",
     "The second lab: pick Orbit Lab or Atom Lab at the top of the panel (each keeps its state while the other is shown). "
     "Atom Lab: any nucleus from hydrogen to oganesson with one electron, in four "
     "pictures (Orbit, as Bohr's 1913 circles or with the real nucleus; Cloud; Cloud 3D; Slice), photons that make the electron "
     "jump and build a spectrum, a zoom from the whole atom down to the protons and neutrons and the quarks inside them "
     "(Cinematic, Accurate or Classic quarks), every known nuclide with its real half-life and decay odds, decays happening on "
     "their own with a decay history, the periodic table with element cards and facts, the light spectrum, and the "
     "Temperature card with its little box of atoms. The four Learn guides are here too: What am I even looking at?, Scale, "
     "Weight and the 22-step timeline, whose buttons set the lab up. "
     "The physics works in SI units inside. Cloud dots, glows and gluon particles are drawn as instanced shapes, a few draw calls for thousands. "
     "Units follow Settings (°F, mph, miles). Defaults asks first; Reset starts a fresh atom; "
     "More / Less info is remembered. The Performance card is on the Atom Lab panel too, with the picture's draw calls."},
    {"0.3", "Orbit Lab complete: Learn guides, Settings with metric or imperial, watch a planet turn",
     "New ⚙ Settings: show distances, speeds and temperatures in metric (km, km/s, °C) or imperial (miles, mi/s, °F). "
     "Every line of every facts card follows it, including the km/h and °C inside the notes, and so does the scale bar; "
     "the choice is remembered. The lab still works in metres and seconds inside. The Defaults button now asks before it "
     "puts Orbit Lab's settings back to how they started. "
     "With Stars set to Off, the picture now follows Windows' light or dark theme. "
     "New Learn card with three guides: What am I even looking at? (one page "
     "with a row of headings to jump to, following your 2D/3D and Sizes choice and the live date), Why do we have "
     "seasons? (with the lab's own Earth: day length and noon Sun height from pole to pole, and each planet's lean), "
     "and the Solar System timeline, whose ▶ buttons take the lab there. "
     "New button in the View card: pick a planet and press Slow time to watch it turn; time slows down so one turn "
     "takes about 24 seconds, and the view follows and zooms in on it."},
    {"0.2", "3D view, night sky, belts, spinning planets, Journey and Jump to a date",
     "The planets now really spin, with the real IAU axes and rates: Earth shows its real coastlines, the others a grid of "
     "latitude and longitude, each with a dashed axis and N; Saturn's rings tilt with its axis. The cards say how long a turn "
     "takes and how far the axis leans, the Sun's card how it spins, and Earth's card which season it is. "
     "Journey and dates card: the Journey zooms out from Earth to the centre of our galaxy, and when it ends (or you touch "
     "anything) all your settings and your view come back. Jump to a date puts everything where it really was or will be "
     "from 1800 to 2100 (Greenwich time); a date outside that range, or one that does not exist, now gets a note instead of "
     "a silent jump. "
     "New 3D view (View card): drag to turn it, right-drag or Shift-drag (or the Drag button) to move it, Spin to turn it "
     "slowly. Nearer things look bigger, and dashed lines drop from each planet to the flat plane of the orbits. "
     "Behind everything is the real night sky: the 2,874 brightest stars at their true directions and colours, the band "
     "of the Milky Way, and (Sky card) constellation lines and names. Zoom far out to find the far-away things at their "
     "real distance: the nearest stars, the Pleiades, the Orion Nebula, the galactic centre and the two Voyagers, all "
     "clickable and in Go to. "
     "The asteroid belt (with its Kirkwood gaps), Jupiter's Trojans, the Kuiper belt and the Oort cloud are here, "
     "with 6,000 stand-in rocks (a fixed random seed, so they are the same every time). Each belt is drawn with a single draw call, and the rocks are only moved when time moves. "
     "Click a belt's name or its Go to tile for its facts; a Belts and Oort cloud switch is under Solar System extras. "
     "A new Performance card shows how long each frame takes (the last one, the average and the worst of the last "
     "240 frames, with a small graph) and how much of it is the app's own work. A VSync switch turns off waiting for "
     "the screen, so you can see the real headroom instead of the monitor's refresh rate. Each Windows build is now "
     "also published on one fixed GitHub Release page, tagged \"latest\"."},
    {"0.1", "First version: Orbit Lab (pilot)",
     "The first version of Physics Lab for Windows, written in C++ with SDL3, OpenGL 3.3 and Dear ImGui. "
     "It has Orbit Lab only: the real Solar System where everything is today (the eight planets, the five dwarf planets, "
     "fourteen moons and six famous comets), with leapfrog steps "
     "around the Sun from NASA/JPL orbital elements. Play and pause, Time speed, zoom with the mouse wheel or the slider, drag "
     "to move the view, click anything for its facts, Go to tiles, the habitable zone, distance rings, trails, the scale bar "
     "and the barycentre. Not here yet: the 3D view, the night sky and far-away things, the belts and Oort cloud, spinning "
     "planets, Journey, Jump to a date, Learn and the other labs."},
};


}  // namespace

// The stage word shown after the version number (change this one word to move from alpha to beta, then "" for 1.0)
constexpr const char* kStage = "alpha";

const char* appVersion() { return PL_VERSION; }
std::string appVersionLabel() { return *kStage ? std::string(PL_VERSION) + " " + kStage : std::string(PL_VERSION); }

const NativeEntry* nativeHistory(int& count) {
    count = int(sizeof kNative / sizeof kNative[0]);
    static NativeEntry out[sizeof kNative / sizeof kNative[0]];
    for (int i = 0; i < count; i++) out[i] = {kNative[i].version, kNative[i].title, kNative[i].text};
    return out;
}

}  // namespace ui
