/*
 * BK Precision Model 390A multimeter data stream reading software
 *
 * V0.1 - January 27, 2018
 *
 * Written by Paul L Daniels (pldaniels@gmail.com)
 * For Louis Rossmann (to facilitate meter display on OBS).
 *
 * Build using MinGW on Windows;
 *		gcc bk390a.c -o bk390.exe
 *
 * Run in command line; (will generate output on screen and to b390a.txt file for OBS)
 *		c:\> bk390.exe -p 4 -t
 *
 * For now you'll have to manually determine which COM port your serial adaptor is 
 * appearing as, ie, COM1 use '-p 1',  COM2 use '-p 2' etc.
 *
 * Exit the program at any time by hitting ctrl-c
 *
 *	
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <wchar.h>
#include <Windows.h>

char help[] = " -p <comport#> [-s <serial port config>] [-t] [-o <filename>] [-l <filename>] [-m] [-d] [-q]\r\n"\
			   "\n"\
			   "\t\tBK-Precision 390A Multimeter serial data decoder\r\n"\
			   "\r\n"\
			   "\t\tBy Paul L Daniels / pldaniels@gmail.com\r\n"\
			   "\t\tv0.1Alpha / January 27, 2018\r\n"\
			   "\r\n"\
			   "\t-h: This help\r\n"\
			   "\t-p <comport>: Set the com port for the meter, eg: -p 2\r\n"\
			   "\t-s <[9600|4800|2400|1200]:[7|8][o|e|n][1|2]>, eg: -s 2400:7o1\r\n"\
			   "\t-t: Generate a text file containing current meter data (default to bk390a.txt)\r\n"\
			   "\t-o <filename>: Set the filename for the meter data ( overrides 'bk390a.txt' )\r\n"\
			   "\t-l <filename>: Set logging and the filename for the log\r\n"\
			   "\t-d: debug enabled\r\n"\
			   "\t-m: show multimeter mode\r\n"\
			   "\t-q: quiet output\r\n"\
			   "\n\n\texample: bk390a.exe -p 2 -t -o obsdata.txt\r\n"\
			   "\r\n";

#define BYTE_RANGE 0
#define BYTE_DIGIT_3 1
#define BYTE_DIGIT_2 2
#define BYTE_DIGIT_1 3
#define BYTE_DIGIT_0 4
#define BYTE_FUNCTION 5
#define BYTE_STATUS 6
#define BYTE_OPTION_1 7
#define BYTE_OPTION_2 8

#define FUNCTION_VOLTAGE	0b00111011
#define FUNCTION_CURRENT_UA			0b00111101 
#define FUNCTION_CURRENT_MA			0b00111001
#define FUNCTION_CURRENT_A			0b00111111
#define FUNCTION_OHMS		0b00110011 
#define FUNCTION_CONTINUITY	0b00110101
#define FUNCTION_DIODE		0b00110001
#define FUNCTION_FQ_RPM		0b00110010
#define FUNCTION_CAPACITANCE 0b00110110
#define FUNCTION_TEMPERATURE 0b00110100
#define FUNCTION_ADP0		0b00111110
#define FUNCTION_ADP1		0b00111100
#define FUNCTION_ADP2		0b00111000
#define FUNCTION_ADP3		0b00111010

#define STATUS_OL			0x01
#define STATUS_BATT			0x02
#define STATUS_SIGN			0x04 
#define STATUS_JUDGE		0x08

#define OPTION1_VAHZ		0x01
#define OPTION1_PMIN		0x04 
#define OPTION1_PMAX		0x08

#define OPTION2_APO			0x01
#define OPTION2_AUTO		0x02
#define OPTION2_AC			0x04
#define OPTION2_DC			0x08

char default_output[] = "bk390a.txt";
uint8_t sigint_pressed;

struct glb {
	uint8_t debug;
	uint8_t quiet;
	uint8_t show_mode;
	uint8_t textfile_output;
	uint16_t flags;

	char *serial_params;
	char *log_filename;
	char *output_filename;
	char *com_address;
};



/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220248
  Function Name	: init
  Returns Type	: int
  ----Parameter List
  1. struct glb *g , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int init( struct glb *g ) {
	g->debug = 0;
	g->quiet = 0;
	g->show_mode = 0;
	g->flags = 0;
	g->textfile_output = 0;

	g->output_filename = default_output;
	g->com_address = NULL;
	g->log_filename = NULL;
	g->serial_params = NULL;

	return 0;
}

/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220258
  Function Name	: parse_parameters
  Returns Type	: int
  ----Parameter List
  1. struct glb *g, 
  2.  int argc, 
  3.  char **argv , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int parse_parameters( struct glb *g, int argc, char **argv ) {

	int i;

	for (i = 0; i < argc; i++) {

		if (argv[i][0] == '-') {

			/* parameter */
			switch (argv[i][1]) {
				case 'h':
					fprintf(stdout,"Usage: %s %s", argv[0], help);
					exit(1);
					break;

				case 'l':
					/* set the logging */
					i++;
					if (i < argc) g->log_filename = argv[i];
					else {
						fprintf(stderr,"Require log filename; -l <filename>\n");
						exit(1);
					}
					break;

				case 'p':
					/* set address of B35*/
					i++;
					if (i < argc) g->com_address = argv[i];
					else {
						fprintf(stderr,"Insufficient parameters; -p <com port>\n");
						exit(1);
					}
					break;

				case 'o':
					/* set output file for text */
					i++;
					if (i < argc) g->output_filename = argv[i];
					else {
						fprintf(stderr,"Insufficient parameters; -o <file>\n");
						exit(1);
					}
					break;

				case 't':
					g->textfile_output = 1;
					break;

				case  'd':
					g->debug = 1;
					break;

				case 'q':
					g->quiet = 1;
					break;

				case 'm':
					g->show_mode = 1;
					break;

				case 's':
					i++;
					if (i < argc) g->serial_params = argv[i];
					else {
						fprintf(stderr,"Insufficient parameters; -s <parameters> [eg 9600:8:o:1] = 9600, 8-bit, odd, 1-stop\n");
						exit(1);
					}
					break;

				default:
						break;
			} // switch
		}
	}

	return 0;
}


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220304
  Function Name	: handle_sigint
  Returns Type	: void
  ----Parameter List
  1. int a , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
