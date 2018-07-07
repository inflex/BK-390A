/*
 * BK Precision Model 390A multimeter data stream reading software
 *
 * V0.1 - January 27, 2018
 * V0.2 - April 4, 2018
 *
 * Written by Paul L Daniels (pldaniels@gmail.com)
 * For Louis Rossmann (to facilitate meter display on OBS).
 *
 */

#include <windows.h>
#include <shellapi.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <sys/time.h>
#include <unistd.h>
#include <wchar.h>

/*
 * Should be defined in the Makefile to pass to the compiler from
 * the github build revision
 *
 */
#ifndef BUILD_VER 
#define BUILD_VER 000
#endif

#ifndef BUILD_DATE
#define BUILD_DATE " "
#endif

//char VERSION[] = BUILD_STR;
#define BYTE_RANGE 0
#define BYTE_DIGIT_3 1
#define BYTE_DIGIT_2 2
#define BYTE_DIGIT_1 3
#define BYTE_DIGIT_0 4
#define BYTE_FUNCTION 5
#define BYTE_STATUS 6
#define BYTE_OPTION_1 7
#define BYTE_OPTION_2 8
#define DATA_FRAME_SIZE 11 // 9 bytes followed by \r\n

#define FUNCTION_VOLTAGE 0b00111011
#define FUNCTION_CURRENT_UA 0b00111101
#define FUNCTION_CURRENT_MA 0b00111001
#define FUNCTION_CURRENT_A 0b00111111
#define FUNCTION_OHMS 0b00110011
#define FUNCTION_CONTINUITY 0b00110101
#define FUNCTION_DIODE 0b00110001
#define FUNCTION_FQ_RPM 0b00110010
#define FUNCTION_CAPACITANCE 0b00110110
#define FUNCTION_TEMPERATURE 0b00110100
#define FUNCTION_ADP0 0b00111110
#define FUNCTION_ADP1 0b00111100
#define FUNCTION_ADP2 0b00111000
#define FUNCTION_ADP3 0b00111010

#define STATUS_OL 0x01
#define STATUS_BATT 0x02
#define STATUS_SIGN 0x04
#define STATUS_JUDGE 0x08

#define OPTION1_VAHZ 0x01
#define OPTION1_PMIN 0x04
#define OPTION1_PMAX 0x08

#define OPTION2_APO 0x01
#define OPTION2_AUTO 0x02
#define OPTION2_AC 0x04
#define OPTION2_DC 0x08

#define WINDOWS_DPI_DEFAULT 72
#define FONT_NAME_SIZE 1024
#define SSIZE 1024

#define FONT_SIZE_MAX 256
#define FONT_SIZE_MIN 10
#define DEFAULT_FONT_SIZE 72
#define DEFAULT_FONT L"Andale"
#define DEFAULT_FONT_WEIGHT 600
#define DEFAULT_WINDOW_HEIGHT 9999
#define DEFAULT_WINDOW_WIDTH 9999
#define DEFAULT_COM_PORT 99

struct glb {
	int window_x, window_y;
	uint8_t debug;
	uint8_t comms_enabled;
	uint8_t quiet;
	uint8_t show_mode;
	uint16_t flags;
	uint8_t com_address;

	wchar_t font_name[FONT_NAME_SIZE];
	int font_size;
	int font_weight;

	COLORREF font_color, background_color;

	char serial_params[SSIZE];
};

/*
 * A whole bunch of globals, because I need
 * them accessible in the Windows handler
 *
 * So many of these I'd like to try get away from being
 * a global.
 *
 */
HFONT hFont, hFontBg;
HFONT holdFont;
HANDLE hComm;
DWORD dwRead;
BOOL fWaitingOnRead = FALSE;
OVERLAPPED osReader = { 0 };

HWND hstatic;
HBRUSH BBrush; // = CreateSolidBrush(RGB(0,0,0));
TEXTMETRIC fontmetrics, smallfontmetrics;

