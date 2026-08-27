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
// ridge on the tray wall as the top drops down (a locking screw was tried
// first, but the CC1101's antenna coils well past the board's far edge,
// right where the screw boss needed to sit). Everything else here is fit
// and alignment, not a lock. The tray's open (cable) end has a short
// fixed stub wall covering its bottom third; the top's leading wall picks
// up right where that stops and covers the rest, closing off the whole
// opening between them except for a recessed USB-C port opening.
// Exterior edges carry a cosmetic rounded fillet (not a chamfer): the
// ceiling's top edge and corners, the tray floor's bottom edge and
// corners, and the top-outer edge of all 3 tray walls that stay exposed
// once assembled (the near stub wall's top edge sits under the top's
// leading wall and is never actually visible, so it's left square). The
// vertical corner edges, where walls meet each other, are NOT rounded --
// a much bigger job for this CSG-built model than the horizontal edges,
// and not attempted yet.
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

// Recessed USB-C port: modules face the board's short left edge (X=0).
// Rather than a soldered/attached cable routed through an open slot, this
// assumes the Super Mini's own onboard USB-C receptacle sits right at
// this opening -- you plug a cable in from outside whenever needed,
// nothing threads through during assembly. ALL GUESSED, no real
// measurement behind any of these yet -- the position assumes the
// connector centers on the module the same way usb_port_y assumes
// (= ESP32 module Y-center), and the size/depth are generic USB-C
// dimensions with margin, not measured off this specific module. Treat
// this as a placeholder to refine once the board's actual connector
// position/height is measured -- don't cut a real part from this without
// checking it first.
usb_port_y = 12;                    // port center, board Y (= ESP32 module Y-center)
usb_port_z_above_board = 4;       // port center height above the board's top surface -- guess
usb_port_w = 10;                    // through-opening width (X) -- generous for a USB-C plug
usb_port_h = 4;                      // through-opening height (Z)
usb_port_recess_margin = 2;       // how much wider/taller the shallow surface recess is than
                                      // the through-opening itself, on each side
usb_port_recess_depth = 1;         // how far the surface recess sinks in (less than wall_t)

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

// Cosmetic rounded fillet around the top face's outer edge (all 4 sides).
// Kept well under wall_t/2 so the rounding stays within the wall's own
// thickness instead of poking out the inner face.
top_fillet = 0.8;

// No external LED -- the onboard LED (GPIO8/GND) shows through the vent
// slots in the ceiling instead of needing its own dedicated hole.
// Ventilation: a parametric wave/ripple texture across the ceiling --
// shallow parallel grooves (cosmetic only, don't cut through) with a
// narrow through-slot at each groove's low point for actual airflow.
// A regular, repeating approximation of an organic flow pattern, not a
// mesh-sculpted match to one -- OpenSCAD's CSG modeling doesn't really
// do freeform organic surfaces directly.
wave_spacing = 10;       // distance between groove centers
wave_amplitude = 0.6;   // how deep each groove cuts into the top surface -- subtle
wave_r = 15;               // groove cylinder radius (bigger = shallower/wider groove)
slot_w = 1.6;               // vent slot width
slot_margin = 6;         // keep slots/grooves at least this far from the outer edge
vent_exclude_r = 6;      // skip/split a slot this close to the button tunnel

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
// the far-end gap isn't an option. TWO flexible cantilever tabs per long
// wall (four total) -- spread toward each end of the board rather than
// one centered tab, so both ends of the ceiling get held down instead of
// just the middle (a single centered tab left the near/far ends of the
// panel with nothing catching them, free to flex or lift even with the
// middle snapped shut). Each tab engages a ramped ridge on the tray
// wall -- ramped on the entry (bottom) side for a smooth push as the top
// drops down, flat on the catch (top) side so pulling the top back off
// requires deliberately flexing the tab back out. UNVALIDATED -- first
// pass, needs a test print to confirm the tabs actually flex without
// snapping in your printed material (PLA especially is brittle; may need
// wider/thicker tabs, or a more flexible filament, to survive repeated
// opening).
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