void handle_sigint( int a ) {
	sigint_pressed = 1;
}


/*-----------------------------------------------------------------\
  Date Code:	: 20180128-001515
  Function Name	: set_cursor_visible
  Returns Type	: void
  ----Parameter List
  1. uint8_t vis , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
void set_cursor_visible( uint8_t vis ) {
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO info;
	info.dwSize = 100;
	info.bVisible = vis;
	SetConsoleCursorInfo(consoleHandle, &info);
}


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220307
  Function Name	: main
  Returns Type	: int
  ----Parameter List
  1. int argc, 
  2.  char **argv , 
  ------------------
  Exit Codes	: 
  Side Effects	: 
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int main( int argc, char **argv ) {
	char cmd[1024];
	char mmode[128];		// String describing multimeter mode (volts, amps etc)
	char mode_separator[] = "\r\n  ";
	char prefix[128];		// Units prefix u, m, k, M etc 
	char units[128];		// Measurement units F, V, A, R
	uint8_t d[256];			// Serial data packet
	uint8_t dps = 0;		// Number of decimal places
	uint32_t logscale = 1;	// What scale do we multiple the screen values for in the log
	FILE *fo, *fl;			// Output file handles for OBS output, and log output
	struct timeval t0, t1;	// Time values for the log files
	uint64_t t0i, t1i;
	struct timezone tz;
	struct glb g;			// Global structure for passing variables around
	int i = 0;				// Generic counter
	char hbc = ' ';			// Heart-beat character

	HANDLE hComm;			// Handle to the serial port
	char  com_port[256];	// com port path / ie, \\.COM4
	BOOL  com_read_status;  // return status of various com port functions
	DWORD dwEventMask;      // Event mask to trigger
	char  temp_char;        // Temporary character 
	DWORD bytes_read;       // Number of bytes read by ReadFile()

	t0i = t1i = 0;
	fo = fl = NULL;

	if (argc == 1) {
	   	fprintf(stdout,"Usage: %s %s", argv[0], help);
		exit(1);
	}

	/*
	 * Set up the signal handler for when someone presses ctrl-c
	 */
	sigint_pressed = 0;
	signal(SIGINT, handle_sigint); 

	/* 
	 * Initialise the global structure
	 */
	init( &g );

	/*
	 * Parse our command line parameters
	 */
	parse_parameters( &g, argc, argv );


	/*
	 * Sanity check our parameters
	 */
	if (g.com_address == NULL) {
		fprintf(stderr, "Require com port address for BK-390A meter, ie, -p 2\r\n");
		exit(1);
	} else {
		snprintf( com_port, sizeof(com_port), "\\\\.\\COM%s", g.com_address );
	} 


	fprintf(stdout,"BK-Precision 390A Multimeter serial data decoder\n"\
			"\n"\
			"  By Paul L Daniels / pldaniels@gmail.com\n"\
			"  v0.1Alpha / January 27, 2018\n"\
			"\n"\
		   );
	/*
	 * Open the serial port 
	 */
	hComm = CreateFile( com_port,                  // Name of port 
			GENERIC_READ,			// Read Access
			0,                      // No Sharing
			NULL,                   // No Security
			OPEN_EXISTING,          // Open existing port only
			0,                      // Non overlapped I/O
			NULL);                  // Null for comm devices

	/*
	 * Check the outcome of the attempt to create the handle for the com port
	 */
	if (hComm == INVALID_HANDLE_VALUE) {
		printf("Error! - Port %s can't be opened\r\n", com_port);
		exit(1);
	} else {
		printf("Port %s Opened\r\n", com_port);
	}

	/*
	 * Set serial port parameters
	 */
	DCB dcbSerialParams = { 0 };                         // Init DCB structure
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	com_read_status = GetCommState(hComm, &dcbSerialParams);      // Retrieve current settings
	if (com_read_status == FALSE) { printf("Error in getting GetCommState()\r\n"); }

	dcbSerialParams.BaudRate = CBR_2400;   
	dcbSerialParams.ByteSize = 7;         
	dcbSerialParams.StopBits = ONESTOPBIT; 
	dcbSerialParams.Parity = ODDPARITY;       

	if (g.serial_params) {

		char *p = g.serial_params;

		if (strncmp(p, "9600:", 5)==0) dcbSerialParams.BaudRate = CBR_9600;      // BaudRate = 9600
		else if (strncmp(p, "4800:", 5)==0) dcbSerialParams.BaudRate = CBR_4800;      // BaudRate = 4800
		else if (strncmp(p, "2400:", 5)==0) dcbSerialParams.BaudRate = CBR_2400;      // BaudRate = 2400
		else if (strncmp(p, "1200:", 5)==0) dcbSerialParams.BaudRate = CBR_1200;      // BaudRate = 1200
		else {
			fprintf(stderr,"Invalid serial speed\r\n");
			set_cursor_visible(TRUE);
			exit(1);
		}



		p = &(g.serial_params[5]);
		if (*p == '7') dcbSerialParams.ByteSize = 7;         
		else if (*p == '8') dcbSerialParams.ByteSize = 8;        
		else {
			fprintf(stderr,"Invalid serial byte size '%c'\r\n", *p);
			set_cursor_visible(TRUE);
			exit(1);
		}

		p++;
		if (*p == 'o') dcbSerialParams.Parity = ODDPARITY;       
		else if (*p == 'e') dcbSerialParams.Parity = EVENPARITY;     
		else if (*p == 'n') dcbSerialParams.Parity = NOPARITY;      
		else {
			fprintf(stderr,"Invalid serial parity type '%c'\r\n", *p);
			set_cursor_visible(TRUE);
			exit(1);
		}

		p++;
		if (*p == '1') dcbSerialParams.StopBits = ONESTOPBIT; 
		else if (*p == '2') dcbSerialParams.StopBits = TWOSTOPBITS;
		else {
			fprintf(stderr,"Invalid serial stop bits '%c'\r\n", *p);
			set_cursor_visible(TRUE);
			exit(1);
		}

	}

	com_read_status = SetCommState(hComm, &dcbSerialParams); 
	if (com_read_status == FALSE) {
		printf("Error setting com port configuration (2400/7/1/O etc)\r\n");
		exit(1);

	} else {
		printf("Set DCB structure success\r\n");
		printf("\tBaudrate = %d\r\n", dcbSerialParams.BaudRate);
		printf("\tByteSize = %d\r\n", dcbSerialParams.ByteSize);
		printf("\tStopBits = %d\r\n", dcbSerialParams.StopBits);
		printf("\tParity   = %d\r\n", dcbSerialParams.Parity);
	}

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout         = 50;
	timeouts.ReadTotalTimeoutConstant    = 50;
	timeouts.ReadTotalTimeoutMultiplier  = 10;
	timeouts.WriteTotalTimeoutConstant   = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	if (SetCommTimeouts(hComm, &timeouts) == FALSE) {
		printf("\tError in setting time-outs\r\n");
		exit(1);

	} else { 
		printf("\tSetting time-outs successful\r\n");
	}


	com_read_status = SetCommMask(hComm, EV_RXCHAR); //Configure Windows to Monitor the serial device for Character Reception
	if (com_read_status == FALSE) {
		printf("\tError in setting CommMask\r\n");
		exit(1);

	} else {
		printf("\tCommMask successful\r\n");
	}



	/*
	 * If required, open the log file, in append mode
	 *
	 */
	if (g.log_filename) {

		fl = fopen(g.log_filename, "a");
		if (fl == NULL) {
			fprintf(stderr,"Couldn't open '%s' file to write/append, NO LOGGING\r\n", g.log_filename);
		}

		/* set "now" to be the log 'zero' time */
		gettimeofday( &t0, &tz );
		t0i = t0.tv_sec *10 +(t0.tv_usec /100000);
	}

	/*
	 * If required, open the text file we're going to generate the multimeter
	 * data in to, this is a single frame only data file it is NOT a log file
	 *
	 */
	if (g.textfile_output) {
		fo = fopen("bk390a.txt","w");
		if (fo == NULL) {
			fprintf(stderr,"Couldn't open bk390a.data file to write, not saving to file\r\n");
			g.textfile_output = 0;
		}
	}

	fprintf(stdout,"\r\nPress Ctrl-C to exit\r\n---------------\r\n");

	set_cursor_visible(FALSE);

	/*
	 * Keep reading, interpreting and converting data until someone
	 * presses ctrl-c or there's an error
	 */
	while (1) {
		char *p, *q;
		double v = 0.0;
		int end_of_frame_received = 0;

		units[0] = '\0';
		prefix[0] = '\0';
		mmode[0] = '\0';
		logscale = 1;


		/*
		 * If the ctrl-c was pressed, then clean up things
		 * and exit 
		 */
		if (sigint_pressed) {
			set_cursor_visible(TRUE);
			if (fo) fclose(fo);
			if (fl) fclose(fl);
			fprintf(stdout, "Exit requested\r\n");
			fflush(stdout);
			exit(1);
		}


		/*
		 * Time to start receiving the serial block data 
		 *
		 * We initially "stage" here waiting for there to 
		 * be something happening on the com port.  Soon as
		 * something happens, then we move forward to trying
		 * to read the data.
		 *
		 */
		if (g.debug == 0) {
			com_read_status = WaitCommEvent(hComm, &dwEventMask, NULL); //Wait for the character to be received
		} else {
			com_read_status = TRUE;
		}


		if (com_read_status == FALSE) {
			printf("Error in WaitCommEvent()\r\n");

		} else {
			if (g.debug == 0) {

				/*
				 * If we're not in debug mode, then read the data from the
				 * com port until we get a \n character, which is the 
				 * end-of-frame marker.
				 *
				 * This is the section where we're capturing the data bytes
				 * from the multimeter.
				 *
				 */
				end_of_frame_received = 0;
				i = 0;
				do {
					com_read_status = ReadFile(hComm, &temp_char, sizeof(temp_char), &bytes_read, NULL);
					d[i] = temp_char;
					if (temp_char == '\n') { end_of_frame_received = 1; break; }
					i++;
				} while ((bytes_read > 0) && (i < sizeof(d)));

			} else {
				/*
				 * Fudge some data here so we can test the
				 * output generation of the screen and files
				 *
				 */
				d[BYTE_RANGE] = 0b00110001;
				d[BYTE_FUNCTION] = 0b00111011;
				d[BYTE_STATUS] = 0b00110100;
				d[BYTE_OPTION_1] = 0;
				d[BYTE_OPTION_2] = 0b00111011;

				d[1] = 4;
				d[2] = 3;
				d[3] = 2;
				d[4] = 1;
				sleep(1);
			}
		}	


		/*
		 * Decode our data.  
		 *
		 * While the data sheet gives a very nice matrix for the RANGE and FUNCTION values
		 * it's probably more human-readable to break it down in to longer code on a per
		 * function selection.
		 *
		 */
		switch (d[BYTE_FUNCTION]) {

			case FUNCTION_VOLTAGE: 
				snprintf(units,sizeof(units),"V");
				snprintf(mmode,sizeof(mmode),"Volts");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 1; snprintf(prefix, sizeof(prefix), "m"); break;
					case 1: dps = 3; break;
					case 2: dps = 2; break;
					case 3: dps = 1; break;
					case 4: dps = 0; break;
				} // test the range byte for voltages
				break; // FUNCTION_VOLTAGE


			case FUNCTION_CURRENT_UA:
				snprintf(units,sizeof(units),"A");
				snprintf(prefix,sizeof(prefix),"m");
				snprintf(mmode,sizeof(mmode),"Amps");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 2; break;
					case 1: dps = 1; break;
				}
				break; // FUNCTION_CURRENT_UA



			case FUNCTION_CURRENT_MA:
				snprintf(units,sizeof(units),"A");
				snprintf(prefix,sizeof(prefix),"μ");
				snprintf(mmode,sizeof(mmode),"Amps");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 1; break;
					case 1: dps = 0; break;
				}
				break; // FUNCTION_CURRENT_MA


			case FUNCTION_CURRENT_A:
				snprintf(units,sizeof(units),"A");
				snprintf(mmode,sizeof(mmode),"Amps");
				break; // FUNCTION_CURRENT_A



			case FUNCTION_OHMS:
				snprintf(mmode,sizeof(mmode),"Resistance");
				snprintf(units,sizeof(units),"Ω");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 1; break;
					case 1: dps = 3; snprintf(prefix,sizeof(prefix),"k"); break;
					case 2: dps = 2; snprintf(prefix,sizeof(prefix),"k"); break;
					case 3: dps = 1; snprintf(prefix,sizeof(prefix),"k"); break;
					case 4: dps = 3; snprintf(prefix,sizeof(prefix),"M"); break;
					case 5: dps = 2; snprintf(prefix,sizeof(prefix),"M"); break;
				}
				break; // FUNCTION_OHMS



			case FUNCTION_CONTINUITY:
				snprintf(mmode,sizeof(mmode),"Continuity");
				snprintf(units,sizeof(units),"Ω");
				dps = 1;
				break; // FUNCTION_CONTINUITY


			case FUNCTION_DIODE:
				snprintf(mmode,sizeof(mmode),"Diode");
				snprintf(units,sizeof(units),"V");
				dps = 1;
				break; // FUNCTION_DIODE


			case FUNCTION_FQ_RPM:
				if (d[BYTE_STATUS] & STATUS_JUDGE) {
					snprintf(mmode,sizeof(mmode),"Frequency");
					snprintf(units,sizeof(units),"Hz");
					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 3; snprintf(prefix, sizeof(prefix),"k"); break;
						case 1: dps = 2; snprintf(prefix, sizeof(prefix),"k"); break;
						case 2: dps = 1; snprintf(prefix, sizeof(prefix),"k"); break;
						case 3: dps = 3; snprintf(prefix, sizeof(prefix),"M"); break;
						case 4: dps = 2; snprintf(prefix, sizeof(prefix),"M"); break;
						case 5: dps = 1; snprintf(prefix, sizeof(prefix),"M"); break;
					} // switch

				} else {
					snprintf(mmode,sizeof(mmode),"RPM");
					snprintf(units,sizeof(units),"rpm");
					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 2; snprintf(prefix, sizeof(prefix),"k"); break;
						case 1: dps = 1; snprintf(prefix, sizeof(prefix),"k"); break;
						case 2: dps = 3; snprintf(prefix, sizeof(prefix),"M"); break;
						case 3: dps = 2; snprintf(prefix, sizeof(prefix),"M"); break;
						case 4: dps = 1; snprintf(prefix, sizeof(prefix),"M"); break;
						case 5: dps = 0; snprintf(prefix, sizeof(prefix),"M"); break;
					} // switch
				}
				break; // FUNCTION_FQ_RPM



			case FUNCTION_CAPACITANCE:
				snprintf(mmode,sizeof(mmode),"Capacitance");
				snprintf(units,sizeof(units),"F");
				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 3; snprintf(prefix, sizeof(prefix),"n"); break;
					case 1: dps = 2; snprintf(prefix, sizeof(prefix),"n"); break;
					case 2: dps = 1; snprintf(prefix, sizeof(prefix),"n"); break;
					case 3: dps = 3; snprintf(prefix, sizeof(prefix),"μ"); break;
					case 4: dps = 2; snprintf(prefix, sizeof(prefix),"μ"); break;
					case 5: dps = 1; snprintf(prefix, sizeof(prefix),"μ"); break;
					case 6: dps = 3; snprintf(prefix, sizeof(prefix),"m"); break;
					case 7: dps = 2; snprintf(prefix, sizeof(prefix),"m"); break;
				}
				break; // FUNCTION_CAPACITANCE

			case FUNCTION_TEMPERATURE:
				snprintf(mmode,sizeof(mmode),"Temperature");
				if (d[BYTE_STATUS] & STATUS_JUDGE) {
					snprintf(units,sizeof(units),"'C");
				} else {
					snprintf(units,sizeof(units),"'F");
				}
				break; // FUNCTION_TEMPERATURE

		}


		/*
		 * Decode the digit data in to human-readable 
		 *
		 * first byte is the sign +/-
		 * bytes 1..4 are ASCII char codes for 0000-9999
		 *
		 */
		v = ((d[1] & 0x0F ) *1000) 
			+((d[2] & 0x0F ) *100)
			+((d[3] & 0x0F ) *10)
			+((d[4] & 0x0F ) *1);

		/*
		 * If we're not showing the meter mode, then just
		 * zero the string we generated previously
		 */
		if (g.show_mode == 0) {
		   	mmode[0] = 0;
			mode_separator[0] = 0;
		} 

		/** range checks **/
		if ( (d[BYTE_STATUS] & STATUS_OL) == 1 ) {
			snprintf(cmd, sizeof(cmd), "O.L." );

		} else {
			if (dps < 0) dps = 0;
			if (dps > 3) dps = 3;

			switch (dps) {
				case 0: snprintf(cmd, sizeof(cmd), "% 05.0f%s%s%s%s",v, prefix, units, mode_separator, mmode ); break;
				case 1: snprintf(cmd, sizeof(cmd), "% 06.1f%s%s%s%s",v/10, prefix, units, mode_separator, mmode ); break;
				case 2: snprintf(cmd, sizeof(cmd), "% 06.2f%s%s%s%s",v/100, prefix, units, mode_separator, mmode ); break;
				case 3: snprintf(cmd, sizeof(cmd), "% 06.3f%s%s%s%s",v/1000, prefix, units, mode_separator, mmode ); break;
			}
		}

		/*
		 * If we're generating the output file for OBS
		 * then rewind and rewrite the file each time.
		 *
		 */
		if (g.textfile_output) {
			rewind(fo);
			fprintf(fo, "%s%c", cmd, 0);
			fflush(fo);
		}

		/*
		 * If we're generating the lof file, make sure we
		 * put down the time-delta 
		 *
		 * FIXME: we don't yet set the appropriate logscale in the
		 *			range/function switch statement sequence
		 *
		 */
		if (g.log_filename && fl) {
			gettimeofday( &t1, &tz );
			t1i = t1.tv_sec *10 +(t1.tv_usec /100000);
			fprintf(fl, "%0.1f %0.6f %s\n"
					, (t1i -t0i)/10.0
					, v /logscale
					, units 
				   );
			fflush(fl);
		}

		if (!g.quiet) {
			//			fprintf(stdout, "\33[2K\r"); // line erase
			//			fprintf(stdout, "\x1B[2A"); // line up
			//			fprintf(stdout, "\33[2K\r"); // line erase
			//		if (g.debug) fprintf(stdout,"[ %03d %03d %03d %03d %03d %03d ]", d[6], d[7], d[8], d[9], d[10], d[11]);
			fprintf(stdout,"\r%c %s", hbc, cmd );
			fflush(stdout);
			if (hbc == ' ') hbc = '.'; else hbc = ' ';

		}

	}

	CloseHandle(hComm);//Closing the Serial Port

	return 0;
}
