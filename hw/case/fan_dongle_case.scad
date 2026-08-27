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
// ways). What actually keeps the case shut is a snap-fit latch: one
// flexible cantilever tab per long wall, engaging a ramped catch ridge on
// the tray wall as the top drops down (a locking screw was tried first,
// but the CC1101's antenna coils well past the board's far edge, right
// where the screw boss needed to sit). Everything else here is fit and
// alignment, not a lock. The tray's open (cable) end has a short fixed
// stub wall covering its bottom
// third; the top's leading wall picks up right where that stops and
// covers the rest, closing off the whole opening between them except for
// a slot sized to the cable itself. Both the top's outer edge and the
// tray's long walls carry a matching cosmetic bevel where they meet.
//
// Render in OpenSCAD, or from the command line:
//   openscad -o tray.stl   -D 'part="tray"' fan_dongle_case.scad
//   openscad -o top.stl    -D 'part="top"'  fan_dongle_case.scad
//   openscad -o preview.png -D 'part="both"' --imgsize=1200,900 --camera=34,12,10,55,0,25,220 fan_dongle_case.scad
//   openscad -o fit.png     -D 'part="fit"'  --imgsize=1200,900 --camera=34,12,10,55,0,25,220 fan_dongle_case.scad
//     ("fit" = tray + a translucent PCB/module reference, no top -- for checking fit; not a real part)

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
esp32_outline = [20, 22.5];  // module footprint envelope, X x Y

// CC1101 module socket/footprint center + silkscreen outline (local)
cc1101_center = [48, 12];
cc1101_outline = [16, 19];

// ==========================================
// ESTIMATED -- verify against real hardware before printing
// ==========================================
// Modules are soldered directly (no header sockets), so components sit
// close to the board -- this is just body height + a safety margin, not
// header-stack height. Increase if your modules are taller than this.
component_clearance = 8;

// USB-C cable: modules face the board's short left edge (X=0). The
// tray's near end carries a short stub wall (see end_wall_frac below);
// above that, the top piece's own leading edge closes the rest of the
// opening except for a slot sized to the cable itself, not a connector.
usb_hole_d = 4;                // cable slot width -- just the cable's own
                                 // diameter, not a connector -- guess, verify
usb_hole_y = 12;               // centered on board Y (= ESP32 module Y-center)
usb_hole_z_above_board = 3.7; // port center height above the board's top surface -- guess, verify
                                // (kept off usb_hole_d/2 exactly -- that coincidence put the
                                // port's bottom edge exactly on the wall's own bottom face and
                                // produced a non-manifold cut)

// Reset-button guide tunnel: button is top-mounted (pressed straight
// down). A tube hangs from the ceiling down toward the board, so a
// toothpick/pin is guided straight to the button instead of just poking
// through a thin hole in the ceiling with nothing below to aim it.
// Position is a rough estimate (typical Super Minis cluster it near the
// USB-C edge) -- check against your actual module and adjust before
// printing, along with button_tube_gap_above_board below.
button_hole_d = 3;      // bore diameter -- fits a toothpick/paperclip/pin
button_hole_x = 8;      // local X, near the left/USB edge
button_hole_y = 4;      // local Y -- right side when facing the USB port
button_tube_wall = 1;                // tube wall thickness
button_tube_gap_above_board = 2;    // gap between the tube's open bottom
                                       // and the board's top surface -- guess,
                                       // verify against actual button height

// Cosmetic 45-degree bevel around the top face's outer edge (all 4 sides).
top_bevel = 1.2;

// No external LED -- the onboard LED (GPIO8/GND) shows through the vent
// holes in the ceiling instead of needing its own dedicated hole.
// Ventilation: small dot grid across the ceiling, skipped near the button
// tunnel so nothing merges awkwardly close together.
vent_hole_d = 2;
vent_spacing = 6;      // grid pitch
vent_margin = 6;       // dots stay at LEAST this far from the outer edge --
                         // the grid is centered on the ceiling, so the actual
                         // margin is whatever's left over after fitting as
                         // many rows/columns as possible at this spacing
