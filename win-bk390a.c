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
#include <windows.h>
#include <strsafe.h>
#include <shellapi.h>

char VERSION[] = "v0.1-Alpha";
char help[] = " -p <comport#> [-s <serial port config>] [-m] [-d] [-q]\r\n"\
			   "\n"\
			   "\t\tBK-Precision 390A Multimeter serial data decoder\r\n"\
			   "\r\n"\
			   "\t\tBy Paul L Daniels / pldaniels@gmail.com\r\n"\
			   "\t\tv0.1Alpha / January 27, 2018\r\n"\
			   "\r\n"\
			   "\t-h: This help\r\n"\
			   "\t-z: Font size (default 48)\r\n"\
			   "\t-p <comport>: Set the com port for the meter, eg: -p 2\r\n"\
			   "\t-s <[9600|4800|2400|1200]:[7|8][o|e|n][1|2]>, eg: -s 2400:7o1\r\n"\
			   "\t-d: debug enabled\r\n"\
			   "\t(-m: show multimeter mode) Not yet available\r\n"\
			   "\t-q: quiet output\r\n"\
			   "\t-v: show version\r\n"\
			   "\n\n\texample: bk390a.exe -p 2 -s 2400:7o1\r\n"\
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

struct glb {
	uint8_t debug;
	uint8_t comms_enabled;
	uint8_t quiet;
	uint8_t show_mode;
	uint16_t flags;
	uint8_t com_address;
	int font_size;

	char serial_params[1024];
};

/* 
 * We have our file handles as globals only so that
 * we can cleanly close them atexit()
 */
