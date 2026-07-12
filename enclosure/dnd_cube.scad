// dnd_cube.scad — parametric enclosure for the DND status cube.
//
// A hollow cube (shell) with a friction-fit lid. The 4 side walls each hold a 5 mm
// LED behind a thin diffuser window; wire channels run down each wall to a protoboard
// that sits on standoffs; one wall has a USB-C cutout for charging/flashing.
//
// Faces (before you calibrate — color assignment is whatever you wire, calibration
// sorts it out):  +Y / +X / -Y / -X = the four LED faces.  Top = lid (off).  Floor
// = the "bottom / charge" face where the USB-C cutout lives.
//
// Export: set `part` to "shell" or "lid", Render (F6), then export STL.
//   Print the LID upside-down (skirt up) — it's modeled plate-down for preview.

part = "both";        // "shell" | "lid" | "both"

// ---------------- Overall ----------------
c        = 50;        // outer edge length (mm)
wall     = 2;         // wall / floor thickness
$fn      = 72;
eps      = 0.05;

// ---------------- LEDs ----------------
led_dia   = 5.0;      // LED body diameter (5 mm through-hole)
led_clear = 0.5;      // bore clearance around the LED
led_wall  = 1.2;      // holder tube wall thickness
led_boss  = 8;        // holder tube length (LED seating depth)
led_z     = 25;       // LED center height above the floor (= cube face center)
diffuser  = 1.0;      // wall left over the LED = the glow window (thinner = brighter)

// ---------------- Protoboard ----------------
board_l     = 30;     // board length (X)
board_w     = 20;     // board width  (Y)
board_cx    = 0;      // board center offset X (shift toward the USB wall if needed)
board_cy    = 0;      // board center offset Y
standoff_h  = 6;      // board height above the floor
standoff_od = 6;      // standoff outer diameter
standoff_id = 2.2;    // pilot hole for an M2.5 self-tapping screw

// ---------------- USB-C cutout ----------------
usb_angle = 90;       // which wall: 0=+Y, 90=+X, 180=-Y, 270=-X
usb_w     = 11;       // width  (size for the cable's molded boot, not just the plug)
usb_h     = 6;        // height
usb_z     = 11;       // center height (≈ standoff_h + board + connector)

// ---------------- Wire channels ----------------
chan_w = 3;           // channel width
chan_d = 1.2;         // channel depth into the wall

// ---------------- Lid ----------------
lid_th    = wall;     // top plate thickness (counts toward total height)
lid_skirt = 6;        // depth of the plug-in skirt
fit       = 0.3;      // clearance between skirt and cavity (tune for your printer)

// ---------------- Derived ----------------
inner   = c - 2*wall;
shell_h = c - lid_th; // walls stop short so the lid plate completes the cube

// ======================================================================
module led_holder() {                 // tube on the +Y wall, LED points +Y
  translate([0, c/2 - diffuser - led_boss, led_z])
    rotate([-90,0,0])
      cylinder(h = led_boss, d = led_dia + 2*led_wall);
}
module led_bore() {                    // bore from outside, leaving `diffuser` mm
  translate([0, c/2 - diffuser, led_z])
    rotate([90,0,0])
      cylinder(h = led_boss + wall + eps, d = led_dia + led_clear);
}
module wire_channel() {                // groove down the +Y inner wall to board level
  z0 = wall + standoff_h;
  translate([-chan_w/2, inner/2 - chan_d, z0])
    cube([chan_w, chan_d + eps, led_z - z0]);
}
module usb_cut() {                     // hole through the +Y wall
  translate([-usb_w/2, inner/2 - eps, usb_z - usb_h/2])
    cube([usb_w, wall + 2*eps, usb_h]);
}
module standoffs() {
  for (sx = [-1, 1], sy = [-1, 1])
    translate([board_cx + sx*board_l/2, board_cy + sy*board_w/2, wall])
      difference() {
        cylinder(h = standoff_h, d = standoff_od);
        translate([0,0,-eps]) cylinder(h = standoff_h + 2*eps, d = standoff_id);
      }
}

module shell() {
  difference() {
    union() {
      difference() {                                   // hollow box, open top
        translate([-c/2, -c/2, 0]) cube([c, c, shell_h]);
        translate([-inner/2, -inner/2, wall]) cube([inner, inner, shell_h]);
      }
      for (a = [0:90:270]) rotate([0,0,a]) led_holder();
      standoffs();
    }
    for (a = [0:90:270]) rotate([0,0,a]) { led_bore(); wire_channel(); }
    rotate([0,0,usb_angle]) usb_cut();
  }
}

module lid() {                          // plate at z=0..lid_th, skirt hangs below
  s = inner - 2*fit;
  union() {
    translate([-c/2, -c/2, 0]) cube([c, c, lid_th]);
    translate([-s/2, -s/2, -lid_skirt])
      difference() {
        cube([s, s, lid_skirt]);
        translate([wall, wall, -eps]) cube([s - 2*wall, s - 2*wall, lid_skirt + 2*eps]);
      }
  }
}

// ======================================================================
if (part == "shell" || part == "both") shell();
if (part == "lid") lid();
if (part == "both") translate([c + 12, 0, lid_skirt]) lid();   // preview beside shell
