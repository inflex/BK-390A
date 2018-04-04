# PREFACE

PLEASE NOTE - This software has not yet been tested with a BK-390A meter, it has been written based off the data protocol datasheet only.   AWAITING TESTERS.

A linux version of this software would be very easy to make, the only change would be the serial port related code (setup and data acquisition).


# BK-390A
bk390a.exe - BK Precision 390A Series multimeter CLI data capture software for OBS/logging on Windows
win-bk390a.exe - GUI windowed application

# Requirements

If you want to build this software on Windows, you'll require MinGW https://sourceforge.net/projects/mingw/files/latest/download

# Setup

1) Build bk390a ( Linux mingw64 install required, or mingw64 for Windows)

	make bk390a
	(or)
	make win-bk390a
	


3) Run from the command line

	win-bk390a.exe -z 120 -p 4 -s 2400:7o1 -m -fc #10ff10 -bc #000000 -wx 480 -wy 60 -fw 600

The program will display in text the current meter display and also generate a text file called "bk390a.txt" which can be read in to programs like OpenBroadcaster so you can have a live on-screen-display of the multimeter.

# Usage
	bk390a.exe  -p <comport#> [-s <serial port config>] [-t] [-o <filename>] [-l <filename>] [-m] [-d] [-q]

                BK-Precision 390A Multimeter serial data decoder

                By Paul L Daniels / pldaniels@gmail.com
                v0.1Alpha / January 27, 2018

        -h: This help
        -p <comport>: Set the com port for the meter, eg: -p 2
        -s <[9600|4800|2400|1200]:[7|8][o|e|n][1|2]>, eg: -s 2400:7o1
        -t: Generate a text file containing current meter data (default to bk390a.txt)
        -o <filename>: Set the filename for the meter data ( overrides 'bk390a.txt' )
        -l <filename>: Set logging and the filename for the log
        -d: debug enabled
        -m: show multimeter mode
        -q: quiet output
        -v: show version


        example: bk390a.exe -p 2 -t -o obsdata.txt


