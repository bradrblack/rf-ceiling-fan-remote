// ESP32-C3 + CC1101 dongle case
// Companion to hw/esp32c3_cc1101_dongle.kicad_pcb
//
// Board/hole dimensions below are read directly from hw/pcb.py (MEASURED).
// Component heights, the reset-button tunnel position, and the LED/cable
// dimensions are ESTIMATES based on typical ESP32-C3 Super Mini layout --
// verify against your actual modules before printing, especially
// button_hole_x/y and the pin/hook dimensions (test-fit before committing).
//
// Assembly: no screws hold the PCB itself. The PCB sits on 4 pads and is
// located by pins that poke up through its mounting holes. The top piece
// drops straight down onto the tray (skirts nest inside the tray's own
// walls with a snug fit_clearance) -- NOT a horizontal slide-in. A plain
// washer-shaped hook post hangs from the ceiling over each pin, clearing
// it on the way down and coming to rest just above the board once seated;
// it holds the PCB down passively, the same way a washer under a screw
// head does, simply because the rigid top above it isn't going anywhere.
// That "isn't going anywhere" is doing real work: unlike a slide-to-lock
// mechanism, a snug friction fit alone does not resist being pulled
// straight back up, and a slotted/keyed hook can't either without either
// colliding with the pin during any sideways lock motion or requiring a
// wider-headed pin than this design uses (the geometry was checked both
// ways). What actually keeps the case shut is a snap-fit latch: two
// flexible cantilever tabs per long wall (four total, spread toward each
// end rather than one centered pair, so both ends of the ceiling panel
// get held down instead of just the middle), each engaging a ramped catch
// ridge on the tray wall as the top drops down -- ramped on its entry
// side for a smooth push going in, flat on its catch side so pulling the
// top back off means deliberately flexing the tab, not just reversing
// the same smooth path (a locking screw was tried first, but the
// CC1101's antenna coils well past the board's far edge, right where the
// screw boss needed to sit). An earlier version of this also tried
// adding a raised nub to each tab as a second, more visible catch
// feature; it's gone now -- on one wall it collided with the tray's own
// solid material (not flexible, so nothing could actually get past it),
// and on the other it pointed away from the ridge and did nothing, so it
// wasn't adding a real second catch either way. The catch ridge and the
// tab's own thinned, flexible section are the visible features on the
// tray and the lid respectively (see skirt_wall(), snap_ridge_low/high).
// Everything else here is fit
// and alignment, not a lock. The tray's open (cable) end has a short
// fixed stub wall covering its bottom third; the top's leading wall picks
// up right where that stops and covers the rest, closing off the whole
// opening between them except for a recessed USB-C port opening. Since
// the top's leading wall spans the tray's full width (Y=0 to outer_h) and
// the tray's own long walls run its full length (X=0 to outer_w), their
// footprints would otherwise overlap in a solid block at each of the two
// near corners, above the stub wall's height -- two separate parts
// trying to occupy the same physical space, which would make it
// physically impossible for the top to ever seat on the tray. The tray's
// long walls are notched clear of that block at each near corner so the
// leading wall actually has
// somewhere to drop into (see the notch cut in bottom_tray()).
// The ceiling and all 3 closed tray walls (both long walls and the far
// wall -- everywhere except the floor) carry a plain dimple-grid surface
// texture (see dot_field()) -- a fully deterministic grid of shallow
// round dimples, no randomness, reused on every surface via a simple
// translate/rotate rather than one-off geometry per side. On the
// ceiling, every other grid column is a real through-hole instead of a
// dimple, which is what actually vents the case and lets the onboard LED
// show through.
// Exterior edges carry a cosmetic rounded fillet (not a chamfer): the
// ceiling's top edge and corners, and the tray floor's bottom edge and
// corners -- the two outermost surfaces of the assembled case, each
// exposed on its own with nothing else to align to. The tray walls'
// TOP edges are deliberately left plain/square, unlike those two: the
// top piece's ceiling sits flush on top of the walls at their full,
// unrounded footprint, so a wall-edge bevel there would recede inward
// right where the ceiling's flush edge continues straight up outside of
// it, leaving a visible groove at the seam instead of one flat face.
// The vertical corners round off independently of that -- all 4 (see
// corner_round_cut_full()), tapering from a fat corner_fillet radius in
// the middle back down to match whatever's at each end: the floor's own
// dome at the bottom everywhere, and at the top either back to square
// (the far end, matching the plain wall top edge and the ceiling's own
// square underside above it) or straight up at full radius into the
// top's leading wall, which continues the taper down to square itself
// (the near/USB end, where the top's leading wall -- not the tray's
// short stub wall -- is what's actually exposed for most of the height).
//
// Render in OpenSCAD, or from the command line:
//   openscad -o tray.stl   -D 'part="tray"' fan_dongle_case.scad
//   openscad -o top.stl    -D 'part="top"'  fan_dongle_case.scad
//   openscad -o preview.png -D 'part="both"' --imgsize=1200,900 --camera=34,12,10,55,0,25,220 fan_dongle_case.scad
//   openscad -o fit.png     -D 'part="fit"'  --imgsize=1200,900 --camera=34,12,10,55,0,25,220 fan_dongle_case.scad
//     ("fit" = tray + a translucent PCB/module reference, no top -- for checking fit; not a real part)
//   openscad -o snap_detail.png -D 'part="detail"' --imgsize=1200,900 --camera=34,12,10,55,0,25,220 fan_dongle_case.scad
//     ("detail" = tray+top assembled, tray rendered translucent so the catch ridges show through
//      the wall right where the top's tabs sit; not a printable part)

// ==========================================
// MEASURED -- from hw/pcb.py, local origin at board bottom-left corner
// ==========================================
board_w = 60;            // board X (kicad Edge.Cuts: 50-110)
board_h = 24;             // board Y (kicad Edge.Cuts: 38-62)
board_t = 1.6;             // standard PCB thickness

// 4x M2.5 clearance mounting holes, (x,y) local to board bottom-left corner
mount_holes = [
  [2.5, 3.5], [2.5, 20.5], [57.5, 3.5], [57.5, 20.5]
];
mount_hole_d = 2.5;

// ESP32-C3 Super Mini socket/footprint center + silkscreen outline (local)
esp32_center = [15, 12];
// Official ESP32-C3 SuperMini board size (per Espressif-adjacent vendor specs,
// e.g. espboards.dev/mischianti.org): 22.5 x 18mm. Replaces an earlier
// guess that had the dimensions both backwards and oversized on the Y
// axis -- board_h is 24mm, so this now leaves real clearance on the
// module's width instead of nearly spanning the full board height.
esp32_outline = [22.5, 18];  // module footprint envelope, X x Y

// CC1101 socket (the 2x4 header pcb.py actually placed) center -- exact
// KiCad footprint position. The real breakout module that plugs into it
// is a separate, bigger board -- see cc1101_module_outline below.
cc1101_center = [48, 12];

