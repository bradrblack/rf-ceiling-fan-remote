// ESP32-C3 + CC1101 dongle case
// Companion to hw/esp32c3_cc1101_dongle.kicad_pcb
//
// Board/hole dimensions below are read directly from hw/pcb.py (MEASURED).
// Component heights, the reset-button tunnel position, and the LED/cable
// hole positions are ESTIMATES based on typical ESP32-C3 Super Mini layout
// -- verify against your actual modules before printing, especially
// button_hole_x/y and usb_hole_z.
//
// Render in OpenSCAD, or from the command line:
//   openscad -o bottom.stl -D 'part="bottom"' fan_dongle_case.scad
//   openscad -o lid.stl    -D 'part="lid"'    fan_dongle_case.scad
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

// USB-C cable exit: modules face the board's short left edge (X=0), so the
// cable exits the case's left wall. Round strain-relief hole, not a
// connector-shaped cutout, since the plan is to solder/attach a cable and
// close the case around it rather than have the connector itself poke out.
usb_hole_d = 6;
usb_hole_y = 12;      // centered on board Y (= ESP32 module Y-center)
usb_hole_z = 6;        // height above case floor -- guess, verify against your module

// Reset-button poke-hole tunnel: button is top-mounted (pressed straight
// down), not on a side edge -- vertical hole through the lid, not the
// wall. Position is a rough estimate (typical Super Minis cluster it near
// the USB-C edge) -- check against your actual module and adjust before
// printing.
button_hole_d = 3;      // fits a toothpick/paperclip/pin
button_hole_x = 8;      // local X, near the left/USB edge
button_hole_y = 4;      // local Y, offset from center toward one side

// External LED, mounted on the lid top, wired in parallel to the onboard
// LED (GPIO8/GND) with its own leads -- no firmware change needed.
led_hole_d = 5;          // standard 5mm through-hole LED

// Ventilation: small dot grid across the lid top, skipped near the LED
// and button holes so nothing merges awkwardly close together.
vent_hole_d = 2;
vent_spacing = 6;      // grid pitch
vent_margin = 8;       // keep dots this far from the lid's outer edge
vent_exclude_r = 6;    // skip any dot this close to the LED/button holes

// ==========================================
// Case construction parameters
// ==========================================
wall_t = 2;               // perimeter wall thickness
floor_t = 2;               // bottom shell floor thickness
board_margin = 2;        // clearance around board edges inside the case
standoff_h = 5;            // floor-to-PCB-bottom height (solder joint clearance)
boss_d = 5;                // mounting boss outer diameter
boss_pilot_d = 2;         // self-tap pilot hole diameter for M2.5 screw
boss_pilot_depth = 4;    // blind hole depth into the boss
lip_h = 2;                  // bottom-shell/lid overlap height
fit_clearance = 0.3;     // FDM friction-fit clearance

// Derived
cavity_w = board_w + 2 * board_margin;
cavity_h = board_h + 2 * board_margin;
outer_w = cavity_w + 2 * wall_t;
outer_h = cavity_h + 2 * wall_t;
bottom_wall_z = standoff_h + board_t + lip_h;

// Board-local -> case-local (board sits centered in the cavity, offset by
// wall_t + board_margin from the case's own bottom-left corner)
board_origin = [wall_t + board_margin, wall_t + board_margin];

function b2c(p) = [p[0] + board_origin[0], p[1] + board_origin[1]];

$fn = 48; // smooth cylinders

part = "both"; // overridden via -D 'part="bottom"' / "lid" / "both" for preview

module mount_hole_positions() {
  for (h = mount_holes) translate(b2c(h)) children();
}

module bottom_shell() {
  difference() {
    union() {
      // floor
      cube([outer_w, outer_h, floor_t]);
      // perimeter wall
      translate([0, 0, floor_t])
        difference() {
          cube([outer_w, outer_h, bottom_wall_z]);
          translate([wall_t, wall_t, -0.5])
            cube([cavity_w, cavity_h, bottom_wall_z + 1]);
        }
      // 4 mounting bosses
      mount_hole_positions()
        translate([0, 0, floor_t])
          cylinder(d = boss_d, h = standoff_h);
    }
    // pilot holes for self-tap screws (blind)
    mount_hole_positions()
      translate([0, 0, floor_t + standoff_h - boss_pilot_depth])
        cylinder(d = boss_pilot_d, h = boss_pilot_depth + 0.5);

    // USB cable strain-relief hole, left wall
    translate([-0.5, board_origin[1] + usb_hole_y, floor_t + usb_hole_z])
      rotate([0, 90, 0])
        cylinder(d = usb_hole_d, h = wall_t + 1);
  }
}

module vent_holes() {
  led_pos = [outer_w / 2, outer_h / 2];
  btn_pos = [board_origin[0] + button_hole_x, board_origin[1] + button_hole_y];
  for (x = [vent_margin : vent_spacing : outer_w - vent_margin])
    for (y = [vent_margin : vent_spacing : outer_h - vent_margin])
      if (norm([x, y] - led_pos) > vent_exclude_r && norm([x, y] - btn_pos) > vent_exclude_r)
        translate([x, y, component_clearance - 0.5])
          cylinder(d = vent_hole_d, h = wall_t + 1);
}

// Built upright in its own natural print orientation: skirt at the bottom
// (Z=0), open cavity, closed top -- prints fine as-is, no rotation needed.
module lid() {
  skirt_w = cavity_w - 2 * fit_clearance;
  skirt_h_ = cavity_h - 2 * fit_clearance;
  difference() {
    union() {
      // skirt: friction-fits inside the bottom shell's lip zone, Z 0..lip_h
      translate([wall_t + fit_clearance, wall_t + fit_clearance, 0])
        cube([skirt_w, skirt_h_, lip_h]);
      // outer wall enclosing the component-clearance cavity, Z 0..component_clearance
      difference() {
        cube([outer_w, outer_h, component_clearance]);
        translate([wall_t, wall_t, -0.5])
          cube([cavity_w, cavity_h, component_clearance + 1]);
      }
      // top panel, closes off the top
      translate([0, 0, component_clearance])
        cube([outer_w, outer_h, wall_t]);
    }
    // external LED hole, centered on lid top
    translate([outer_w / 2, outer_h / 2, component_clearance - 0.5])
      cylinder(d = led_hole_d, h = wall_t + 1);

    // reset button poke-hole tunnel, straight down through the lid top
    translate([board_origin[0] + button_hole_x, board_origin[1] + button_hole_y, component_clearance - 0.5])
      cylinder(d = button_hole_d, h = wall_t + 1);

    // ventilation dot grid
    vent_holes();
  }
}

if (part == "bottom") {
  bottom_shell();
} else if (part == "lid") {
  translate([0, outer_h + 10, 0])
    lid();
} else {
  bottom_shell();
  // Lid's local Z=0 (bottom of its skirt) sits at the PCB's top surface,
  // so the skirt descends into the bottom shell's lip_h zone above the PCB.
  translate([0, 0, floor_t + standoff_h + board_t])
    lid();
}