wchar_t line1[SSIZE];
wchar_t line2[SSIZE];
wchar_t line3[SSIZE];
struct glb *glbs;

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
int init(struct glb *g) {
	g->window_x = DEFAULT_WINDOW_WIDTH;
	g->window_y = DEFAULT_WINDOW_HEIGHT;
	g->debug = 0;
	g->comms_enabled = 1;
	g->quiet = 0;
	g->show_mode = 0;
	g->flags = 0;
	g->font_size = DEFAULT_FONT_SIZE;
	g->font_weight = DEFAULT_FONT_WEIGHT;
	g->com_address = DEFAULT_COM_PORT;

	StringCbPrintfW(g->font_name, FONT_NAME_SIZE, DEFAULT_FONT);
	g->font_color = RGB(16, 255, 16);
	g->background_color = RGB(0, 0, 0);

	g->serial_params[0] = '\0';

	return 0;
}

void show_help(void) {
	wprintf(L"BK-Precision 390A Multimeter serial data decoder\r\n"
"By Paul L Daniels / pldaniels@gmail.com\r\n"
"Build %d / %s\r\n"
"\r\n"
" [-p <comport#>] [-s <serial port config>] [-m] [-fn <fontname>] [-fc <#rrggbb>] [-fw <weight>] [-bc <#rrggbb>] [-wx <width>] [-wy <height>] [-d] [-q]\r\n"
"\r\n"
"\t-h: This help\r\n"
"\t-p <comport>: Set the com port for the meter, eg: -p 2\r\n"
"\t-s <[9600|4800|2400|1200]:[7|8][o|e|n][1|2]>, eg: -s 2400:7o1\r\n"
"\t-m: show multimeter mode (second line of text)\r\n"
"\t-z: Font size (default 72, max 256pt)\r\n"
"\t-fn <font name>: Font name (default 'Andale')\r\n"
"\t-fc <#rrggbb>: Font colour\r\n"
"\t-bc <#rrggbb>: Background colour\r\n"
"\t-fw <weight>: Font weight, typically 100-to-900 range\r\n"
"\t-wx <width>: Force Window width (normally calculated based on font size)\r\n"
"\t-wy <height>: Force Window height\r\n"
"\t-d: debug enabled\r\n"
"\t-q: quiet output\r\n"
"\t-v: show version\r\n"
"\r\n"
"\tDefaults: -s 2400:7o1 -z 72 -fc #10ff10 -bc #000000 -fw 600\r\n"
"\r\n"
"\texample: bk390a.exe -z 120 -p 4 -s 2400:7o1 -m -fc #10ff10 -bc #000000 -wx 480 -wy 60 -fw 600\r\n"
		, BUILD_VER
		, BUILD_DATE 
		);
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
int parse_parameters(struct glb *g) {
	LPWSTR *argv;
	int argc;
	int i;
	int fz = DEFAULT_FONT_SIZE;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (NULL == argv) {
		return 0;
	}

	/*if (argc ==1) {
		wprintf(L"Usage: %s", help);
		exit(1);
	}*/

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* parameter */
			switch (argv[i][1]) {
				case 'h':
					show_help();
					exit(1);
					break;

				case 'w':
					if (argv[i][2] == 'x') {
						i++;
						g->window_x = _wtoi(argv[i]);
					} else if (argv[i][2] == 'y') {
						i++;
						g->window_y = _wtoi(argv[i]);
					}
					break;

				case 'b':
					if (argv[i][2] == 'c') {
						int r, gg, b;

						i++;
						swscanf(argv[i], L"#%02x%02x%02x", &r, &gg, &b);
						g->background_color = RGB(r, gg, b);
					}
					break;

				case 'f':
					if (argv[i][2] == 'w') {
						i++;
						g->font_weight = _wtoi(argv[i]);

					} else if (argv[i][2] == 'c') {
						int r, gg, b;

						i++;
						swscanf(argv[i], L"#%02x%02x%02x", &r, &gg, &b);
						g->font_color = RGB(r, gg, b);

					} else if (argv[i][2] == 'n') {
						i++;
						StringCbPrintfW(g->font_name, FONT_NAME_SIZE, L"%s", argv[i]);
					}
					break;

				case 'z':
					i++;
					if (i < argc) {
						fz = _wtoi(argv[i]);
						if (fz < FONT_SIZE_MIN) {
							fz = FONT_SIZE_MIN;
						} else if (fz > FONT_SIZE_MAX) {
							fz = FONT_SIZE_MAX;
						}
						g->font_size = fz;
					}
					break;

				case 'p':
					i++;
					if (i < argc) {
						g->com_address = _wtoi(argv[i]);
					} else {
						wprintf(L"Insufficient parameters; -p <com port>\n");
						exit(1);
					}
					break;

				case 'c': g->comms_enabled = 0; break;

				case 'd': g->debug = 1; break;

				case 'q': g->quiet = 1; break;

				case 'm': g->show_mode = 1; break;

				case 'v':
							 wprintf(L"Build %d\r\n", BUILD_VER);
							 exit(0);
							 break;

				case 's':
							 i++;
							 if (i < argc) {
								 wcstombs(g->serial_params, argv[i], sizeof(g->serial_params));
							 } else {
								 wprintf(L"Insufficient parameters; -s <parameters> [eg 9600:8:o:1] = 9600, 8-bit, odd, 1-stop\n");
								 exit(1);
							 }
							 break;

				default: break;
			} // switch
		}
	}

	LocalFree(argv);

	return 0;
}

