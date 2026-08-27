// ESP32-C3 + CC1101 dongle case
// Companion to hw/esp32c3_cc1101_dongle.kicad_pcb
//
// Board/hole dimensions below are read directly from hw/pcb.py (MEASURED).
// Component heights, the reset-button tunnel position, and the LED/cable
// dimensions are ESTIMATES based on typical ESP32-C3 Super Mini layout --
// verify against your actual modules before printing, especially
// button_hole_x/y and the pin/hook dimensions (test-fit before committing).
//
// Assembly: no screws. The PCB sits on 4 pads and is located by pins that
// poke up through its mounting holes. The top piece slides in lengthwise
// from the open (cable) end and nests INSIDE the tray's own side walls,
// stopping against the tray's closed far-end wall. As it slides home, a
// keyhole-slotted hook post on the underside of the top catches each pin
// and holds the PCB down against its pad -- no separate parts, and it's
// still removable by sliding the top back out. A rail along each of the
// tray's long walls engages a matching groove in the top's skirts, so the
// top itself can't be lifted straight off the tray either -- it only
// comes off by sliding back out the way it went in. The tray's open
// (cable) end has a short fixed stub wall covering its bottom third; the
// top's leading wall picks up right where that stops and covers the
// rest, closing off the whole opening between them except for a slot
// sized to the cable itself.
//
// Render in OpenSCAD, or from the command line:
//   openscad -o tray.stl   -D 'part="tray"' fan_dongle_case.scad
//   openscad -o top.stl    -D 'part="top"'  fan_dongle_case.scad
//   openscad -o preview.png -D 'part="both"' --imgsize=1200,900 --camera=34,12,10,55,0,25,220 fan_dongle_case.scad

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
button_hole_y = 20;     // local Y -- left side when facing the USB port
button_tube_wall = 1;                // tube wall thickness
button_tube_gap_above_board = 2;    // gap between the tube's open bottom
                                       // and the board's top surface -- guess,
                                       // verify against actual button height

// No external LED -- the onboard LED (GPIO8/GND) shows through the vent
// holes in the ceiling instead of needing its own dedicated hole.
// Ventilation: small dot grid across the ceiling, skipped near the button
// tunnel so nothing merges awkwardly close together.
vent_hole_d = 2;
vent_spacing = 6;      // grid pitch
vent_margin = 8;       // keep dots this far from the outer edge
vent_exclude_r = 6;    // skip any dot this close to the button tunnel

// Alignment pins (replace the old screw bosses) + sliding retention hooks.
// UNVALIDATED -- these are first-pass guesses for a friction/clearance fit
// and need a test print before trusting them.
pin_d = 2.2;               // pin diameter -- clearance fit through the board's 2.5mm holes
pin_h = 4;                   // pin height above the pad top (must clear board_t + hook post + slack)
hook_gap_above_board = 0.3; // gap between the board's top surface and the hook post's bottom
hook_od = 5.5;               // hook post outer diameter -- kept small so it clears the skirts
                              // at this design's mount-hole Y positions; re-check if those move
hook_pin_clearance = 0.6;   // extra width around the pin in the hook's hole/slot, for a free slide

// Retention rails: a continuous inward lip along each of the tray's long
// walls, engaging a matching groove cut into the top's skirts. Without
// this, nothing stops the top piece from simply lifting straight up off
// the tray -- the hooks only hold the PCB down against its pins, they
// don't hold the top down against the tray. Runs the full slide length,
// so it doesn't block sliding in/out, only vertical separation.
rail_protrusion = 0.8;   // how far the rail sticks in from the tray wall's inner face
rail_h = 1.5;               // rail height (vertical)
rail_clearance = 0.3;      // clearance around the rail inside the groove

// Near-end stub wall: the tray's near (cable) end is open so the top can
// slide in, but a short fixed wall up to end_wall_frac of the tray's
// height closes off the bottom part on the tray side itself. The top's
// leading wall picks up from exactly where this stops and covers the
// rest, so together they seal the whole opening (minus the cable slot)
// once assembled -- rather than the top being the only thing closing it.
end_wall_frac = 1 / 3;

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
outer_w = cavity_w + 2 * wall_t;
outer_h = cavity_h + 2 * wall_t;
tray_wall_h = standoff_h + board_t + component_clearance; // floor-top to ceiling-bottom
skirt_bot_z = floor_t + standoff_h + board_t; // where the top's skirts/leading wall start, just above the board
ceiling_bot_z = floor_t + tray_wall_h;
ceiling_top_z = ceiling_bot_z + wall_t;
rail_z_lo = skirt_bot_z + (ceiling_bot_z - skirt_bot_z) / 2 - rail_h / 2; // rail vertically centered
                                                                            // in the skirt engagement zone
button_tube_od = button_hole_d + 2 * button_tube_wall;
button_tube_bot_z = skirt_bot_z + button_tube_gap_above_board;
end_wall_top_z = floor_t + tray_wall_h * end_wall_frac;

// Board-local -> case-local (board sits centered in the cavity, offset by
// wall_t + board_margin from the case's own bottom-left corner)
board_origin = [wall_t + board_margin, wall_t + board_margin];

function b2c(p) = [p[0] + board_origin[0], p[1] + board_origin[1]];

$fn = 48; // smooth cylinders

part = "both"; // overridden via -D 'part="tray"' / "top" / "both" for preview

module mount_hole_positions() {
  for (h = mount_holes) translate(b2c(h)) children();
}