// ==========================================
// ESTIMATED -- verify against real hardware before printing
// ==========================================
// Modules are soldered directly (no header sockets), so components sit
// close to the board -- this is just body height + a safety margin, not
// header-stack height. Increase if your modules are taller than this.
component_clearance = 8;

// The real HW-862 CC1101 breakout module that plugs into the socket
// above is physically much bigger than the header footprint, and isn't
// centered on it -- it extends further toward the board's far edge, in
// the same direction the antenna exits. Measured with a ruler against a
// real HW-862 module (on a different board using the same part --
// github.com/bradrblack/rf-ceiling-fan-remote) -- module body came out
// to roughly 28-34 x 15-18mm across two separate measurement passes on
// the photo (~20% spread from measurement/rounding error), with its
// center about 11-13mm further from the socket toward the far edge.
// Rounded up a bit past the high end of that spread for margin. NOT yet
// confirmed against the actual dongle board/socket orientation --
// verify with calipers before printing.
cc1101_module_offset = [12, 0];
cc1101_module_outline = [30, 17];
cc1101_module_center = [cc1101_center[0] + cc1101_module_offset[0],
                          cc1101_center[1] + cc1101_module_offset[1]];

// The ESP32 module is raised above the main board on pin headers -- this
// is how far above the main board its own PCB's top surface sits, needed
// below both for the USB-C port (a component mounted on that PCB) and
// further down for the BOOT button guide tunnel (see there for how a
// version of this sized off the main board directly went wrong).
esp32_pcb_top_above_board = 3.74;   // measured: standard 2.54mm pin header
                                       // + 1.2mm ESP32 PCB thickness (from a
                                       // detailed reference model of the
                                       // real board, replacing an earlier
                                       // ~1mm estimate)

// USB-C port opening, flush through the leading-edge wall (no recessed
// pocket around it -- see the through-opening comment further down for
// why): modules face the board's short left edge (X=0). Rather than a
// soldered/attached cable routed through an open slot, this assumes the
// Super Mini's own onboard USB-C receptacle sits right at this opening
// -- you plug a cable in from outside whenever needed, nothing threads
// through during assembly.
//
// The USB-C connector itself sits flush on top of the ESP32 PCB (no
// gap) and is 3.2mm tall, per a detailed reference model of the real
// board -- its own span is esp32_pcb_top_above_board to +3.2mm above
// the main board. usb_port_w is cross-checked against that same
// reference (connector is 9mm wide) with a small margin. The Y position
// still assumes the connector centers on the module the same way
// usb_port_y assumes (= ESP32 module Y-center) -- not directly measured.
//
// The opening itself is a plain closed rectangle, NOT open at the top
// into the ceiling: an earlier version cut all the way up through the
// case's own top surface on the reasoning that there was no structural
// need to leave material there, but that let you see straight through
// to the ceiling's underside/seam through the opening, which looks
// wrong and isn't needed -- the plug only has to reach the connector,
// not the roof of the case. See usb_port_window_top/usb_port_h further
// down (after ceiling_bot_z exists) for how the opening is kept clear
// of that boundary instead.
usb_port_y = 12;                    // port center, board Y (= ESP32 module Y-center)
usb_connector_height = 3.2;         // measured: real USB-C connector height, PCB to top
// Sized to the OUTER edge of the old recessed pocket (removed above),
// not just the bare connector + a small margin: that recess used to be
// 2mm wider/taller than the through-opening on each side specifically
// to fit a cable's plug housing, and simply deleting the recess without
// carrying that size into the through-opening itself would have left
// the actual hole too tight for a real plug to pass through.
usb_port_w = 14;                    // through-opening width (Y) -- was 10 (bare connector 9mm + small margin),
                                       // now the old recess's outer width (10 + 2x2mm margin)
usb_port_h = 6;                     // through-opening height (Z) -- what the plug itself needs (measured), not the bare connector

// Reset-button guide tunnel: button is top-mounted (pressed straight
// down). A tube hangs from the ceiling down toward the board, so a
// toothpick/pin is guided straight to the button instead of just poking
// through a thin hole in the ceiling with nothing below to aim it.
// Position is now measured off the real board, from the center of the
// near mounting hole (2.5, 3.5): the BOOT button is 11mm from that
// center along the board's long (X) direction, and 6mm from it along
// the short (Y) direction.
button_hole_d = 3;      // bore diameter -- fits a toothpick/paperclip/pin
button_hole_x = 13.5;   // local X -- mount_holes' (2.5, 3.5) + 11 in X
button_hole_y = 9.5;    // local Y -- mount_holes' (2.5, 3.5) + 6 in Y
button_tube_wall = 1;                // tube wall thickness
// The BOOT button sits on top of the ESP32 module's own PCB, not
// directly on the main board -- the ESP32 is raised on pin headers, and
// esp32_pcb_top_above_board (measured) is how far above the main board
// its own PCB surface sits. Sizing the tube's stop height off the main
// board alone (as an earlier pass here did, with just a flat 2mm gap)
// drove the tube straight through the ESP32 PCB itself, since that PCB's
// own top surface is well above that 2mm mark (see esp32_pcb_top_above_board,
// defined above alongside the USB port section, which also needs it).
// button_height_above_esp32_pcb (the button component's own height above
// its PCB) is still a guess -- verify against the actual button before
// printing.
button_height_above_esp32_pcb = 2;  // guess -- typical tact-switch height
button_tube_gap_above_board = 1;    // gap between the tube's open bottom
                                       // and the button's top surface

// Cosmetic rounded fillet around the top face's outer edge (all 4 sides).
// Kept well under wall_t/2 so the rounding stays within the wall's own
// thickness instead of poking out the inner face.
top_fillet = 0.8;

// Vertical-corner fillet radius (see corner_round_cut()) -- deliberately
// bigger than top_fillet. That radius blends into a top/bottom edge it's
// only grazing across, but a vertical corner runs the whole wall height,
// and at 0.8mm it read as basically still-sharp over that long a run.
// Bounded by the 2mm-thick wall corner itself (a 2x2mm post in cross
// section) rather than by wall_t/2 the way the horizontal fillets are --
// 1.4mm leaves 0.6mm of flat wall beyond the fillet's tangent point
// before reaching the wall's own inner corner, still comfortably solid.
corner_fillet = 1.4;

// No external LED -- the onboard LED (GPIO8/GND) shows through the vent
// holes in the ceiling instead of needing its own dedicated hole.
// Surface texture: a plain, fully deterministic grid of round dimples
// recessed into the surface (no randomness at all, unlike the earlier
// PCB-trace attempt) -- one generator (dot_field()) reused via simple
// translate/rotate on every exposed side: the ceiling and all 3 closed
// tray walls, everything except the floor. On the ceiling, every other
// column of the grid is a real through-hole instead of a dimple, which
// is what actually vents the case and lets the onboard LED show through.
dot_spacing = 5;           // grid pitch dimples/vents are placed on
dot_r = 1.8;                  // dimple cutter sphere radius (bigger than the visible dimple itself)
dot_depth = 0.35;           // how far each dimple sinks below the surrounding surface
vent_hole_d = 3;             // ceiling vent through-hole diameter (airflow + LED visibility)
vent_col_stride = 3;        // ceiling: every Nth grid column, and every other row within it,
                              // becomes a vent instead of a dimple