/*
 *   Declare Windows procedures
 */
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);

void enable_coms(struct glb *pg, wchar_t *com_port) {
   BOOL com_read_status;  // return status of various com port functions
   /*
    * Open the serial port
    */
   hComm = CreateFile(com_port,      // Name of port
         GENERIC_READ,  // Read Access
         0,             // No Sharing
         NULL,          // No Security
         OPEN_EXISTING, // Open existing port only
         0,             // Non overlapped I/O
         NULL);         // Null for comm devices

   /*
    * Check the outcome of the attempt to create the handle for the com port
    */
   if (hComm == INVALID_HANDLE_VALUE) {
      wprintf(L"Error while trying to open com port 'COM%d'\r\n", pg->com_address);
      exit(1);
   } else {
      if (!pg->quiet) wprintf(L"Port COM%d Opened\r\n", pg->com_address);
   }

   /*
    * Set serial port parameters
    */
   DCB dcbSerialParams = {0}; // Init DCB structure
   dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

   com_read_status = GetCommState(hComm, &dcbSerialParams); // Retrieve current settings
   if (com_read_status == FALSE) {
      wprintf(L"Error in getting GetCommState()\r\n");
      CloseHandle(hComm);
      exit(1);
   }

   dcbSerialParams.BaudRate = CBR_2400;
   dcbSerialParams.ByteSize = 7;
   dcbSerialParams.StopBits = ONESTOPBIT;
   dcbSerialParams.Parity = ODDPARITY;

   if (pg->serial_params[0] != '\0') {
      char *p = pg->serial_params;

      if (strncmp(p, "9600:", 5) == 0) dcbSerialParams.BaudRate = CBR_9600; // BaudRate = 9600
      else if (strncmp(p, "4800:", 5) == 0) dcbSerialParams.BaudRate = CBR_4800; // BaudRate = 4800
      else if (strncmp(p, "2400:", 5) == 0) dcbSerialParams.BaudRate = CBR_2400; // BaudRate = 2400
      else if (strncmp(p, "1200:", 5) == 0) dcbSerialParams.BaudRate = CBR_1200; // BaudRate = 1200
      else {
         wprintf(L"Invalid serial speed\r\n");
         CloseHandle(hComm);
         exit(1);
      }

      p = &(pg->serial_params[5]);
      if (*p == '7') dcbSerialParams.ByteSize = 7;
      else if (*p == '8') dcbSerialParams.ByteSize = 8;
      else {
         wprintf(L"Invalid serial byte size '%c'\r\n", *p);
         CloseHandle(hComm);
         exit(1);
      }

      p++;
      if (*p == 'o') dcbSerialParams.Parity = ODDPARITY;
      else if (*p == 'e') dcbSerialParams.Parity = EVENPARITY;
      else if (*p == 'n') dcbSerialParams.Parity = NOPARITY;
      else {
         wprintf(L"Invalid serial parity type '%c'\r\n", *p);
         CloseHandle(hComm);
         exit(1);
      }

      p++;
      if (*p == '1') dcbSerialParams.StopBits = ONESTOPBIT;
      else if (*p == '2') dcbSerialParams.StopBits = TWOSTOPBITS;
      else {
         wprintf(L"Invalid serial stop bits '%c'\r\n", *p);
         CloseHandle(hComm);
         exit(1);
      }
   }

   com_read_status = SetCommState(hComm, &dcbSerialParams);
   if (com_read_status == FALSE) {
      wprintf(L"Error setting com port configuration (2400/7/1/O etc)\r\n");
      CloseHandle(hComm);
      exit(1);
   } else {

      if (!pg->quiet) {
         wprintf(L"\tBaudrate = %ld\r\n", dcbSerialParams.BaudRate);
         wprintf(L"\tByteSize = %ld\r\n", dcbSerialParams.ByteSize);
         wprintf(L"\tStopBits = %d\r\n", dcbSerialParams.StopBits);
         wprintf(L"\tParity   = %d\r\n", dcbSerialParams.Parity);
      }
   }

   COMMTIMEOUTS timeouts = {0};
   timeouts.ReadIntervalTimeout = 50;
   timeouts.ReadTotalTimeoutConstant = 1000; // ReadFile should wait up to one second
   timeouts.ReadTotalTimeoutMultiplier = 10;
   timeouts.WriteTotalTimeoutConstant = 50;
   timeouts.WriteTotalTimeoutMultiplier = 10;
   if (SetCommTimeouts(hComm, &timeouts) == FALSE) {
      wprintf(L"\tError in setting time-outs\r\n");
      CloseHandle(hComm);
      exit(1);

   } else {
      if (!pg->quiet) { wprintf(L"\tSetting time-outs successful\r\n"); }
   }

   com_read_status = SetCommMask(hComm, EV_RXCHAR | EV_ERR); // Configure Windows to Monitor the serial device for Character Reception and Errors
   if (com_read_status == FALSE) {
      wprintf(L"\tError in setting CommMask\r\n");
      CloseHandle(hComm);
      exit(1);

   } else {
      if (!pg->quiet) { wprintf(L"\tCommMask successful\r\n"); }
   }
}