vent_exclude_r = 6;    // skip any dot this close to the button tunnel

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
// as a general buffer past the board's far edge; may need to grow a lot
// once the antenna's real reach is measured.
far_end_extra = 10;

// Snap-fit latch: what actually keeps the case shut, now that a screw in
// the far-end gap isn't an option. One flexible cantilever tab per long
// wall, centered along the board's length, engaging a ramped ridge on the
// tray wall -- ramped on the entry (bottom) side for a smooth push as the
// top drops down, flat on the catch (top) side so pulling the top back
// off requires deliberately flexing the tab back out. UNVALIDATED --
// first pass, needs a test print to confirm the tab actually flexes
// without snapping in your printed material (PLA especially is brittle;
// may need a wider/thicker tab, or a more flexible filament, to survive
// repeated opening).
snap_tab_w = 8;               // tab width
snap_tab_thickness = 1;       // tab thickness (thinner than wall_t, for flex)
snap_bump_protrusion = 0.6;  // how far the catch bump sticks out
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
button_tube_bot_z = skirt_bot_z + button_tube_gap_above_board;
end_wall_top_z = floor_t + tray_wall_h * end_wall_frac;

// Board-local -> case-local (board sits centered in the cavity, offset by
// wall_t + board_margin from the case's own bottom-left corner)
board_origin = [wall_t + board_margin, wall_t + board_margin];

function b2c(p) = [p[0] + board_origin[0], p[1] + board_origin[1]];

// snap-fit tab/ridge position: centered along the board's length, and
// vertically centered in the skirt's engagement zone (same spot the old
// rail used to sit)
snap_x = board_origin[0] + board_w / 2;
snap_z_lo = skirt_bot_z + (ceiling_bot_z - skirt_bot_z) / 2 - snap_engage_h / 2;

// Bevel to match the top's, applied to the top outer edge of the tray's
// long side walls -- only the exterior-facing surface tapers; the
// interior (cavity/skirt-facing) surface stays flush so it doesn't
// disturb the rail or the skirt fit.
module beveled_wall_low(length, thickness, height, bevel) {
  // exterior face at local Y=0
  hull() {
    cube([length, thickness, height - bevel]);
    translate([0, bevel, height - bevel])
      cube([length, thickness - bevel, bevel]);
  }
}
module beveled_wall_high(length, thickness, height, bevel) {
  // exterior face at local Y=thickness
  hull() {
    cube([length, thickness, height - bevel]);
    translate([0, 0, height - bevel])
      cube([length, thickness - bevel, bevel]);
  }
}