// Open-ended U-channel: floor + 2 long side walls + 1 closed far-end wall
// (at X=outer_w). The near end (X=0, cable side) is open, both for the
// top piece to slide in from and for the cable itself to exit through.
module bottom_tray() {
  union() {
    // floor
    cube([outer_w, outer_h, floor_t]);
    // long side walls, full tray height, spanning the whole length
    translate([0, 0, floor_t])
      cube([outer_w, wall_t, tray_wall_h]);
    translate([0, outer_h - wall_t, floor_t])
      cube([outer_w, wall_t, tray_wall_h]);
    // closed far-end wall -- the slide's hard stop
    translate([outer_w - wall_t, 0, floor_t])
      cube([wall_t, outer_h, tray_wall_h]);
    // near-end stub wall -- fixed, doesn't slide, closes the bottom
    // end_wall_frac of the open (cable) end. The top's leading wall picks
    // up right where this stops (see top_slide()), so the two don't
    // occupy the same space -- the top would have nowhere to slide into
    // if this reached as high as the top's own leading wall does.
    translate([0, 0, floor_t])
      cube([wall_t, outer_h, end_wall_top_z - floor_t]);
    // retention rails, full length, engaging the top's skirt grooves so
    // the top can't be lifted straight up once slid into place
    translate([0, wall_t, rail_z_lo])
      cube([outer_w, rail_protrusion, rail_h]);
    translate([0, outer_h - wall_t - rail_protrusion, rail_z_lo])
      cube([outer_w, rail_protrusion, rail_h]);
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

module vent_holes(ceiling_z) {
  btn_pos = [board_origin[0] + button_hole_x, board_origin[1] + button_hole_y];
  for (x = [vent_margin : vent_spacing : outer_w - vent_margin])
    for (y = [vent_margin : vent_spacing : outer_h - vent_margin])
      if (norm([x, y] - btn_pos) > vent_exclude_r)
        translate([x, y, ceiling_z - 0.5])
          cylinder(d = vent_hole_d, h = wall_t + 1);
}

// One retention hook, built at the local origin (XY centered on a pin).
// The entire suspended post -- not just a cap at the bottom -- is shaped
// like the hook: a disk with a pin-clearance hole and a slot cut from its
// +X edge to that hole, extruded the full height from just above the
// board up to the ceiling. +X is the "leading" side, the direction from
// which each pin enters the hook as the top piece slides forward (+X)
// into its seated position.
module hook_post(hook_bot_z, ceiling_top_z) {
  translate([0, 0, hook_bot_z])
    linear_extrude(height = ceiling_top_z - wall_t - hook_bot_z)
      difference() {
        circle(d = hook_od);
        circle(d = pin_d + hook_pin_clearance);
        translate([-0.5, -(pin_d + hook_pin_clearance) / 2])
          square([hook_od / 2 + 1.5, pin_d + hook_pin_clearance]);
      }
}

// Ceiling + 2 skirt walls, nested just inside the tray's long walls with
// fit_clearance, plus a hook_post() over each pin and a leading-edge wall
// that closes off the open (cable) end down to a small round port. Built
// directly in the tray's own coordinate frame (same X/Y/Z as
// bottom_tray()) at its final, fully-seated position -- no translate
// needed to assemble it.
module top_slide() {
  hook_bot_z = skirt_bot_z + hook_gap_above_board;

  difference() {
    union() {
      // ceiling panel, spans the tray's full outer footprint
      translate([0, 0, ceiling_bot_z])
        cube([outer_w, outer_h, wall_t]);
      // skirt walls, nested just inside the tray's long walls
      translate([0, wall_t + fit_clearance, skirt_bot_z])
        cube([outer_w, wall_t, ceiling_bot_z - skirt_bot_z]);
      translate([0, outer_h - 2 * wall_t - fit_clearance, skirt_bot_z])
        cube([outer_w, wall_t, ceiling_bot_z - skirt_bot_z]);
      // leading-edge wall: picks up exactly where the tray's fixed stub
      // wall stops (end_wall_top_z) and covers the rest up to the
      // ceiling -- can't reach all the way to the floor itself, since
      // that space is occupied by the tray's own stub wall, fixed and
      // already in the way of any lower reach as the top slides in
      translate([0, 0, end_wall_top_z])
        cube([wall_t, outer_h, ceiling_top_z - end_wall_top_z]);
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
    // even start sliding on. This way the cable gets plugged into the
    // board first, then the top slides into place around it. Straight
    // sides down to the wall's bottom, rounded off at the top where the
    // cable actually rests.
    translate([-0.5, board_origin[1] + usb_hole_y - usb_hole_d / 2, end_wall_top_z])
      cube([wall_t + 1, usb_hole_d, (skirt_bot_z + usb_hole_z_above_board) - end_wall_top_z]);
    translate([-0.5, board_origin[1] + usb_hole_y, skirt_bot_z + usb_hole_z_above_board])
      rotate([0, 90, 0])
        cylinder(d = usb_hole_d, h = wall_t + 1);
    // grooves matching the tray's retention rails -- sized to just clear
    // the rail (rail_protrusion + rail_clearance deep), not padded out
    // further, so the skirt (wall_t = 2mm thick) keeps solid material
    // behind the groove instead of being cut through
    translate([-1, wall_t, rail_z_lo - rail_clearance])
      cube([outer_w + 2, rail_protrusion + rail_clearance, rail_h + 2 * rail_clearance]);
    translate([-1, outer_h - wall_t - rail_protrusion - rail_clearance, rail_z_lo - rail_clearance])
      cube([outer_w + 2, rail_protrusion + rail_clearance, rail_h + 2 * rail_clearance]);
  }
}

if (part == "tray") {
  bottom_tray();
} else if (part == "top") {
  translate([0, outer_h + 10, 0])
    top_slide();
} else {
  bottom_tray();
  top_slide();
}