// Based on code from: https://bytes.com/topic/net/answers/666485-trying-retrieve-list-active-serial-com-ports-c
bool auto_detect_port(struct glb *pg) {
   TCHAR szDevices[65535];
   unsigned long dwChars = QueryDosDevice(NULL, szDevices, 65535);
   TCHAR *ptr = szDevices;
   
   wchar_t com_port[SSIZE]; // com port path / ie, \\.COM4
   char temp_char;        // Temporary character
   uint8_t d[SSIZE];      // Serial data packet
   DWORD bytes_read;      // Number of bytes read by ReadFile()
   int end_of_frame_received = 0;
   BOOL com_read_status;
   DWORD dwEventMask;     // Event mask to trigger
   int i;
   int attempts_remaining = 2;
   

   while (dwChars) {
      int port;
      
      if (swscanf(ptr, L"COM%d", &port) == 1) { // if it finds the format COM#
         // found a com port!
         // it will never be COM1, which is reserved
         if (port != 1) {
            // try to communicate and listen for appropriately-formatted data packet
            pg->com_address = port;
            if (pg->debug) { wprintf(L"Port detected: COM%d\r\n",port); }
            snwprintf(com_port, sizeof(com_port), L"\\\\.\\COM%d", port);
            if (pg->comms_enabled) {
               enable_coms(pg, com_port); // establish serial communication parameters
            }

            if (pg->debug) { wprintf(L"DATA START: "); }
            i = 0;
            do {
               com_read_status = ReadFile(hComm, &temp_char, sizeof(temp_char), &bytes_read, NULL);
               d[i] = temp_char;
               if (pg->debug) { wprintf(L"%02x ", d[i]); }
               i++;
               
               if (temp_char == '\n') {
                  end_of_frame_received = 1;
                  break;
               }
            } while ((bytes_read > 0) && (i < sizeof(d)));
            
            if (pg->debug) { wprintf(L":END\r\n"); }
            
            // see if data is valid with 2 checks
            // #1 - length check
            if(i == DATA_FRAME_SIZE) {
               if (pg->debug) {
                  wprintf(L"LENGTH CHECK: SUCCESS\r\n");
               }
               // #2 - check to see if the data fits the protocol
               switch (d[BYTE_FUNCTION]) {
                  case FUNCTION_VOLTAGE:
                  case FUNCTION_CURRENT_UA:
                  case FUNCTION_CURRENT_MA:
                  case FUNCTION_CURRENT_A:
                  case FUNCTION_OHMS:
                  case FUNCTION_CONTINUITY:
                  case FUNCTION_DIODE:
                  case FUNCTION_FQ_RPM:
                  case FUNCTION_CAPACITANCE:
                  case FUNCTION_TEMPERATURE:
                     if (pg->debug) {
                        wprintf(L"DATA FORMAT CHECK: SUCCESS\r\n");
                     }
                     return true; // passed our check
               }
               
               if (pg->debug) {
                  wprintf(L"DATA FORMAT CHECK: FAIL\r\n");
               } // if debug
               attempts_remaining--;
               if(attempts_remaining > 0) {
                  continue; // try again from the top. same port.
               }

            } else {
               if (pg->debug) {
                  wprintf(L"LENGTH CHECK: FAIL\r\n");
               } // if debug
               attempts_remaining--;
               if(attempts_remaining > 0) {
                  continue; // try again from the top. same port.
               }
            }
         }
      }
      
      // if we made it this far, this particular COM did not work out
      if(hComm) { // prevent small memory leak!
         CloseHandle(hComm);
      }
      
      // advance the string pointers
      TCHAR *temp_ptr = wcschr(ptr, '\0');
      dwChars -= (DWORD)((temp_ptr - ptr) / sizeof(TCHAR) + 1);
      ptr = temp_ptr + 1;
      attempts_remaining = 2; // reset remaining attempts
   }
   
   return false; // if we made it to the end of the function, auto-detection failed
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
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
	wchar_t linetmp[SSIZE]; // temporary string for building main line of text
	wchar_t prefix[SSIZE]; // Units prefix u, m, k, M etc
	wchar_t units[SSIZE];  // Measurement units F, V, A, R
	wchar_t mmmode[SSIZE]; // Multimeter mode, Resistance/diode/cap etc

	uint8_t d[SSIZE];      // Serial data packet
	uint8_t dt[SSIZE];      // Serial data packet
	int dt_loaded = 0;	// set when we have our first valid data
	uint8_t dps = 0;     // Number of decimal places
	struct glb g;        // Global structure for passing variables around
	int i = 0;           // Generic counter
	MSG msg;
	WNDCLASSW wc = {0};
	wchar_t com_port[SSIZE]; // com port path / ie, \\.COM4
	BOOL com_read_status;  // return status of various com port functions
	DWORD dwEventMask;     // Event mask to trigger
	char temp_char;        // Temporary character
	DWORD bytes_read;      // Number of bytes read by ReadFile()
	HDC dc;

	glbs = &g;

	/*
	 * Initialise the global structure
	 */
	init(&g);

	/*
	 * Parse our command line parameters
	 */
	parse_parameters(&g);

	/*
	 *
	 * Now do all the Windows GDI stuff
	 *
	 */
	BBrush = CreateSolidBrush(g.background_color);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpszClassName = L"BK-390A Meter";
	wc.hInstance = hInstance;
	wc.hbrBackground = BBrush;
	wc.lpfnWndProc = WindowProcedure;
	wc.hCursor = LoadCursor(0, IDC_ARROW);

	NONCLIENTMETRICS metrics;
	metrics.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &metrics, 0);

	RegisterClassW(&wc);
   
   hstatic = CreateWindowW(wc.lpszClassName, L"BK-390A Meter", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, g.window_x, g.window_y, NULL, NULL, hInstance, NULL);

	/*
	 *
	 * Create fonts and get their metrics/sizes
	 *
	 */
	dc = GetDC(hstatic);

	hFont = CreateFont(-(g.font_size), 0, 0, 0, g.font_weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH,
			g.font_name);
	holdFont = (HFONT)SelectObject(dc, hFont);
	GetTextMetrics(dc, &fontmetrics);

	hFontBg = CreateFont(-(g.font_size / 4), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH,
			g.font_name);
	holdFont = (HFONT)SelectObject(dc, hFontBg);
	GetTextMetrics(dc, &smallfontmetrics);

	/*
	 * If the user hasn't explicitly set a window size
	 * then we will try to determine a size based on our
	 * font metrics
	 */
	if (g.window_x == DEFAULT_WINDOW_WIDTH) g.window_x = fontmetrics.tmAveCharWidth * 9;
	if (g.window_y == DEFAULT_WINDOW_HEIGHT) g.window_y = ((((fontmetrics.tmAscent) + smallfontmetrics.tmHeight + metrics.iCaptionHeight) * GetDeviceCaps(dc, LOGPIXELSY)) / WINDOWS_DPI_DEFAULT);
   
   SetWindowPos(hstatic,HWND_TOP,50,50,g.window_x,g.window_y,(UINT)0); // resize accordingly and give window focus

   /*
	 * Handle the COM Port
	 */
