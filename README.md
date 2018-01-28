# BK-390A
BK Precision 390A Series multimeter CLI data capture software for OBS/logging

# Requirements

# Setup

1) Build bk390a

	gcc bk390a.c -o bk390a.exe 


3) Run from the command line

	bk390a.exe -p 4 -t 

The program will display in text the current meter display and also generate a text file called "bk390a.txt" which can be read in to programs like OpenBroadcaster so you can have a live on-screen-display of the multimeter.

# Usage

	