texture_margin = 6;        // keep the whole textured field at least this far from the outer edge
vent_exclude_r = 6;      // keep dimples/vents this far from the button tunnel

// Alignment pins (replace the old screw bosses) + PCB retention washers.
// UNVALIDATED -- these are first-pass guesses for a friction/clearance fit
// and need a test print before trusting them.
pin_d = 2.2;               // pin diameter -- clearance fit through the board's 2.5mm holes
pin_h = 4;                   // pin height above the pad top (must clear board_t + hook post + slack)
hook_gap_above_board = 0.3; // gap between the board's top surface and the hook post's bottom
hook_od = 5.5;               // washer outer diameter -- kept small so it clears the skirts
                              // at this design's mount-hole Y positions; re-check if those move
hook_pin_clearance = 0.6;   // extra width around the pin in the washer's hole, for a free drop-through

// Near-end stub wall: the tray's near (cable) end only has a short fixed
// wall up to end_wall_frac of the tray's height. The top's leading wall
// picks up from exactly where this stops and covers the rest, so together
// they seal the whole opening (minus the cable slot) once assembled --
// rather than the top being the only thing closing it.
end_wall_frac = 1 / 3;

// far_end_extra used to exist for a locking screw boss past the board's
// far edge -- dropped (see snap-fit below) because the CC1101's antenna
// coils well past that point, right where the boss would have been. Kept
// as a general buffer past the board's far edge.
//
// antenna_reach: measured against a ruler in a real-board photo (a CC1101
// MW-862 breakout on a different board using the same module) -- from the
// antenna's solder point, right at the module's own edge, up through the
// full helical coil to its tip, about 38mm. NOT yet confirmed on the
// actual dongle board/socket orientation -- the module could be seated
// either way in its socket, which would send the antenna in the opposite
// X direction instead; re-check once the real dongle board is in hand.
// far_end_extra is sized to clear the MODULE's far edge (not the
// socket's -- see cc1101_module_outline/cc1101_module_center above, the
// module overhangs the socket by another 10+mm on its own) plus the
// antenna reach, plus a few mm of margin.
antenna_reach = 38;
far_end_extra = (cc1101_module_center[0] + cc1101_module_outline[0] / 2 - board_w) + antenna_reach + 4;

// Snap-fit latch: what actually keeps the case shut, now that a screw in
// the far-end gap isn't an option. TWO flexible cantilever tabs per long
// wall (four total) -- spread toward each end of the board rather than
// one centered tab, so both ends of the ceiling get held down instead of
// just the middle (a single centered tab left the near/far ends of the
// panel with nothing catching them, free to flex or lift even with the
// middle snapped shut). Each tab engages a ramped ridge on the tray
// wall -- ramped on the entry (bottom) side for a smooth push as the top
// drops down, flat on the catch (top) side so pulling the top back off
// requires deliberately flexing the tab back out (the ramp gives the
// insertion push a mechanical advantage that the flat catch face doesn't
// give a straight pull, which is what makes the two directions genuinely
// different, not just the same path in reverse). UNVALIDATED -- first
// pass, needs a test print to confirm the tabs actually flex without
// snapping in your printed material (PLA especially is brittle; may need
// wider/thicker tabs, or a more flexible filament, to survive repeated
// opening).
snap_tab_w = 8;               // tab width
snap_tab_thickness = 1;       // tab thickness (thinner than wall_t, for flex)
snap_bump_protrusion = 0.6;  // how far the catch ridge sticks out from the tray wall
snap_engage_h = 3;             // ridge/catch engagement height

// ==========================================
// Case construction parameters
// ==========================================
wall_t = 2;               // wall thickness
floor_t = 2;               // tray floor thickness
board_margin = 2;        // clearance around board edges inside the case
standoff_h = 5;            // pad height, floor-to-PCB-bottom (solder joint clearance)
boss_d = 5;                // mounting pad outer diameter
fit_clearance = 0.3;     // FDM friction/slide clearance

// Derived
cavity_w = board_w + 2 * board_margin;
cavity_h = board_h + 2 * board_margin;
outer_w = cavity_w + 2 * wall_t + far_end_extra;
outer_h = cavity_h + 2 * wall_t;
tray_wall_h = standoff_h + board_t + component_clearance; // floor-top to ceiling-bottom
skirt_bot_z = floor_t + standoff_h + board_t; // where the top's skirts/leading wall start, just above the board
ceiling_bot_z = floor_t + tray_wall_h;
ceiling_top_z = ceiling_bot_z + wall_t;
button_tube_od = button_hole_d + 2 * button_tube_wall;
button_tube_bot_z = skirt_bot_z + esp32_pcb_top_above_board
                     + button_height_above_esp32_pcb + button_tube_gap_above_board;
end_wall_top_z = floor_t + tray_wall_h * end_wall_frac;

// The USB port opening's top edge is anchored exactly at ceiling_bot_z
// (where the leading wall ends and the solid ceiling begins) instead of
// being centered on the real connector: an earlier version centered the
// window on the connector, which -- combined with usb_port_h -- pushed
// the top edge past that boundary, letting you see through to the
// ceiling's underside/seam through the opening. usb_port_margin_below_ceiling
// is 0 (flush with the boundary, not into it) -- kept as a named
// quantity rather than inlined so it's obvious where to add margin back
// if a flush top edge ever turns out to read as a seam line in practice.
// The window (usb_port_h tall) still comfortably contains the real
// connector's span (esp32_pcb_top_above_board to +usb_connector_height)
// -- checked below with assert() rather than just assumed, since this
// exact kind of boundary math has been wrong here twice already this
// session.
usb_port_margin_below_ceiling = 0;
usb_port_window_top = ceiling_bot_z - usb_port_margin_below_ceiling;
usb_port_window_bot = usb_port_window_top - usb_port_h;
usb_port_z_above_board = usb_port_window_bot - skirt_bot_z + usb_port_h / 2;

assert(usb_port_window_bot <= skirt_bot_z + esp32_pcb_top_above_board,
        "USB port window bottom doesn't clear the real connector's bottom edge");
assert(usb_port_window_top >= skirt_bot_z + esp32_pcb_top_above_board + usb_connector_height,
        "USB port window top doesn't clear the real connector's top edge");

// Board-local -> case-local (board sits centered in the cavity, offset by
// wall_t + board_margin from the case's own bottom-left corner)
board_origin = [wall_t + board_margin, wall_t + board_margin];

function b2c(p) = [p[0] + board_origin[0], p[1] + board_origin[1]];