HFONT hFont;
HANDLE hComm;			// Handle to the serial port
HWND hstatic;
char cmd[1024];
int tick = 0;
int newcmd = 0;

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
	g->comms_enabled = 1;
	g->quiet = 0;
	g->show_mode = 0;
	g->flags = 0;
	g->font_size = 48;
	g->com_address = 0;

	g->serial_params[0] = '\0';

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
int parse_parameters( struct glb *g ) {
	LPWSTR *argv;
	int argc;
	int i;
	int fz = 48;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if( NULL == argv ) {
		return 0;
	}

	//   else for( i=0; i<nArgs; i++) printf("%d: %ws\n", i, szArglist[i]);

	for (i = 0; i < argc; i++) {

		if (argv[i][0] == '-') {

			/* parameter */
			switch (argv[i][1]) {
				case 'h':
					fprintf(stdout,"Usage: %s %s", argv[0], help);
					exit(1);
					break;

				case 'z':
					i++;
					if (i < argc) fz = _wtoi(argv[i]);
					if (fz < 10) fz = 10;
					if (fz > 144) fz = 144;
					g->font_size = fz;
					break;

				case 'p':
					/* set address of B35*/
					i++;
					if (i < argc) {
						g->com_address = _wtoi(argv[i]);
//					   	wcstombs(g->com_address, argv[i], sizeof(g->com_address));
					} else {
						fprintf(stderr,"Insufficient parameters; -p <com port>\n");
						exit(1);
					}
					break;

				case 'c':
					g->comms_enabled = 0;
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

				case 'v':
					fprintf(stdout,"%s\r\n", VERSION);
					exit(0);
					break;

				case 's':
					i++;
					if (i < argc) wcstombs(g->serial_params, argv[i], sizeof(g->serial_params));
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

	LocalFree(argv);

	return 0;
}

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
void AddMenus(HWND);

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
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
		PWSTR lpCmdLine, int nCmdShow) {


	char mmode[128];		// String describing multimeter mode (volts, amps etc)
	char mode_separator[] = "\r\n  ";
	char prefix[128];		// Units prefix u, m, k, M etc 
	char units[128];		// Measurement units F, V, A, R
	uint8_t d[256];			// Serial data packet
	uint8_t dps = 0;		// Number of decimal places
	struct glb g;			// Global structure for passing variables around
	int i = 0;				// Generic counter

	MSG  msg ;
	WNDCLASSW wc = {0};
	//char  com_port[256];	// com port path / ie, \\.COM4
	wchar_t com_port[256];	// com port path / ie, \\.COM4
	BOOL  com_read_status;  // return status of various com port functions
	DWORD dwEventMask;      // Event mask to trigger
	char  temp_char;        // Temporary character 
	DWORD bytes_read;       // Number of bytes read by ReadFile()

	cmd[0] = '\0';


#define IDM_FILE_PARAMETERS 1
#define IDM_FILE_QUIT 2

	/* 
	 * Initialise the global structure
	 */
	init( &g );

	/*
	 * Parse our command line parameters
	 */
	parse_parameters( &g );


	/*
	 * Sanity check our parameters
	 */
	if (g.com_address == NULL) {

		snwprintf( com_port, sizeof(com_port), L"\\\\.\\COM%s", "2" );
		//		fprintf(stderr, "Require com port address for BK-390A meter, ie, -p 2\r\n");
		//		exit(1);
	} else {
		//snwprintf( com_port, sizeof(com_port), L"\\\\.\\COM%s", g.com_address );
		snwprintf( com_port, sizeof(com_port), L"\\\\.\\COM%d", g.com_address );
	} 


#define COMMS_ENABLED
#ifdef COMMS_ENABLED
	if (g.comms_enabled) {
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
			wprintf(L"Error on: %s\r\n", com_port );
			fprintf(stderr,"Error! - Port '%s' can't be opened\r\n", g.com_address);
			exit(1);
		} else {
			if (!g.quiet) printf("Port COM%d Opened\r\n", g.com_address);
		}

		/*
		 * Set serial port parameters
		 */
		DCB dcbSerialParams = { 0 };                         // Init DCB structure
		dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

		com_read_status = GetCommState(hComm, &dcbSerialParams);      // Retrieve current settings
		if (com_read_status == FALSE) { fprintf(stderr,"Error in getting GetCommState()\r\n"); }

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
				exit(1);
			}



			p = &(g.serial_params[5]);
			if (*p == '7') dcbSerialParams.ByteSize = 7;         
			else if (*p == '8') dcbSerialParams.ByteSize = 8;        
			else {
				fprintf(stderr,"Invalid serial byte size '%c'\r\n", *p);
				exit(1);
			}

			p++;
			if (*p == 'o') dcbSerialParams.Parity = ODDPARITY;       
			else if (*p == 'e') dcbSerialParams.Parity = EVENPARITY;     
			else if (*p == 'n') dcbSerialParams.Parity = NOPARITY;      
			else {
				fprintf(stderr,"Invalid serial parity type '%c'\r\n", *p);
				exit(1);
			}

			p++;
			if (*p == '1') dcbSerialParams.StopBits = ONESTOPBIT; 
			else if (*p == '2') dcbSerialParams.StopBits = TWOSTOPBITS;
			else {
				fprintf(stderr,"Invalid serial stop bits '%c'\r\n", *p);
				exit(1);
			}

		}

		com_read_status = SetCommState(hComm, &dcbSerialParams); 
		if (com_read_status == FALSE) {
			fprintf(stderr,"Error setting com port configuration (2400/7/1/O etc)\r\n");
			exit(1);

		} else {
			if (!g.quiet) {
				printf("Set DCB structure success\r\n");
				printf("\tBaudrate = %ld\r\n", dcbSerialParams.BaudRate);
				printf("\tByteSize = %ld\r\n", dcbSerialParams.ByteSize);
				printf("\tStopBits = %d\r\n", dcbSerialParams.StopBits);
				printf("\tParity   = %d\r\n", dcbSerialParams.Parity);
			}
		}

		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout         = 50;
		timeouts.ReadTotalTimeoutConstant    = 50;
		timeouts.ReadTotalTimeoutMultiplier  = 10;
		timeouts.WriteTotalTimeoutConstant   = 50;
		timeouts.WriteTotalTimeoutMultiplier = 10;
		if (SetCommTimeouts(hComm, &timeouts) == FALSE) {
			fprintf(stderr,"\tError in setting time-outs\r\n");
			exit(1);

		} else { 
			if (!g.quiet) printf("\tSetting time-outs successful\r\n");
		}


		com_read_status = SetCommMask(hComm, EV_RXCHAR); //Configure Windows to Monitor the serial device for Character Reception
		if (com_read_status == FALSE) {
			fprintf(stderr,"\tError in setting CommMask\r\n");
			exit(1);

		} else {
			if (!g.quiet) printf("\tCommMask successful\r\n");
		}
	}