// snap-fit tab/ridge X positions: spread toward each end of the board
// (20%/80% along its length) rather than one centered point, clear of the
// hook washers at the board's corners. Z is centered in the skirt's
// engagement zone (same spot the old rail used to sit).
snap_x_list = [board_origin[0] + board_w * 0.2, board_origin[0] + board_w * 0.8];
snap_z_lo = skirt_bot_z + (ceiling_bot_z - skirt_bot_z) / 2 - snap_engage_h / 2;

// Rounded fillet to match the top's, applied to the top outer edge of the
// tray's long side walls -- only the exterior-facing surface rounds off;
// the interior (cavity/skirt-facing) surface stays flush so it doesn't
// disturb the snap-fit tab or the skirt fit. A cylinder lying along the
// wall's length, positioned so it's tangent to both the flat lower wall
// and the flat inner cap -- true quarter-round, not a chamfer.
module rounded_wall_low(length, thickness, height, r) {
  // exterior face at local Y=0
  union() {
    cube([length, thickness, height - r]);
    translate([0, r, height - r])
      cube([length, thickness - r, r]);
    translate([0, r, height - r])
      rotate([0, 90, 0])
        cylinder(r = r, h = length, $fn = 24);
  }
}
module rounded_wall_high(length, thickness, height, r) {
  // exterior face at local Y=thickness
  union() {
    cube([length, thickness, height - r]);
    translate([0, 0, height - r])
      cube([length, thickness - r, r]);
    translate([0, thickness - r, height - r])
      rotate([0, 90, 0])
        cylinder(r = r, h = length, $fn = 24);
  }
}
// Same idea, but for a wall running along Y (exterior face at local
// X=thickness, larger X) -- used for the tray's far end wall.
module rounded_wall_x_high(length_y, thickness, height, r) {
  union() {
    cube([thickness, length_y, height - r]);
    translate([0, 0, height - r])
      cube([thickness - r, length_y, r]);
    translate([thickness - r, 0, height - r])
      rotate([-90, 0, 0])
        cylinder(r = r, h = length_y, $fn = 24);
  }
}

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
    // floor, with a rounded fillet around its bottom outer edge too --
    // no sharp edge where the case sits on a table
    rounded_bottom(outer_w, outer_h, floor_t, top_fillet);
    // long side walls, full tray height, spanning the whole length --
    // rounded fillet to match the top's, on the exterior-facing side only
    translate([0, 0, floor_t])
      rounded_wall_low(outer_w, wall_t, tray_wall_h, top_fillet);
    translate([0, outer_h - wall_t, floor_t])
      rounded_wall_high(outer_w, wall_t, tray_wall_h, top_fillet);
    // closed far-end wall, same rounded top edge as the long walls
    translate([outer_w - wall_t, 0, floor_t])
      rounded_wall_x_high(outer_h, wall_t, tray_wall_h, top_fillet);
    // near-end stub wall -- closes the bottom end_wall_frac of the open
    // (cable) end. The top's leading wall picks up right where this
    // stops (see top_slide()), so the two seal the opening together. No
    // rounding here -- its top edge is an internal seam under the top's
    // leading wall once assembled, never actually exposed.
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
}

// One vent slot: a rounded-rectangle (capsule) through-cut from y_lo to
// y_hi at a fixed X, full ceiling thickness.
module vent_slot(x, y_lo, y_hi) {
  translate([x, 0, ceiling_bot_z - 0.5])
    hull() {
      translate([0, y_lo, 0]) cylinder(d = slot_w, h = wall_t + 1);
      translate([0, y_hi, 0]) cylinder(d = slot_w, h = wall_t + 1);
    }
}