// snap-fit tab/ridge X positions: spread toward each end of the board
// (20%/80% along its length) rather than one centered point, clear of the
// hook washers at the board's corners. Z is centered in the skirt's
// engagement zone (same spot the old rail used to sit).
//
// A third pair sits out in the far-end extension (module + antenna
// clearance, added later -- see far_end_extra): that extension is now
// ~57mm, more than the board's own 60mm length, and originally had zero
// clips of its own -- the far ~59mm of a 125mm case (from the far board
// edge all the way to the closed end) was relying entirely on the two
// board-region clips to hold it shut. Positioned 60% of the way out
// into that extension: clear of the CC1101 module's real footprint
// (cc1101_module_center/outline, world X up to ~79) with room to
// spare, and well short of the far wall's own corner rounding.
snap_x_list = [board_origin[0] + board_w * 0.2, board_origin[0] + board_w * 0.8,
                 board_origin[0] + board_w + far_end_extra * 0.6];
snap_z_lo = skirt_bot_z + (ceiling_bot_z - skirt_bot_z) / 2 - snap_engage_h / 2;

// Flat-bottomed box with a rounded fillet around its BOTTOM outer edge
// (all 4 sides, including corners) -- same minkowski-dome technique as
// rounded_top(), just mirrored to round the bottom instead of the top.
// Used for the tray floor, so the case doesn't have a sharp edge where it
// sits on a table either.
module rounded_bottom(w, h, t, r) {
  union() {
    translate([0, 0, r])
      linear_extrude(t - r)
        square([w, h]);
    intersection() {
      translate([0, 0, r])
        minkowski() {
          translate([r, r, 0])
            linear_extrude(0.02)
              square([w - 2 * r, h - 2 * r]);
          sphere(r = r, $fn = 24);
        }
      translate([-1, -1, -1])
        cube([w + 2, h + 2, r + 1]);
    }
  }
}

// Snap-fit catch ridge, wedge-shaped: full snap_bump_protrusion at the
// bottom (snap_z_lo), tapering to ~flush at the top (snap_z_lo +
// snap_engage_h). A tab sliding down past this ramps outward gradually
// (easy); pulling back up hits the flat, full-width bottom face
// immediately (hard) -- that asymmetry is what makes it a catch and not
// just a bump. Called once per position in snap_x_list.
module snap_ridge_low(x) {
  // attaches to the low wall's inner face (Y=wall_t), protrudes toward +Y
  translate([x - snap_tab_w / 2 - 1, wall_t, snap_z_lo])
    hull() {
      cube([snap_tab_w + 2, snap_bump_protrusion, 0.1]);
      translate([0, 0, snap_engage_h])
        cube([snap_tab_w + 2, 0.1, 0.1]);
    }
}
module snap_ridge_high(x) {
  // attaches to the high wall's inner face (Y=outer_h-wall_t), protrudes toward -Y
  translate([x - snap_tab_w / 2 - 1, outer_h - wall_t - snap_bump_protrusion, snap_z_lo])
    hull() {
      cube([snap_tab_w + 2, snap_bump_protrusion, 0.1]);
      translate([0, snap_bump_protrusion - 0.1, snap_engage_h])
        cube([snap_tab_w + 2, 0.1, 0.1]);
    }
}

// One skirt wall, nested just inside the tray's own wall, thinned to a
// flexible snap-fit tab at every position in snap_x_list. front_y is this
// skirt's own outward (ridge-facing) face; dir is +1 if the skirt's
// material sits at Y >= front_y (the near/low wall) or -1 if it sits at
// Y <= front_y (the far/high wall). One module builds both walls from
// mirrored parameters instead of two hand-written copies -- those two
// copies had drifted apart (a sign error left the far wall's tabs half
// the thickness of the near wall's) and are gone now that both walls
// come from the same math. The tab itself has no separate raised nub --
// an earlier attempt at one either collided with the tray's own solid
// wall (impossible to flex past, since the wall isn't the flexible part)
// or, on the other wall, pointed away from the ridge and did nothing.
// The catch is the ridge alone (snap_ridge_low/high) pressing against
// this thinned, flexible section -- ramped on its entry side for a smooth
// push going in, flat on its catch side so pulling back out means
// deliberately flexing the tab, not just reversing the same smooth path.
module skirt_wall(front_y, dir) {
  difference() {
    // Stops at outer_w - wall_t, not outer_w: the tray's closed far-end
    // wall already occupies that last wall_t of length across the full
    // width (see bottom_tray()), so running the skirt all the way to
    // outer_w would drive it straight into that wall -- a real
    // tray/lid collision, not just a cosmetic overlap, caught by
    // intersecting the two rendered parts and finding non-empty volume
    // right at the far corner.
    translate([0, dir > 0 ? front_y : front_y - wall_t, skirt_bot_z])
      cube([outer_w - wall_t, wall_t, ceiling_bot_z - skirt_bot_z]);
    for (x = snap_x_list)
      translate([x - snap_tab_w / 2,
                 dir > 0 ? front_y + snap_tab_thickness : front_y - wall_t - 0.5,
                 skirt_bot_z - 0.5])
        cube([snap_tab_w, wall_t - snap_tab_thickness + 0.5, ceiling_bot_z - skirt_bot_z + 1]);
  }
}

// Rounds one exterior vertical corner where two walls meet at a sharp
// 90-degree edge, into a quarter-cylinder fillet of radius r -- the same
// idea as the horizontal-edge fillets elsewhere in this file (a small
// notch cut away to leave a rounded transition), just applied to a
// vertical edge instead of a horizontal one. (px,py) is the sharp
// corner's own (X,Y); (dx,dy) point from that corner back toward the
// case's interior (+1/-1 each), so the fillet's tangent center lands r
// inward along both walls. Returns the small notch sliver (square minus
// quarter-circle) to SUBTRACT from the wall union -- not the fillet
// material itself.
module corner_round_cut(px, py, dx, dy, r, z0, h) {
  cx = px + dx * r;
  cy = py + dy * r;
  translate([0, 0, z0])
    difference() {
      translate([min(px, cx), min(py, cy), 0])
        cube([abs(px - cx), abs(py - cy), h]);
      translate([cx, cy, -0.5])
        cylinder(r = r, h = h + 1);
    }
}

// Tangent-preserving taper between two different corner radii on the same
// vertical corner (r1 at z1 to r2 at z2) -- splices a fat corner_fillet
// straight run into the thin top_fillet radius that the floor's bottom
// dome and the walls' own top-edge fillet already use, without a step at
// either end. A plain conic frustum can't do this: its axis stays fixed,
// so as it necks down from r1 to r2 it pulls away from both wall faces
// instead of staying tangent to them. Building it from the hull of two
// SPHERES instead fixes that -- each sphere is centered at the tangent
// point for its own radius (r inward from the corner along both walls),
// so each one individually touches both wall faces, and the hull of two
// shapes that both touch a given plane also touches that plane everywhere
// along its length. The envelope being subtracted from also has to taper
// (hull of the two matching square footprints) -- an untapered envelope
// sized for the bigger radius would leave a flat, unrounded shelf of
// leftover material near the smaller end.
module corner_taper_cut(px, py, dx, dy, r1, z1, r2, z2) {
  cx1 = px + dx * r1; cy1 = py + dy * r1;
  cx2 = px + dx * r2; cy2 = py + dy * r2;
  difference() {
    hull() {
      translate([min(px, cx1), min(py, cy1), z1]) cube([abs(px - cx1), abs(py - cy1), 0.01]);
      translate([min(px, cx2), min(py, cy2), z2]) cube([abs(px - cx2), abs(py - cy2), 0.01]);
    }
    hull() {
      translate([cx1, cy1, z1]) sphere(r = r1);
      translate([cx2, cy2, z2]) sphere(r = r2);
    }
  }
}