#if FAKE_SERIAL == 1
	if (0) {
#else 
	if (g.com_address == DEFAULT_COM_PORT) { // no port was specified, so attempt an auto-detect
#endif
      if(g.debug) {
         wprintf(L"Now attempting an auto-detect....\r\n");
      }
		if(!auto_detect_port(&g))  { // returning false means auto-detect failed
         wprintf(L"Failed to automatically detect COM port. Perhaps try using -p?\r\n");
         exit(1);
      }
      if (g.debug) { wprintf(L"COM%d automatically detected.\r\n",g.com_address); }
   } else { // the port was specified, so let's try enabling it
      if (g.comms_enabled) {
         snwprintf(com_port, sizeof(com_port), L"\\\\.\\COM%d", g.com_address);
#if FAKE_SERIAL == 0
         enable_coms(&g, com_port); // establish serial communication parameters
#endif
      }
   }

	/*
	 * Keep reading, interpreting and converting data until someone
	 * presses ctrl-c or there's an error
	 */
	while (msg.message != WM_QUIT) {
		char *p, *q;
		double v = 0.0;
		int end_of_frame_received = 0;

		linetmp[0] = '\0';

		/*
		 *
		 * Let Windows handle itself first
		 *
		 */

		// while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
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
#if FAKE_SERIAL == 1
		if (0) {
#else
		com_read_status = WaitCommEvent(hComm, &dwEventMask, NULL); // Wait for the character to be received
		if (com_read_status == FALSE) {
			StringCbPrintf(linetmp, sizeof(linetmp), L"N/C");
			StringCbPrintf(mmmode, sizeof(mmmode), L"Check RS232");
#endif

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

#if FAKE_SERIAL == 1
            d[BYTE_RANGE] = 0x31; // 000.0 format in resistance mode
            d[BYTE_DIGIT_3] = 0x34; // 4
            d[BYTE_DIGIT_2] = 0x33; // 3
            d[BYTE_DIGIT_1] = 0x32; // 2
            d[BYTE_DIGIT_0] = 0x31; // 1
            d[BYTE_FUNCTION] = 0x33; // resistance mode
            d[BYTE_STATUS] = 0x30 | STATUS_SIGN; // no judge, no - sign, batt not low, not overloading
            d[BYTE_OPTION_1] = 0x30;
            d[BYTE_OPTION_2] = 0x30;
				i = DATA_FRAME_SIZE;
				usleep(500000);
#else
			if (g.debug) { wprintf(L"DATA START: "); }
			end_of_frame_received = 0;
			i = 0;
			do {
				com_read_status = ReadFile(hComm, &temp_char, sizeof(temp_char), &bytes_read, NULL);
				if (bytes_read) {
					d[i] = temp_char;
					if (g.debug) { wprintf(L"%02x ", d[i]); }

			      i++;

					if (temp_char == '\n') {
						end_of_frame_received = 1;
						break;
					}
				}
			} while ((bytes_read > 0) && (i < sizeof(d)));

			if (g.debug) { wprintf(L":END [%d bytes]\r\n", i); }
#endif

			/*
			 * Validate the received data
			 *
			 */
			if (i != DATA_FRAME_SIZE) {
				if (g.debug) { wprintf(L"Invalid number of bytes, expected %d, received %d, loading previous frame\r\n", DATA_FRAME_SIZE, i); }
				if (dt_loaded) memcpy(d, dt, sizeof(d));
			} else {
				memcpy(dt, d, sizeof(d)); // make a copy.
				dt_loaded = 1;
			}

			/*
			 * Initialise the strings used for units, prefix and mode
			 * so we don't end up with uncleared prefixes etc
			 * ( see https://www.youtube.com/watch?v=5HUyEykicEQ )
			 *
			 * Prefix string initialised to single space, prevents 
			 * annoying string width jump (on monospace, can't stop
			 * it with variable width strings unless we draw the 
			 * prefix+units separately in a fixed location
			 *
			 */
			StringCbPrintf(prefix, sizeof(prefix), L" ");
			units[0] = '\0';
			mmmode[0] = '\0';

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
					StringCbPrintf(units, sizeof(units), L"V");
					StringCbPrintf(mmmode, sizeof(mmmode), L"Volts");

					switch (d[BYTE_RANGE] & 0x0F) {
						case 0:
							dps = 1;
							StringCbPrintf(prefix, sizeof(prefix), L"m");
							break;
						case 1: dps = 3; break;
						case 2: dps = 2; break;
						case 3: dps = 1; break;
						case 4: dps = 0; break;
					}      // test the range byte for voltages
					break; // FUNCTION_VOLTAGE

				case FUNCTION_CURRENT_UA:
					StringCbPrintf(units, sizeof(units), L"A");
					StringCbPrintf(prefix, sizeof(prefix), L"\u00B5");
					StringCbPrintf(mmmode, sizeof(mmmode), L"Amps");

					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 1; break;
						case 1: dps = 0; break;
					}
					break; // FUNCTION_CURRENT_UA

				case FUNCTION_CURRENT_MA:
					StringCbPrintf(units, sizeof(units), L"A");
					StringCbPrintf(prefix, sizeof(prefix), L"m");
					StringCbPrintf(mmmode, sizeof(mmmode), L"Amps");

					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 2; break;
						case 1: dps = 1; break;
					}
					break; // FUNCTION_CURRENT_MA

				case FUNCTION_CURRENT_A:
					StringCbPrintf(units, sizeof(units), L"A");
					StringCbPrintf(mmmode, sizeof(mmmode), L"Amps");
					dps = 2;
					break; // FUNCTION_CURRENT_A

				case FUNCTION_OHMS:
					StringCbPrintf(mmmode, sizeof(mmmode), L"Resistance");
					StringCbPrintf(units, sizeof(units), L"\u2126");

					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 1; break;
						case 1: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
						case 2: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
						case 3: dps = 1; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
						case 4: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
						case 5: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
					}
					break; // FUNCTION_OHMS

				case FUNCTION_CONTINUITY:
					StringCbPrintf(mmmode, sizeof(mmmode), L"Continuity");
					StringCbPrintf(units, sizeof(units), L"\u2126");
					dps = 1;
					break; // FUNCTION_CONTINUITY

				case FUNCTION_DIODE:
					StringCbPrintf(mmmode, sizeof(mmmode), L"DIODE");
					StringCbPrintf(units, sizeof(units), L"V");
					dps = 3;
					break; // FUNCTION_DIODE

				case FUNCTION_FQ_RPM:
					if (!(d[BYTE_STATUS] & STATUS_JUDGE)) {
						StringCbPrintf(mmmode, sizeof(mmmode), L"Frequency");
						StringCbPrintf(units, sizeof(units), L"Hz");
						switch (d[BYTE_RANGE] & 0x0F) {
							case 0: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
							case 1: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
							case 2: dps = 1; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
							case 3: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
							case 4: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
							case 5: dps = 1; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
						} // switch

					} else {
						StringCbPrintf(mmmode, sizeof(mmmode), L"RPM");
						StringCbPrintf(units, sizeof(units), L"rpm");
						switch (d[BYTE_RANGE] & 0x0F) {
							case 0: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
							case 1: dps = 1; StringCbPrintf(prefix, sizeof(prefix), L"k"); break;
							case 2: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
							case 3: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
							case 4: dps = 1; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
							case 5: dps = 0; StringCbPrintf(prefix, sizeof(prefix), L"M"); break;
						} // switch
					}
					break; // FUNCTION_FQ_RPM

				case FUNCTION_CAPACITANCE:
					StringCbPrintf(mmmode, sizeof(mmmode), L"Capacitance");
					StringCbPrintf(units, sizeof(units), L"F");
					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"n"); break;
						case 1: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"n"); break;
						case 2: dps = 1; StringCbPrintf(prefix, sizeof(prefix), L"n"); break;
						case 3: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"\u00B5"); break;
						case 4: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"\u00B5"); break;
						case 5: dps = 1; StringCbPrintf(prefix, sizeof(prefix), L"\u00B5"); break;
						case 6: dps = 3; StringCbPrintf(prefix, sizeof(prefix), L"m"); break;
						case 7: dps = 2; StringCbPrintf(prefix, sizeof(prefix), L"m"); break;
					}
					break; // FUNCTION_CAPACITANCE

				case FUNCTION_TEMPERATURE:
					StringCbPrintf(mmmode, sizeof(mmmode), L"Temperature");
					if (d[BYTE_STATUS] & STATUS_JUDGE) {
						StringCbPrintf(units, sizeof(units), L"\u00B0C");
					} else {
						StringCbPrintf(units, sizeof(units), L"\u00B0F");
					}
					dps = 0;
					break; // FUNCTION_TEMPERATURE
			}

			/*
			 * Decode the digit data in to human-readable
			 *
			 * bytes 1..4 are ASCII char codes for 0000-9999
			 *
			 */
			v = ((d[1] & 0x0F) * 1000) 
				+ ((d[2] & 0x0F) * 100) 
				+ ((d[3] & 0x0F) * 10) 
				+ ((d[4] & 0x0F) * 1);

			/*
			 * Sign of output (+/-)
			 */
			if (d[BYTE_STATUS] & STATUS_SIGN) {
				v = -v;
			}

			/*
			 * If we're not showing the meter mode, then just
			 * zero the string we generated previously
			 */
			if (g.show_mode == 0) {
				mmmode[0] = 0;
			}

			/** range checks **/
			if ((d[BYTE_STATUS] & STATUS_OL) == 1) {
				StringCbPrintf(linetmp, sizeof(linetmp), L"O.L.");

			} else {
				if (dps < 0) dps = 0;
				if (dps > 3) dps = 3;

				switch (dps) {
					case 0: StringCbPrintf(linetmp, sizeof(linetmp), L"% 05.0f%s%s", v, prefix, units); break;
					case 1: StringCbPrintf(linetmp, sizeof(linetmp), L"% 06.1f%s%s", v / 10, prefix, units); break;
					case 2: StringCbPrintf(linetmp, sizeof(linetmp), L"% 06.2f%s%s", v / 100, prefix, units); break;
					case 3: StringCbPrintf(linetmp, sizeof(linetmp), L"% 06.3f%s%s", v / 1000, prefix, units); break;
				}
			}
		} // if com-read status == TRUE

		StringCbPrintf(line1, sizeof(line1), L"%-40s", linetmp);
		StringCbPrintf(line2, sizeof(line2), L"%-40s", mmmode);
		StringCbPrintf(line3, sizeof(line3), L"V.%03d", BUILD_VER);
		InvalidateRect(hstatic, NULL, FALSE);
      UpdateWindow(hstatic);

	} // Windows message loop

	CloseHandle(hComm); // Closing the Serial Port

	return (int)msg.wParam;
}


