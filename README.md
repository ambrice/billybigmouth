# billybigmouth

This is an Arduino sketch for replacing the songs in a Billy Big Mouth Bass fish.

## Hardware used

1. Teensy 4.1
2. Teensy Audio Shield Rev D
3. Two MX1508 motor controllers to control the mouth and body motors
4. PAM8403 amplifier for audio shield line out to drive the speakers.

## BBMP (Bill Big Mouth Protocol)

Sketch will look for up to 10 .wav files on the SD card (use 44.1 KHz 16-bit signed mono wav files).  For each wav there should be a .dat file with same name except .dat instead of .wav.  That file should be a text file of motor movement instructions.  Each instruction is one line, with a character H,T,M, or R followed by a 6 digit timestamp in milliseconds (must include leading zeros).  The character specifies whether to move the head, tail, mouth or release the head (note that can not move tail while head is moved).
H - Move head forward and keep it there (can not move tail until released)
R - Release the head
T - Move the tail for 100 milliseconds
M - Move the mouth open for 100 milliseconds

So the line
T001200

Would mean to move the tail for 100ms after the song has been playing for 1.2 seconds.  All instructions must be in ascending order of timestamp.

