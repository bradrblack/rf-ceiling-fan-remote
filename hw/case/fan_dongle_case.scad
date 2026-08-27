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
// keyhole-slotted hook tab on the underside of the top catches each pin
// and holds the PCB down against its pad -- no separate parts, and it's
// still removable by sliding the top back out.
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

// USB-C cable: modules face the board's short left edge (X=0), and that
// whole end of the tray is open (no wall) so the top piece can slide in
// from there -- the cable just exits through that same open port, bounded
// above by the top's ceiling and on the sides by the tray's long walls.
// usb_hole_y kept for reference/future strain-relief notch, not currently
// used to cut anything.
usb_hole_y = 12;      // centered on board Y (= ESP32 module Y-center)

// Reset-button poke-hole tunnel: button is top-mounted (pressed straight
// down) -- vertical hole through the top piece's ceiling. Position is a
// rough estimate (typical Super Minis cluster it near the USB-C edge) --
// check against your actual module and adjust before printing.
button_hole_d = 3;      // fits a toothpick/paperclip/pin
button_hole_x = 8;      // local X, near the left/USB edge
button_hole_y = 20;     // local Y -- left side when facing the USB port

// External LED, mounted on the top piece's ceiling, wired in parallel to
// the onboard LED (GPIO8/GND) with its own leads -- no firmware change needed.
led_hole_d = 5;          // standard 5mm through-hole LED

// Ventilation: small dot grid across the ceiling, skipped near the LED
// and button holes so nothing merges awkwardly close together.
vent_hole_d = 2;
vent_spacing = 6;      // grid pitch
vent_margin = 8;       // keep dots this far from the outer edge
vent_exclude_r = 6;    // skip any dot this close to the LED/button holes

// Alignment pins (replace the old screw bosses) + sliding retention hooks.
// UNVALIDATED -- these are first-pass guesses for a friction/clearance fit
// and need a test print before trusting them.
pin_d = 2.2;               // pin diameter -- clearance fit through the board's 2.5mm holes
pin_h = 4;                   // pin height above the pad top (must clear board_t + hook_h + slack)
hook_gap_above_board = 0.3; // gap between the board's top surface and the hook tab's bottom
hook_h = 1.6;                // hook tab thickness (vertical)
hook_od = 5.5;               // hook tab outer diameter -- kept small so it clears the skirts
                              // at this design's mount-hole Y positions; re-check if those move
hook_pin_clearance = 0.6;   // extra width around the pin in the hook's hole/slot, for a free slide
hook_rib_w = 2.5;            // width of the support rib connecting each hook up to the ceiling

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
  led_pos = [outer_w / 2, outer_h / 2];
  btn_pos = [board_origin[0] + button_hole_x, board_origin[1] + button_hole_y];
  for (x = [vent_margin : vent_spacing : outer_w - vent_margin])
    for (y = [vent_margin : vent_spacing : outer_h - vent_margin])
      if (norm([x, y] - led_pos) > vent_exclude_r && norm([x, y] - btn_pos) > vent_exclude_r)
        translate([x, y, ceiling_z - 0.5])
          cylinder(d = vent_hole_d, h = wall_t + 1);
}

// One retention hook + its support rib, built at the local origin (XY
// centered on a pin). The hook is a disk with a pin-clearance hole and a
// slot cut from its +X edge to that hole -- +X is the "leading" side, the
// direction from which each pin enters the hook as the top piece slides
// forward (+X) into its seated position. The rib rises from the disk's
// solid trailing (-X) half, clear of the slot, up to the ceiling.
module hook_and_rib(hook_bot_z, ceiling_top_z) {
  union() {
    translate([0, 0, hook_bot_z])
      difference() {
        cylinder(d = hook_od, h = hook_h);
        cylinder(d = pin_d + hook_pin_clearance, h = hook_h + 1);
        translate([0, -(pin_d + hook_pin_clearance) / 2, -0.5])
          cube([hook_od / 2 + 1, pin_d + hook_pin_clearance, hook_h + 1]);
      }
    translate([-hook_od / 2, -hook_rib_w / 2, hook_bot_z])
      cube([hook_od / 2, hook_rib_w, ceiling_top_z - wall_t - hook_bot_z]);
  }
}

// Ceiling + 2 skirt walls, nested just inside the tray's long walls with
// fit_clearance, plus a hook_and_rib() over each pin. Built directly in
// the tray's own coordinate frame (same X/Y/Z as bottom_tray()) at its
// final, fully-seated position -- no translate needed to assemble it.
module top_slide() {
  skirt_bot_z = floor_t + standoff_h + board_t; // rests just above the board
  ceiling_bot_z = floor_t + tray_wall_h;
  ceiling_top_z = ceiling_bot_z + wall_t;
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
      // retention hooks, one per mounting pin
      mount_hole_positions()
        hook_and_rib(hook_bot_z, ceiling_top_z);
    }
    // external LED hole
    translate([outer_w / 2, outer_h / 2, ceiling_bot_z - 0.5])
      cylinder(d = led_hole_d, h = wall_t + 1);
    // reset button poke-hole tunnel
    translate([board_origin[0] + button_hole_x, board_origin[1] + button_hole_y, ceiling_bot_z - 0.5])
      cylinder(d = button_hole_d, h = wall_t + 1);
    // ventilation dot grid
    vent_holes(ceiling_bot_z);
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
