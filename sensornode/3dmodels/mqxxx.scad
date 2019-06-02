$fn=60;
union() {
    for (i=[0:45:359]) {
        rotate(i) translate([4.75, 0, -6]) {
            if (i != 0 && i != 180) {
                color("silver") cylinder(d=1,h=6);
                translate([0,0,0.05]) intersection() {
                    translate([0,0,-0.3])cube([1,1,0.5], center=true);
                    sphere(0.5);
                }
            }
        }
    }
    color("seagreen") cylinder(d=16.8, h=5);
    color("lightgray")translate([0,0,3]) cylinder(d1=14, d2=10, h=23-5-6.5);
}
