# TXT-C64Basic
Allowing C64 Basic programs (for old interfaces) to run directly on a TXT. For user input an SSH connection (eg. PuTTY) is needed because the TXT lacks a keyboard.
The goal is to run the C64 fishertechnik programs "as they are" directly (i.e. interpreted) on a TXT. It emulates a vintage parallel interface and the Commodore 64 screen.
EX/EY are mapped to I1/I2 and I1-I4 are mapped to C1-C4, I5-I8 are left as they are. M1-M4 (O1-O8) are also as they are and work only in on/off mode because the old interfaces do not support PWM.
The fischertechnik Basic programs include 6502 machine code, this code is not interpreted or emulated but instead it is just loaded and any calls to it are captured and dedicated routines are called instead. This pertains to motor control, turtle graphics, image display and some robot commands.
The code is poorly tested and only a limited set of the existing BASIC programs were ever run, so bugs must be expected.