// Full-height vertical corner fillet, blending a fat corner_fillet radius
// (see corner_fillet's own comment -- a corner needs a bigger radius than
// a grazing edge to actually read as rounded) down to the thin top_fillet
// radius at the bottom via corner_taper_cut(), so it meets the floor's own
// bottom dome at an exact, tangent radius match instead of colliding with
// it at a mismatched one:
// - bottom: starts at top_fillet, right at z0 -- exactly where
//   rounded_bottom()'s dome levels out to its full-radius equator, same
//   radius, same tangent point, so the two meet with nothing sharp or
//   mismatched between them (no taper needed here, they already agree).
// - middle: tapers up to the full corner_fillet radius for the rest of
//   the run.
// top_mode selects how the run ends at z_top:
// - "fillet" (the closed/far-end corners, z_top = the exposed top rim):
//   tapers back down to EXACTLY top_fillet radius, right at z_top -- no
//   cap needed, since a corner_taper_cut() ending at r2=top_fillet already
//   produces the identical cross-section a plain corner_round_cut(r=
//   top_fillet) would at that same height (its endpoint IS just that
//   circle). top_fillet, not square, because the ceiling above isn't
//   actually square there either: rounded_top() only rounds the LAST
//   top_fillet of the ceiling's own 2mm thickness at its corners, so the
//   bottom ~1.2mm of the ceiling's corner is a plain top_fillet-radius
//   post cut at that same exact radius (see the matching
//   corner_round_cut() cut through the ceiling in top_slide()) --
//   matching cross-sections on both sides of the seam is what makes the
//   two parts actually meet instead of showing a step or pinch there.
// - "abrupt" (the near-end stub-wall corners, z_top = end_wall_top_z): just
//   runs the fat radius straight up to z_top with no taper-back -- that
//   height is the stub wall's own top edge, already left square and
//   hidden under the top's leading wall (see bottom_tray()), and the top's
//   own leading wall picks up at this same full radius and does its own
//   taper down to top_fillet higher up (see top_slide()), so there's
//   nothing for this end to blend into here.
module corner_round_cut_full(px, py, dx, dy, r, z0, z_top, top_mode = "fillet") {
  taper_h = min(r, (z_top - z0) / 2);
  z_taper1_end = z0 + taper_h;
  corner_taper_cut(px, py, dx, dy, top_fillet, z0, r, z_taper1_end);
  if (top_mode == "abrupt") {
    if (z_top > z_taper1_end)
      corner_round_cut(px, py, dx, dy, r, z_taper1_end, z_top - z_taper1_end);
  } else {
    z_taper2_start = max(z_taper1_end, z_top - taper_h);
    if (z_taper2_start > z_taper1_end)
      corner_round_cut(px, py, dx, dy, r, z_taper1_end, z_taper2_start - z_taper1_end);
    corner_taper_cut(px, py, dx, dy, r, z_taper2_start, top_fillet, z_top);
  }
}

$fn = 48; // smooth cylinders

part = "both"; // overridden via -D 'part="tray"' / "top" / "both" for preview

module mount_hole_positions() {
  for (h = mount_holes) translate(b2c(h)) children();
}

// Open-ended U-channel: floor + 2 long side walls + 1 closed far-end wall
// (at X=outer_w). The near end (X=0, cable side) only has a short stub
// wall up to end_wall_top_z -- the top's leading wall covers the rest
// once dropped into place, and the cable exits through the gap either way.
module bottom_tray() {
  difference() {
  union() {
    // floor, with a rounded fillet around its bottom outer edge too --
    // no sharp edge where the case sits on a table
    rounded_bottom(outer_w, outer_h, floor_t, top_fillet);
    // long side walls, full tray height, spanning the whole length. No
    // top-edge bevel here (unlike the floor's bottom edge or the top
    // piece's own top edge) -- the top piece's ceiling sits directly on
    // top of these walls at their full, unrounded outer footprint (see
    // rounded_top() in top_slide()), so a bevel here would recede inward
    // right where the ceiling's flush edge continues straight up outside
    // of it, leaving a visible groove at the seam instead of one flat
    // continuous face. Only the vertical corners round off (see below).
    translate([0, 0, floor_t])
      cube([outer_w, wall_t, tray_wall_h]);
    translate([0, outer_h - wall_t, floor_t])
      cube([outer_w, wall_t, tray_wall_h]);
    // closed far-end wall, same reasoning -- no top-edge bevel
    translate([outer_w - wall_t, 0, floor_t])
      cube([wall_t, outer_h, tray_wall_h]);
    // near-end stub wall -- closes the bottom end_wall_frac of the open
    // (cable) end. The top's leading wall picks up right where this
    // stops (see top_slide()), so the two seal the opening together. Its
    // own TOP edge is left square -- that's an internal seam under the
    // top's leading wall once assembled, never actually exposed. Its two
    // VERTICAL corners (where it meets the long walls) are a different
    // edge entirely and stay exposed for this wall's whole height
    // regardless of what the lid covers above it -- see the corner
    // rounding below, which treats them the same as the far corners.
    translate([0, 0, floor_t])
      cube([wall_t, outer_h, end_wall_top_z - floor_t]);
    // snap-fit catch ridges, two per long wall (snap_z_lo is already an
    // absolute Z, no extra offset needed)
    for (x = snap_x_list) {
      snap_ridge_low(x);
      snap_ridge_high(x);
    }
    // 4 mounting pads with alignment pins on top
    mount_hole_positions()
      union() {
        translate([0, 0, floor_t])
          cylinder(d = boss_d, h = standoff_h);
        translate([0, 0, floor_t + standoff_h])
          cylinder(d = pin_d, h = pin_h);
      }
  }
  // Dimple texture on the 3 closed walls (cosmetic only -- no vent holes
  // here, only the ceiling needs airflow)
  translate([0, 0, floor_t])
    rotate([90, 0, 0])
      dot_field(outer_w, tray_wall_h, undef, false, 0);
  translate([0, outer_h, floor_t + tray_wall_h])
    rotate([-90, 0, 0])
      dot_field(outer_w, tray_wall_h, undef, false, 0);
  translate([outer_w, 0, floor_t])
    rotate([0, 0, 90])
      rotate([90, 0, 0])
        dot_field(outer_h, tray_wall_h, undef, false, 0);
  // Rounded vertical corners at the closed (far) end, where the long
  // walls meet the far wall. Larger radius than top_fillet -- over a run
  // this tall (tray_wall_h), 0.8mm read as basically sharp; a corner needs
  // a bigger radius than a grazing top/bottom edge does to actually look
  // rounded. Runs the full height, floor to top rim: tapers up from
  // top_fillet radius at the bottom (matching rounded_bottom()'s dome,
  // see corner_round_cut_full()) to the full corner_fillet radius for the
  // visible bulk of the wall, then top_mode="fillet" tapers it back down
  // to top_fillet radius by the top rim -- matching the ceiling's own
  // corner post directly above it (see the matching corner_round_cut()
  // cut through the ceiling in top_slide()), not a square collar.
  corner_round_cut_full(outer_w, 0, -1, 1, corner_fillet, top_fillet, ceiling_bot_z);
  corner_round_cut_full(outer_w, outer_h, -1, -1, corner_fillet, top_fillet, ceiling_bot_z);
  // Same rounding at the near (open, cable) end, where the stub wall
  // meets each long wall -- exposed for the stub wall's own height
  // (floor to end_wall_top_z) same as any other corner, independent of
  // the stub wall's own top edge staying square (see the stub wall's own
  // comment above). top_mode="abrupt": that top height is the hidden seam
  // under the top's leading wall, not an exposed rim, so there's no
  // reason to taper it back down here -- the top's own leading wall picks
  // up at this same full radius and tapers it the rest of the way (see
  // top_slide()).
  corner_round_cut_full(0, 0, 1, 1, corner_fillet, top_fillet, end_wall_top_z, top_mode = "abrupt");
  corner_round_cut_full(0, outer_h, 1, -1, corner_fillet, top_fillet, end_wall_top_z, top_mode = "abrupt");
  // Notch each long wall's near-end corner clear for the top's leading
  // wall to drop into. The long walls run the tray's full length (X=0 to
  // outer_w) and the leading wall spans the tray's full width (Y=0 to
  // outer_h) -- above end_wall_top_z, where the leading wall is the only
  // thing left to close this end, their footprints genuinely overlap in
  // a wall_t x wall_t x (ceiling_bot_z - end_wall_top_z) block at each
  // corner. Without this cut the two parts would occupy the same
  // physical space there and the top could never actually seat on the
  // tray -- this is what was making the near/port corner look wrong no
  // matter how the fillet was tuned: it wasn't the fillet, it was solid
  // tray wall sitting exactly where the lid's own wall needs to be.
  // Matches the leading wall's own wall_t width exactly, no extra
  // fit_clearance margin: unlike the skirt walls (which need clearance
  // because they slide sideways into a snug nested fit between the
  // tray's cavity walls), this notch and the leading wall only move
  // relative to each other in Z during the drop, never sideways, so
  // there's nothing to bind against -- and this corner sits right at
  // the visible outer edge, where any extra margin shows up as a
  // literal slot through to the tray's interior instead of a hidden
  // internal clearance.
  translate([-0.5, 0, end_wall_top_z])
    cube([wall_t + 0.5, wall_t, ceiling_bot_z - end_wall_top_z + 0.01]);
  translate([-0.5, outer_h - wall_t, end_wall_top_z])
    cube([wall_t + 0.5, wall_t, ceiling_bot_z - end_wall_top_z + 0.01]);
  }
}