// Snap-fit catch ridges, wedge-shaped: full snap_bump_protrusion at the
// bottom (snap_z_lo), tapering to ~flush at the top (snap_z_lo +
// snap_engage_h). A tab sliding down past this ramps outward gradually
// (easy); pulling back up hits the flat, full-width bottom face
// immediately (hard) -- that asymmetry is what makes it a catch and not
// just a bump.
module snap_ridge_low() {
  // attaches to the low wall's inner face (Y=wall_t), protrudes toward +Y
  translate([snap_x - snap_tab_w / 2 - 1, wall_t, snap_z_lo])
    hull() {
      cube([snap_tab_w + 2, snap_bump_protrusion, 0.1]);
      translate([0, 0, snap_engage_h])
        cube([snap_tab_w + 2, 0.1, 0.1]);
    }
}
module snap_ridge_high() {
  // attaches to the high wall's inner face (Y=outer_h-wall_t), protrudes toward -Y
  translate([snap_x - snap_tab_w / 2 - 1, outer_h - wall_t - snap_bump_protrusion, snap_z_lo])
    hull() {
      cube([snap_tab_w + 2, snap_bump_protrusion, 0.1]);
      translate([0, snap_bump_protrusion - 0.1, snap_engage_h])
        cube([snap_tab_w + 2, 0.1, 0.1]);
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
  union() {
    // floor
    cube([outer_w, outer_h, floor_t]);
    // long side walls, full tray height, spanning the whole length --
    // bevel to match the top's, on the exterior-facing side only
    translate([0, 0, floor_t])
      beveled_wall_low(outer_w, wall_t, tray_wall_h, top_bevel);
    translate([0, outer_h - wall_t, floor_t])
      beveled_wall_high(outer_w, wall_t, tray_wall_h, top_bevel);
    // closed far-end wall
    translate([outer_w - wall_t, 0, floor_t])
      cube([wall_t, outer_h, tray_wall_h]);
    // near-end stub wall -- closes the bottom end_wall_frac of the open
    // (cable) end. The top's leading wall picks up right where this
    // stops (see top_slide()), so the two seal the opening together.
    translate([0, 0, floor_t])
      cube([wall_t, outer_h, end_wall_top_z - floor_t]);
    // snap-fit catch ridges, one per long wall (snap_z_lo is already an
    // absolute Z, no extra offset needed)
    snap_ridge_low();
    snap_ridge_high();
    // 4 mounting pads with alignment pins on top
    mount_hole_positions()
      union() {
        translate([0, 0, floor_t])
          cylinder(d = boss_d, h = standoff_h);
        translate([0, 0, floor_t + standoff_h])
          cylinder(d = pin_d, h = pin_h);
      }
  }
}

// Grid is centered on the ceiling (equal leftover margin on every side),
// not just started at vent_margin and run until it stops fitting -- fits
// as many full rows/columns as possible at vent_spacing, at least
// vent_margin from the edge.
module vent_holes(ceiling_z) {
  btn_pos = [board_origin[0] + button_hole_x, board_origin[1] + button_hole_y];
  n_cols = floor((outer_w - 2 * vent_margin) / vent_spacing) + 1;
  n_rows = floor((outer_h - 2 * vent_margin) / vent_spacing) + 1;
  x0 = (outer_w - (n_cols - 1) * vent_spacing) / 2;
  y0 = (outer_h - (n_rows - 1) * vent_spacing) / 2;
  for (i = [0 : n_cols - 1])
    for (j = [0 : n_rows - 1]) {
      x = x0 + i * vent_spacing;
      y = y0 + j * vent_spacing;
      if (norm([x, y] - btn_pos) > vent_exclude_r)
        translate([x, y, ceiling_z - 0.5])
          cylinder(d = vent_hole_d, h = wall_t + 1);
    }
}

// One PCB retention washer, built at the local origin (XY centered on a
// pin). A plain ring -- not slotted or keyed to the pin -- extruded the
// full height from just above the board up to the ceiling. The pin passes
// straight through the ring's clearance hole on the way down; once
// seated, the ring's solid annulus rests near the board's surface around
// its mounting hole and holds it down passively, since the rigid top it
// hangs from can't lift away (see the snap-fit latch, header comment).
module hook_post(hook_bot_z, ceiling_top_z) {
  translate([0, 0, hook_bot_z])
    linear_extrude(height = ceiling_top_z - wall_t - hook_bot_z)
      difference() {
        circle(d = hook_od);
        circle(d = pin_d + hook_pin_clearance);
      }
}

// Flat-topped box with a 45-degree bevel around its top outer edge (all 4
// sides): full size up to (t - bevel), then a hull() out to a top face
// inset by "bevel" on every side, tapering the last "bevel" of height.
module beveled_top(w, h, t, bevel) {
  hull() {
    cube([w, h, t - bevel]);
    translate([bevel, bevel, t - bevel])
      cube([w - 2 * bevel, h - 2 * bevel, bevel]);
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
      // cosmetic bevel around the top outer edge
      translate([0, 0, ceiling_bot_z])
        beveled_top(outer_w, outer_h, wall_t, top_bevel);
      // skirt walls, nested just inside the tray's long walls, each thinned
      // down to a flexible snap-fit tab at snap_x -- the matching ridge on
      // the tray wall protrudes past this tab's resting face, so the ridge
      // alone (already ramped/flat, see snap_ridge_low/high) creates the
      // catch interference without needing a separate bump here
      difference() {
        translate([0, wall_t + fit_clearance, skirt_bot_z])
          cube([outer_w, wall_t, ceiling_bot_z - skirt_bot_z]);
        translate([snap_x - snap_tab_w / 2, wall_t + fit_clearance + snap_tab_thickness, skirt_bot_z - 0.5])
          cube([snap_tab_w, wall_t - snap_tab_thickness + 0.5, ceiling_bot_z - skirt_bot_z + 1]);
      }
      difference() {
        translate([0, outer_h - 2 * wall_t - fit_clearance, skirt_bot_z])
          cube([outer_w, wall_t, ceiling_bot_z - skirt_bot_z]);
        translate([snap_x - snap_tab_w / 2, outer_h - 2 * wall_t - fit_clearance, skirt_bot_z - 0.5])
          cube([snap_tab_w, wall_t - snap_tab_thickness + 0.5, ceiling_bot_z - skirt_bot_z + 1]);
      }
      // leading-edge wall: picks up exactly where the tray's fixed stub
      // wall stops (end_wall_top_z) and covers up to the ceiling's own
      // underside (NOT ceiling_top_z) -- stopping there, rather than
      // overlapping the ceiling's own bevel zone, lets the ceiling's
      // 45-degree edge taper show through on this short side too instead
      // of being filled back in flat by this wall
      translate([0, 0, end_wall_top_z])
        cube([wall_t, outer_h, ceiling_bot_z - end_wall_top_z]);
      // retention hooks, one per mounting pin
      mount_hole_positions()
        hook_post(hook_bot_z, ceiling_top_z);
      // reset-button guide tube, hanging from the ceiling down toward the
      // board (overlaps into the ceiling itself for a clean union)
      translate([board_origin[0] + button_hole_x, board_origin[1] + button_hole_y, button_tube_bot_z])
        cylinder(d = button_tube_od, h = ceiling_top_z - button_tube_bot_z);
    }
    // reset button bore, straight through the guide tube and the ceiling
    translate([board_origin[0] + button_hole_x, board_origin[1] + button_hole_y, button_tube_bot_z - 0.5])
      cylinder(d = button_hole_d, h = (ceiling_top_z - button_tube_bot_z) + 1);
    // ventilation dot grid
    vent_holes(ceiling_bot_z);
    // cable slot through the leading-edge wall, open at the wall's own
    // bottom edge (end_wall_top_z, where the tray's stub wall stops) --
    // a fully closed hole would mean threading the whole cable
    // (including whatever's on its far end) through before the top could
    // even be dropped on. This way the cable gets plugged into the board
    // first, then the top drops into place around it. Straight sides
    // down to the wall's bottom, rounded off at the top where the cable
    // actually rests.
    translate([-0.5, board_origin[1] + usb_hole_y - usb_hole_d / 2, end_wall_top_z])
      cube([wall_t + 1, usb_hole_d, (skirt_bot_z + usb_hole_z_above_board) - end_wall_top_z]);
    translate([-0.5, board_origin[1] + usb_hole_y, skirt_bot_z + usb_hole_z_above_board])
      rotate([0, 90, 0])
        cylinder(d = usb_hole_d, h = wall_t + 1);
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
      cube([esp32_outline[0], esp32_outline[1], 4]); // height: unmeasured guess
  color("red", 0.5)
    translate([board_origin[0] + cc1101_center[0] - cc1101_outline[0] / 2,
                board_origin[1] + cc1101_center[1] - cc1101_outline[1] / 2,
                floor_t + standoff_h + board_t])
      cube([cc1101_outline[0], cc1101_outline[1], 5]); // height: unmeasured guess
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
} else {
  bottom_tray();
  top_slide();
  board_reference();
}
