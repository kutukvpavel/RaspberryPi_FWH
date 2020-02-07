# RaspberryPi_FWH
A simple programmer for Intel(R) FWH-compatible flash chips.

It's derived from "Raspberry Pi LPC flasher": http://ponyservis.blogspot.com/p/programming-lpc-flash-using-raspberry-pi.html
BTW, the original author named the project incorrectly: not LPC, but FWH flasher (he flashed SST49LF016C).
Those protocols are similar (they share the same physical layer), but their command sets are vastly different, see:
https://flashrom.org/Technology#Communication_bus_protocol

Currently only reading (of contents and chip/manufacturer ID) is implemented.
Reading has been tested on SST49LF004B, but should work on any FWH-compatible chip, since it doesn't use any Software Command Sequences (SCSes).

There is a good stub for writing, it compiles and executes, but fais to actually write the data. Even the first byte (SST49LF004B requires a 3-byte SCS prior to every byte being written, the code currently send the SCS only once, there is "WriteOneshot" member that has to be checked).

This project actually does not require a Pi, it can be easilly ported to any microcontroller thanks to pure C language and Arduino-like style of wiringPi IO library. File access can be substituted with a UART stream and a simple PC application. Or you could use an SD card.