// Simple dimple-grid texture, built in a canonical frame: the field
// spans local X=[0,span_u], Y=[0,span_v], with the textured surface
// sitting at local Z=0. Every grid point gets a shallow round dimple cut
// with a sphere (a sphere bigger than the visible dimple opening gives a
// clean rounded dish rather than a sharp ball-mill divot); on the
// ceiling, every `vent_col_stride`-th column instead gets a genuine
// through-hole. Fully deterministic -- no seed, no randomness, just a
// grid -- and reused as-is on every textured surface via a plain
// translate/rotate at the call site, which is the "efficient" part: one
// generator, no per-surface special-casing. `exclude` is an optional
// [x,y] point (in this same local frame) to keep clear of, e.g. the
// button tunnel. `vias` enables the through-hole columns, each cut from
// local Z=-thru to +thru (well past the surface on both sides, so it's a
// clean through-cut regardless of the caller's actual thickness) --
// these double as both a PCB "via" look and the case's actual
// ventilation/LED-visibility openings, so only the ceiling uses them.
module dot_field(span_u, span_v, exclude, vias, thru) {
  n_cols = floor((span_u - 2 * texture_margin) / dot_spacing) + 1;
  n_rows = floor((span_v - 2 * texture_margin) / dot_spacing) + 1;
  u0 = (span_u - (n_cols - 1) * dot_spacing) / 2;
  v0 = (span_v - (n_rows - 1) * dot_spacing) / 2;
  has_excl = !is_undef(exclude);

  for (i = [0 : n_cols - 1])
    for (j = [0 : n_rows - 1]) {
      u = u0 + i * dot_spacing;
      v = v0 + j * dot_spacing;
      if (!has_excl || norm([u, v] - exclude) > vent_exclude_r) {
        if (vias && i % vent_col_stride == 0 && j % 2 == 0)
          translate([u, v, -thru])
            cylinder(d = vent_hole_d, h = 2 * thru, $fn = 20);
        else
          translate([u, v, dot_r - dot_depth])
            sphere(r = dot_r, $fn = 14);
      }
    }
}

// Ceiling texture: the dot field mapped directly onto the top surface
// (no rotation needed, the canonical frame already matches world X/Y
// here), with vent columns enabled and the button tunnel excluded.
module dot_texture_ceiling(ceiling_top_z) {
  btn_pos = [board_origin[0] + button_hole_x, board_origin[1] + button_hole_y];
  translate([0, 0, ceiling_top_z])
    dot_field(outer_w, outer_h, btn_pos, true, wall_t + 1);
}

// One PCB retention washer, built at the local origin (XY centered on a
// pin). A plain ring -- not slotted or keyed to the pin -- extruded the
// full height from just above the board up to the ceiling. The pin passes
// straight through the ring's clearance hole on the way down; once
// seated, the ring's solid annulus rests near the board's surface around
// its mounting hole and holds it down passively, since the rigid top it
// hangs from can't lift away (see the snap-fit latch, header comment).
// half/dir/axis: at mounting holes close enough to a module that the full
// hook_od washer would physically overlap it, half=true keeps only the
// semicircle on the dir side (+1/-1) of the pin and drops the other, so
// the washer still catches the board on its open side without hitting the
// module. axis picks which diameter line the cut runs along -- "x" keeps
// the +X/-X half (module overlaps lengthwise), "y" keeps the +Y/-Y half
// (module overlaps across the board's width, e.g. opening the cap toward
// the board's own long edge to clear a module that covers the board's
// center-ward area at that hole -- confirmed against the real board for
// the CC1101 holes). FIRST-PASS UNVALIDATED for any hole not yet
// confirmed against real hardware in hand -- based on estimated/measured
// footprints, not a substitute for checking the actual module.
module hook_post(hook_bot_z, ceiling_top_z, half = false, dir = 1, axis = "x") {
  translate([0, 0, hook_bot_z])
    linear_extrude(height = ceiling_top_z - wall_t - hook_bot_z)
      if (half) {
        intersection() {
          difference() {
            circle(d = hook_od);
            circle(d = pin_d + hook_pin_clearance);
          }
          rotate([0, 0, axis == "y" ? 90 : 0])
            translate([dir > 0 ? 0 : -hook_od, -hook_od, 0])
              square([hook_od, hook_od * 2]);
        }
      } else {
        difference() {
          circle(d = hook_od);
          circle(d = pin_d + hook_pin_clearance);
        }
      }
}

