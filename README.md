# RaspberryPi_FWH
A simple programmer for Intel(R) FWH-compatible flash chips.

It's derived from "Raspberry Pi LPC flasher": http://ponyservis.blogspot.com/p/programming-lpc-flash-using-raspberry-pi.html
BTW, the original author named the project incorrectly: not LPC but FWH flasher.
Those protocols are similar (they share physical layer), but their command sets are vastly different, see:
https://flashrom.org/Technology#Communication_bus_protocol.

Currently only reading (contents and chip/manufacturer ID) is implemented, but the rest of the code can be ported easily enough too.