// Wave/ripple texture: shallow parallel grooves (cosmetic, don't cut
// through) spanning the ceiling's full width, repeating along its length
// at wave_spacing, centered with equal margin on both ends. Each groove
// gets a vent_slot() at its low point (X), split into two segments around
// the button tunnel if a groove happens to land close to it.
module wave_texture(ceiling_top_z) {
  btn_pos = [board_origin[0] + button_hole_x, board_origin[1] + button_hole_y];
  n = floor((outer_w - 2 * slot_margin) / wave_spacing) + 1;
  x0 = (outer_w - (n - 1) * wave_spacing) / 2;
  y_lo = slot_margin;
  y_hi = outer_h - slot_margin;
  for (i = [0 : n - 1]) {
    x = x0 + i * wave_spacing;
    // shallow cosmetic groove across the full width -- a large-radius
    // cylinder lying along Y, sunk into the top surface by wave_amplitude
    translate([x, -1, ceiling_top_z + wave_r - wave_amplitude])
      rotate([-90, 0, 0])
        cylinder(r = wave_r, h = outer_h + 2);
    // vent slot at this groove's low point
    if (abs(x - btn_pos[0]) < vent_exclude_r) {
      if (btn_pos[1] - vent_exclude_r > y_lo + slot_w)
        vent_slot(x, y_lo, btn_pos[1] - vent_exclude_r);
      if (btn_pos[1] + vent_exclude_r < y_hi - slot_w)
        vent_slot(x, btn_pos[1] + vent_exclude_r, y_hi);
    } else {
      vent_slot(x, y_lo, y_hi);
    }
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
      // down to a flexible snap-fit tab at every position in snap_x_list --
      // the matching ridge on the tray wall protrudes past each tab's
      // resting face, so the ridge alone (already ramped/flat, see
      // snap_ridge_low/high) creates the catch interference without
      // needing a separate bump here
      difference() {
        translate([0, wall_t + fit_clearance, skirt_bot_z])
          cube([outer_w, wall_t, ceiling_bot_z - skirt_bot_z]);
        for (x = snap_x_list)
          translate([x - snap_tab_w / 2, wall_t + fit_clearance + snap_tab_thickness, skirt_bot_z - 0.5])
            cube([snap_tab_w, wall_t - snap_tab_thickness + 0.5, ceiling_bot_z - skirt_bot_z + 1]);
      }
      difference() {
        translate([0, outer_h - 2 * wall_t - fit_clearance, skirt_bot_z])
          cube([outer_w, wall_t, ceiling_bot_z - skirt_bot_z]);
        for (x = snap_x_list)
          translate([x - snap_tab_w / 2, outer_h - 2 * wall_t - fit_clearance, skirt_bot_z - 0.5])
            cube([snap_tab_w, wall_t - snap_tab_thickness + 0.5, ceiling_bot_z - skirt_bot_z + 1]);
      }
      // leading-edge wall: picks up exactly where the tray's fixed stub
      // wall stops (end_wall_top_z) and covers up to the ceiling's own
      // underside (NOT ceiling_top_z) -- stopping there, rather than
      // overlapping the ceiling's own fillet zone, lets the ceiling's
      // rounded edge show through on this short side too instead of
      // being filled back in flat by this wall
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
    // wave/ripple texture + integrated vent slots
    wave_texture(ceiling_top_z);
    // Recessed USB-C port through the leading-edge wall: a shallow surface
    // recess (exterior face only, not through) surrounds a smaller
    // through-opening, giving a stepped-in receptacle look rather than a
    // flush slot. There's no cable to thread through assembly anymore --
    // this assumes the board's own onboard USB-C connector sits right
    // here and gets plugged into from outside after the case is closed --
    // but every dimension is still a guess (see usb_port_* above).
    translate([-0.5,
               board_origin[1] + usb_port_y - usb_port_w / 2,
               skirt_bot_z + usb_port_z_above_board - usb_port_h / 2])
      cube([wall_t + 1, usb_port_w, usb_port_h]);
    translate([-0.5,
               board_origin[1] + usb_port_y - usb_port_w / 2 - usb_port_recess_margin,
               skirt_bot_z + usb_port_z_above_board - usb_port_h / 2 - usb_port_recess_margin])
      cube([usb_port_recess_depth + 0.5,
            usb_port_w + 2 * usb_port_recess_margin,
            usb_port_h + 2 * usb_port_recess_margin]);
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