// Flat-topped box with a rounded fillet around its top outer edge (all 4
// sides, including corners): flat-walled prism up to (t - r), capped
// with a minkowski-rounded dome for the last r of height. The dome's
// "equator" (at t - r) exactly matches the prism's own w x h cross
// section along the straight edges (inset-then-expand-by-r nets out to
// the original size), so the only visible seam is a slight softening
// right at the 4 corners, where the dome is already rounding them off
// but the prism below still has square corners.
module rounded_top(w, h, t, r) {
  union() {
    linear_extrude(t - r)
      square([w, h]);
    translate([0, 0, t - r])
      intersection() {
        minkowski() {
          translate([r, r, 0])
            linear_extrude(0.02)
              square([w - 2 * r, h - 2 * r]);
          sphere(r = r, $fn = 24);
        }
        translate([-1, -1, 0])
          cube([w + 2, h + 2, r + 1]);
      }
  }
}

// Ceiling + 2 skirt walls, nested just inside the tray's long walls with
// fit_clearance, plus a hook_post() washer over each pin and a
// leading-edge wall that closes off the open (cable) end down to a small
// round port. Built directly in the tray's own coordinate frame (same
// X/Y/Z as bottom_tray()) -- drops straight down onto the tray, no
// translate needed to assemble it.
module top_slide() {
  hook_bot_z = skirt_bot_z + hook_gap_above_board;

  difference() {
    union() {
      // ceiling panel, spans the tray's full outer footprint, with a
      // cosmetic rounded fillet around the top outer edge
      translate([0, 0, ceiling_bot_z])
        rounded_top(outer_w, outer_h, wall_t, top_fillet);
      // skirt walls, nested just inside the tray's long walls, each thinned
      // down to a flexible snap-fit tab at every position in snap_x_list
      // (see skirt_wall() -- one module, mirrored, for both walls)
      skirt_wall(wall_t + fit_clearance, 1);
      skirt_wall(outer_h - wall_t - fit_clearance, -1);
      // leading-edge wall: picks up exactly where the tray's fixed stub
      // wall stops (end_wall_top_z) and covers up to the ceiling's own
      // underside (NOT ceiling_top_z) -- stopping there, rather than
      // overlapping the ceiling's own fillet zone, lets the ceiling's
      // rounded edge show through on this short side too instead of
      // being filled back in flat by this wall
      translate([0, 0, end_wall_top_z])
        cube([wall_t, outer_h, ceiling_bot_z - end_wall_top_z]);
      // retention hooks, one per mounting pin -- half-cap (see hook_post())
      // at any hole whose washer footprint overlaps a module's own
      // footprint.
      //   CC1101 holes: axis="y", checked against the real module's
      //   footprint (cc1101_module_center/cc1101_module_outline, not the
      //   socket) -- being much bigger than the socket alone, the module
      //   now overlaps BOTH far-end holes, not just one, and both holes
      //   sit right at the module's near/far Y edge (module_outline[1]
      //   spans almost exactly hole-to-hole). So the module is on the
      //   board's CENTER side of each hole, not the edge side -- the cap
      //   has to open (cut away) toward the center, where the module
      //   actually sits, and keep its material toward whichever long
      //   edge (Y=0 or Y=outer_h) is nearest -- the opposite of what an
      //   earlier pass here had it doing, which pointed the kept material
      //   straight into the module.
      //   ESP32 holes: axis="x", now checked against esp32_outline's real
      //   published dimensions (22.5 x 18mm) instead of an early guess
      //   that had it both backwards and nearly spanning the full board
      //   height -- the real (narrower) module clips these two holes by
      //   about 1.5mm in X, a clear overlap rather than measurement
      //   noise. Y still isn't the fix here: even at 18mm wide, the
      //   module's Y-span only leaves ~0.5mm past each hole on its far
      //   side, too thin a margin to trust as "the hole sits at the
      //   edge" the way CC1101's holes clearly do -- X remains the
      //   decisive, unambiguous overlap axis, so the cap keeps the half
      //   away from the module's X-center instead.
      for (h = mount_holes) {
        overlaps_cc1101 = (h[0] + hook_od / 2 > cc1101_module_center[0] - cc1101_module_outline[0] / 2)
                        && (h[0] - hook_od / 2 < cc1101_module_center[0] + cc1101_module_outline[0] / 2)
                        && (h[1] + hook_od / 2 > cc1101_module_center[1] - cc1101_module_outline[1] / 2)
                        && (h[1] - hook_od / 2 < cc1101_module_center[1] + cc1101_module_outline[1] / 2);
        overlaps_esp32 = (h[0] + hook_od / 2 > esp32_center[0] - esp32_outline[0] / 2)
                       && (h[0] - hook_od / 2 < esp32_center[0] + esp32_outline[0] / 2)
                       && (h[1] + hook_od / 2 > esp32_center[1] - esp32_outline[1] / 2)
                       && (h[1] - hook_od / 2 < esp32_center[1] + esp32_outline[1] / 2);
        translate(b2c(h))
          if (overlaps_cc1101)
            hook_post(hook_bot_z, ceiling_top_z, half = true, axis = "y",
                       dir = (h[1] < board_h / 2) ? -1 : 1);
          else if (overlaps_esp32)
            hook_post(hook_bot_z, ceiling_top_z, half = true, axis = "x",
                       dir = (h[0] >= esp32_center[0]) ? 1 : -1);
          else
            hook_post(hook_bot_z, ceiling_top_z);
      }
      // reset-button guide tube, hanging from the ceiling down toward the
      // board (overlaps into the ceiling itself for a clean union)
      translate([board_origin[0] + button_hole_x, board_origin[1] + button_hole_y, button_tube_bot_z])
        cylinder(d = button_tube_od, h = ceiling_top_z - button_tube_bot_z);
    }
    // reset button bore, straight through the guide tube and the ceiling
    translate([board_origin[0] + button_hole_x, board_origin[1] + button_hole_y, button_tube_bot_z - 0.5])
      cylinder(d = button_hole_d, h = (ceiling_top_z - button_tube_bot_z) + 1);
    // Dimple texture + integrated vent holes
    dot_texture_ceiling(ceiling_top_z);
    // USB-C port through the leading-edge wall: a plain flush opening,
    // straight through the wall's full thickness, no recessed pocket
    // around it. An earlier version stepped the surface back around the
    // opening for a receptacle-style look -- removed, since that step
    // narrows the usable opening right where a cable's own plug
    // housing (wider than the bare connector) needs clearance to seat,
    // not just the bare connector pins. This assumes the board's own
    // onboard USB-C connector sits right here and gets plugged into
    // from outside after the case is closed -- but every dimension is
    // still a guess (see usb_port_* above).
    //
    // A plain closed rectangle (usb_port_window_bot to usb_port_window_top,
    // both computed above alongside ceiling_bot_z): the plug only needs
    // to reach the connector, not the roof of the case, so the opening
    // stops with real wall material still above it rather than cutting
    // through into the ceiling.
    translate([-0.5,
               board_origin[1] + usb_port_y - usb_port_w / 2,
               usb_port_window_bot])
      cube([wall_t + 1, usb_port_w, usb_port_h]);
    // Vertical corner rounding on the leading-edge wall (the near/USB-side
    // corners), picking up exactly where the tray's own near-corner
    // rounding leaves off: the tray rounds those same 2 corners at full
    // corner_fillet radius up to end_wall_top_z with no taper-back (see
    // bottom_tray()), since nothing of the tray continues above that
    // height at this end -- this wall is what continues there once
    // assembled, so it starts at that same full radius (no seam) and
    // tapers down to top_fillet radius by ceiling_bot_z, matching the
    // ceiling's own corner post there too (see the matching
    // corner_round_cut() cuts below, through the ceiling's full
    // thickness at all 4 corners) instead of vanishing to a point and
    // leaving the ceiling's own sharp pre-dome collar looking square.
    corner_taper_cut(0, 0, 1, 1, corner_fillet, end_wall_top_z, top_fillet, ceiling_bot_z);
    corner_taper_cut(0, outer_h, 1, -1, corner_fillet, end_wall_top_z, top_fillet, ceiling_bot_z);
    // Inset all 4 corners of the ceiling itself at top_fillet radius, from
    // ceiling_bot_z up to exactly where rounded_top()'s own dome begins
    // (ceiling_top_z - top_fillet) -- NOT the ceiling's full thickness:
    // above that height the dome already rounds the corner over at this
    // same radius (its cross-section there is a shrinking quarter-circle
    // of radius <= top_fillet, same tangent point), so this cut stops
    // exactly where that takes over rather than running a flat-topped
    // cylinder straight through the dome and squaring off the roofline.
    // Below it, though, the ceiling's corner is plain square for that
    // whole ~1.2mm stretch (2mm thick minus the dome's own top_fillet) --
    // exactly the collar that made the assembled corner read as square
    // above the wall's fillet, even though the wall itself was rounded
    // right up to the seam. This fills that stretch at the same radius,
    // so the corner reads as continuously rounded from the wall below
    // all the way up into the dome's own taper to the top edge.
    corner_round_cut(outer_w, 0, -1, 1, top_fillet, ceiling_bot_z, wall_t - top_fillet);
    corner_round_cut(outer_w, outer_h, -1, -1, top_fillet, ceiling_bot_z, wall_t - top_fillet);
    corner_round_cut(0, 0, 1, 1, top_fillet, ceiling_bot_z, wall_t - top_fillet);
    corner_round_cut(0, outer_h, 1, -1, top_fillet, ceiling_bot_z, wall_t - top_fillet);
  }
}