/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) /* handle the messages */
	{
		case WM_CREATE: 
			break;

		case WM_PAINT:
			HDC hdc;
			PAINTSTRUCT ps;
			RECT wrect;
			GetWindowRect( hwnd, &wrect ); 

			hdc = BeginPaint(hwnd, &ps);
			SetBkColor(hdc, glbs->background_color);
			SetTextColor(hdc, glbs->font_color);

			holdFont = (HFONT)SelectObject(hdc, hFont);
			TextOutW(hdc, 0, 0, line1, wcslen(line1));

			holdFont = (HFONT)SelectObject(hdc, hFontBg);
			TextOutW(hdc, smallfontmetrics.tmAveCharWidth, fontmetrics.tmAscent * 1.1, line2, wcslen(line2));

			holdFont = (HFONT)SelectObject(hdc, hFontBg);
			TextOutW(hdc,  (wrect.right -wrect.left) -(smallfontmetrics.tmAveCharWidth *9), fontmetrics.tmAscent * 1.1, line3, wcslen(line3));

			EndPaint(hwnd, &ps);
			break;

		case WM_COMMAND: break;

		case WM_DESTROY:
			DeleteObject(hFont);
			PostQuitMessage(0); /* send a WM_QUIT to the message queue */
			break;
		default: /* for messages that we don't deal with */ return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}
