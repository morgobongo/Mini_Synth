// Custom Button ("Top Hat" design) for Mode and Octave - Flat Bottom

// Configurable parameters
top_diameter = 7.6;     // Top diameter (slightly less than 8mm for tolerance)
top_height = 5;         // Height of the part protruding from the enclosure
base_diameter = 12;     // Base diameter to keep the button inside the enclosure
base_height = 4;        // Base thickness (flat bottom)

// Render quality (number of facets)
$fn = 64;

// Module to create a cylinder with a rounded top edge
module rounded_top_cylinder(d, h, r_corner) {
    r = d / 2;
    rotate_extrude() {
        hull() {
            square([r - r_corner, h]); // Main body
            square([r, h - r_corner]); // Bottom edges
            translate([r - r_corner, h - r_corner]) circle(r=r_corner); // Rounded corner
        }
    }
}

module custom_button() {
    union() {
        // The wide base (solid)
        cylinder(d=base_diameter, h=base_height);
        
        // The upper part with a rounded top (1.2mm radius)
        translate([0, 0, base_height])
            rounded_top_cylinder(d=top_diameter, h=top_height, r_corner=1.2);
    }
}

// Generate the button
custom_button();