// Rough visual fit-check only -- NOT a real component model and not part
// of either printable piece (only shown in the "both" preview, never
// under "tray"/"top"). PCB outline/position/mounting holes are measured
// (see hw/pcb.py); module heights above the board are unmeasured guesses
// -- update once the real modules are in hand.
module board_reference() {
  color("green", 0.5)
    translate([board_origin[0], board_origin[1], floor_t + standoff_h])
      cube([board_w, board_h, board_t]);
  color("orange", 0.5)
    translate([board_origin[0] + esp32_center[0] - esp32_outline[0] / 2,
                board_origin[1] + esp32_center[1] - esp32_outline[1] / 2,
                floor_t + standoff_h + board_t])
      cube([esp32_outline[0], esp32_outline[1], esp32_pcb_top_above_board]); // measured PCB height only, not the taller connector/IC on top
  color("red", 0.5)
    translate([board_origin[0] + cc1101_module_center[0] - cc1101_module_outline[0] / 2,
                board_origin[1] + cc1101_module_center[1] - cc1101_module_outline[1] / 2,
                floor_t + standoff_h + board_t])
      cube([cc1101_module_outline[0], cc1101_module_outline[1], 5]); // height: unmeasured guess
  // USB-C connector body, mounted on the ESP32 module -- sized and
  // positioned from the same detailed reference model as
  // esp32_pcb_top_above_board/usb_connector_height: 9mm wide x 7.3mm
  // deep, sitting 1mm proud of the ESP32 sub-module's own near edge (not
  // the main board's edge -- an earlier version of this box started 2mm
  // out past the case's leading wall entirely, which the real board's
  // photos contradict directly: the connector does not overhang the
  // main PCB at all, since the ESP32 sub-module itself is well inset
  // from the main board's edge).
  usb_connector_depth = 7.3;
  usb_connector_width = 9;
  color("silver", 0.7)
    translate([board_origin[0] + esp32_center[0] - esp32_outline[0] / 2 - 1,
                board_origin[1] + esp32_center[1] - usb_connector_width / 2,
                skirt_bot_z + usb_port_z_above_board - usb_port_h / 2])
      cube([usb_connector_depth, usb_connector_width, usb_port_h]);
  // Antenna, extending antenna_reach past the CC1101 module's far edge --
  // see antenna_reach's own comment for how it was measured and its
  // orientation caveat. A plain cylinder standing in for the real coil,
  // just to make the reach visually checkable against the case length.
  color("silver", 0.6)
    translate([board_origin[0] + cc1101_module_center[0] + cc1101_module_outline[0] / 2,
                board_origin[1] + cc1101_module_center[1],
                floor_t + standoff_h + board_t + 5])
      rotate([0, 90, 0])
        cylinder(d = 4, h = antenna_reach, $fn = 16);
}

if (part == "tray") {
  bottom_tray();
} else if (part == "top") {
  translate([0, outer_h + 10, 0])
    top_slide();
} else if (part == "fit") {
  // tray + PCB reference only, no top -- the top is opaque and would
  // hide the board from any outside camera angle, so this is the view
  // that actually shows how the board sits in the tray
  bottom_tray();
  board_reference();
} else if (part == "detail") {
  // debug/visualization only, not a printable part: fully assembled, but
  // the tray is translucent so the catch ridges show through the wall
  // right where the top's tabs sit, instead of being hidden behind it.
  color("gold", 0.35) bottom_tray();
  top_slide();
} else {
  // Default combined preview: tray, top, and PCB reference all visibly
  // distinct colors (and translucent) so it's easy to see how everything
  // actually lines up, rather than three overlapping solids in the same
  // default material color.
  #color("steelblue", 0.5) bottom_tray();
  color("lightgray", 0.6) top_slide();
  board_reference();
}
