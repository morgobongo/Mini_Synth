// 20 cubes of 4x4x1 mm
size = 4;
height = 1;
spacing = 2; // 2mm space between each part
columns = 5;

for (i = [0 : 19]) {
    x = (i % columns) * (size + spacing);
    y = floor(i / columns) * (size + spacing);
    
    translate([x, y, 0])
        cube([size, size, height]);
}
