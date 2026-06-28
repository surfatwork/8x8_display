This displays dancing lEDs on an 8x8 Max7219 display using an arduino. The left and right displays are rotated top to bottom and left to right.

The wiring test makes sure the wiring is correct. It displays flashing rows, columns and a checkerboard pattern

if it doesnt work, try chganging the type of display. Values are 

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW

#define HARDWARE_TYPE MD_MAX72XX::PAROLA_HW

#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW

#define HARDWARE_TYPE MD_MAX72XX::DR0CR0RR0_HW