#endif

	/*
	 *
	 * Now do all the ugly Windows stuff
	 *
	 */

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpszClassName = L"BK390-A Meter";
	wc.hInstance     = hInstance;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpfnWndProc   = WindowProcedure;
	wc.hCursor       = LoadCursor(0, IDC_ARROW);

	RegisterClassW(&wc);
	hstatic = CreateWindowW(wc.lpszClassName, L"BK390-A Meter",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			100, 100, 390, 350, NULL, NULL, hInstance, NULL);

	/* Make the window visible on the screen */
	//ShowWindow (hwnd, nCmdShow);

	hFont = CreateFont(g.font_size,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
			CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Impact"));

	/*
	 * Keep reading, interpreting and converting data until someone
	 * presses ctrl-c or there's an error
	 */
	while (msg.message != WM_QUIT) {
		char *p, *q;
		double v = 0.0;
		int end_of_frame_received = 0;

		units[0] = '\0';
		prefix[0] = '\0';
		mmode[0] = '\0';

		/* 
		 *
		 * Let Windows handle itself first 
		 *
		 */

		//while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
		if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) break;
			TranslateMessage (&msg);
			DispatchMessage (&msg);
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
		com_read_status = WaitCommEvent(hComm, &dwEventMask, NULL); //Wait for the character to be received
		if (com_read_status == FALSE) {
			//			fprintf(stderr,"Error in WaitCommEvent()\r\n");
			snprintf(cmd,sizeof(cmd),"No connect %d  ", ((tick++)%1000));			

		} else {
			/*
			 * If we're not in debug mode, then read the data from the
			 * com port until we get a \n character, which is the 
			 * end-of-frame marker.
			 *
			 * This is the section where we're capturing the data bytes
			 * from the multimeter.
			 *
			 */

			if (g.debug) fprintf(stdout,"DATA START: ");
			end_of_frame_received = 0;
			i = 0;
			do {
				com_read_status = ReadFile(hComm, &temp_char, sizeof(temp_char), &bytes_read, NULL);
				d[i] = temp_char;
				if (g.debug) fprintf(stdout,"%x ", d[i]);
				if (temp_char == '\n') { end_of_frame_received = 1; break; }
				i++;
			} while ((bytes_read > 0) && (i < sizeof(d)));
			if (g.debug) fprintf(stdout,":END\r\n");


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
					case 0: snprintf(cmd, sizeof(cmd), "% 05.0f%s%s  %s%s",v, prefix, units, mode_separator, mmode ); break;
					case 1: snprintf(cmd, sizeof(cmd), "% 06.1f%s%s  %s%s",v/10, prefix, units, mode_separator, mmode ); break;
					case 2: snprintf(cmd, sizeof(cmd), "% 06.2f%s%s  %s%s",v/100, prefix, units, mode_separator, mmode ); break;
					case 3: snprintf(cmd, sizeof(cmd), "% 06.3f%s%s  %s%s",v/1000, prefix, units, mode_separator, mmode ); break;
				}
			}
		} // if com-read status == TRUE

		if (newcmd == 0) {
			newcmd = 1;
			InvalidateRect(hstatic,  NULL, FALSE);
			//usleep(250000UL);
			//			SendMessage(hstatic, WM_USER+11, 1, 2);
			newcmd = 0;
		}


	} // Windows message loop

	CloseHandle(hComm);//Closing the Serial Port

	return (int) msg.wParam;
}




/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)                  /* handle the messages */
	{
		case WM_CREATE:
			//			AddMenus(hwnd);
			break;

		case WM_PAINT:
			HDC hdc;
			PAINTSTRUCT ps;
			DWORD color;
			HFONT holdFont;

			hdc = BeginPaint(hwnd, &ps);
			color = GetSysColor(COLOR_BTNFACE);
			SetBkColor(hdc, color);
			SetTextColor(hdc, RGB(0,0,0));

			holdFont = SelectObject(hdc, hFont );
			TextOutA(hdc, 5, 5, cmd, strlen(cmd));
			SelectObject(hdc, holdFont);
			EndPaint(hwnd, &ps);
			//			fprintf(stderr,"%s\n",cmd);
			newcmd = 0;
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDM_FILE_PARAMETERS:
					break;

				case IDM_FILE_QUIT:
					SendMessage(hwnd, WM_CLOSE, 0, 0);
					break;
			}
			break;

		case WM_DESTROY:
			DeleteObject(hFont);
			PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
			break;
		default:                      /* for messages that we don't deal with */
			return DefWindowProc (hwnd, message, wParam, lParam);
	}

	return 0;
}

void AddMenus(HWND hwnd) {

	HMENU hMenubar;
	HMENU hMenu;

	hMenubar = CreateMenu();
	hMenu = CreateMenu();

	AppendMenuW(hMenu, MF_STRING, IDM_FILE_PARAMETERS, L"&Parameters");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hMenu, MF_STRING, IDM_FILE_QUIT, L"&Quit");

	AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR) hMenu, L"&File");
	SetMenu(hwnd, hMenubar);
}
