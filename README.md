# Embedded Systems

Team project for the Embedded Systems course (Computer Science, BSc).

By [Fabio Nicolini](https://github.com/fnicolini) and [Antonio Vivace](https://github.com/avivace).

The development board used is the Silicon Labs **C8051F020**, with an additional board (driven via SMBus) with an accelerometer, a thermometer and an LCD display. The project was developed using Silicon Lab's Keil IDE.

The project implements:

- Continuos detection of the inclination and the temperature;
- Continuos display of the value on the LCD display;
- LCD controls (turn on, off, backlight intensity) with an hardware button.

Using the SMBus, interrupts and PWN techniques.