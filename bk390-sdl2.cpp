/*
 * BK390A Multimeter for Linux
 *
 * V0.1 - October 4, 2018
 * 
 *
 * Written by Paul L Daniels (pldaniels@gmail.com)
 *
 */

#include <SDL.h>
#include <SDL_ttf.h>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <X11/Xlib.h>

#define FL __FILE__,__LINE__

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
#define DEFAULT_FONT "Andale"
#define DEFAULT_FONT_WEIGHT 600
#define DEFAULT_WINDOW_HEIGHT 9999
#define DEFAULT_WINDOW_WIDTH 9999
#define DEFAULT_COM_PORT 99




#define DATA_FRAME_SIZE 11
#define ee ""
#define uu "\u00B5"
#define kk "k"
#define MM "M"
#define mm "m"
#define nn "n"
#define pp "p"
#define dd "\u00B0"
#define oo "\u03A9"

struct serial_params_s {
	char *device;
	int fd, n;
	int cnt, size, s_cnt;
	struct termios oldtp, newtp;
};

struct meter_param {
	char mode[20];
	char units[20];
	int dividers[8];
	char prefix[8][2];
};


/*
 * Global structure, it's a little naughty but
 * it's better at least to pass this around via
 * the function calls that have it assumed and
 * floating in the heap
 *
 */

struct glb {
	uint8_t debug;
	uint8_t quiet;
	uint8_t show_mode;
	uint16_t flags;
	char *com_address;
	char *output_file;

	char *serial_parameters_string;
	struct serial_params_s serial_params;

	int font_size;
	int window_width, window_height;
	int wx_forced, wy_forced;
	SDL_Color font_color, background_color;

};

struct glb *glbs;


bool fileExists(const char *filename) {
	struct stat buf;
	return (stat(filename, &buf) == 0);
}



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
	g->debug = 0;
	g->quiet = 0;
	g->flags = 0;
	g->com_address = NULL;
	g->output_file = NULL;

	g->font_size = 60;
	g->window_width = 400;
	g->window_height = 100;
	g->wx_forced = 0;
	g->wy_forced = 0;

	g->font_color =  { 10, 255, 10 };
	g->background_color = { 0, 0, 0 };

	return 0;
}

void show_help(void) {
	fprintf(stdout,"BK390A Multimeter OSD\r\n"
			"By Paul L Daniels / pldaniels@gmail.com\r\n"
			"Build %d / %s\r\n"
			"\r\n"
			" [-p <comport#>] [-s <serial port config>] [-d] [-q]\r\n"
			"\r\n"
			"\t-h: This help\r\n"
			"\t-p <comport>: Set the com port for the meter, eg: -p /dev/ttyUSB0\r\n"
			"\t-s <[9600|4800|2400|1200]:[7|8][o|e|n][1|2]>, eg: -s 2400:8n1\r\n"
			"\t-o <output file> ( used by FlexBV to read the data )\r\n"
			"\t-d: debug enabled\r\n"
			"\t-q: quiet output\r\n"
			"\t-v: show version\r\n"
			"\t-z <font size in pt>\r\n"
			"\t-fc <foreground colour, f0f0ff>\r\n"
			"\t-bc <background colour, 101010>\r\n"
			"\r\n"
			"\r\n"
			"\texample: bside-adm20 -p /dev/ttyUSB0\r\n"
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
int parse_parameters(struct glb *g, int argc, char **argv ) {
	int i;

	if (argc == 1) {
		show_help();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* parameter */
			switch (argv[i][1]) {

				case 'h':
					show_help();
					exit(1);
					break;

				case 'z':
					i++;
					if (i < argc) {
						g->font_size = atoi(argv[i]);
					} else {
						fprintf(stdout,"Insufficient parameters; -z <font size pts>\n");
						exit(1);
					}
					break;

				case 'p':
					/*
					 * com port can be multiple things in linux
					 * such as /dev/ttySx or /dev/ttyUSBxx
					 */
					i++;
					if (i < argc) {
						g->serial_params.device = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -p <com port>\n");
						exit(1);
					}
					break;

				case 'o':
					/* 
					 * output file where this program will put the text
					 * line containing the information FlexBV will want 
					 *
					 */
					i++;
					if (i < argc) {
						g->output_file = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -o <output file>\n");
						exit(1);
					}
					break;

				case 'd': g->debug = 1; break;

				case 'q': g->quiet = 1; break;

				case 'v':
							 fprintf(stdout,"Build %d\r\n", BUILD_VER);
							 exit(0);
							 break;

							 /*
				case 'f':
							 if (argv[i][2] == 'c') {
								 i++;
								 sscanf(argv[i], "%02x%02x%02x"
										 , &g->font_color.r
										 , &g->font_color.g
										 , &g->font_color.b
										 );

							 }
							 break;

				case 'b':
							 if (argv[i][2] == 'c') {
								 i++;
								 sscanf(argv[i], "%02x%02x%02x"
										 , &g->background_color.r
										 , &g->background_color.g
										 , &g->background_color.b
										 );
							 }
							 break;
							 */

				case 'w':
							 if (argv[i][2] == 'x') {
								 i++;
								 g->wx_forced = atoi(argv[i]);
							 } else if (argv[i][2] == 'y') {
								 i++;
								 g->wy_forced = atoi(argv[i]);
							 }
							 break;

				case 's':
							 i++;
							 g->serial_parameters_string = argv[i];
							 // Not needed, we hard code at 2400-8n1 because
							 // that's what these meters should be doing.  
							 //
							 // If not, then feel free to adjust the parameter or
							 // populate this section.
							 break;

				default: break;
			} // switch
		}
	}

	return 0;
}



/*
 *
 * Default parameters are 2400:7none1, given that the multimeter
 * is shipped like this and cannot be changed then we shouldn't
 * have to worry about needing to make changes, but we'll probably
 * add that for future changes.
 *
 */
/*
void open_port(struct serial_params_s *s) {
	int r; 

	s->fd = open( s->device, O_RDWR | O_NOCTTY |O_NDELAY );
	if (s->fd <0) {
		perror( s->device );
	}

	fcntl(s->fd,F_SETFL,0);
	tcgetattr(s->fd,&(s->oldtp)); // save current serial port settings 
	tcgetattr(s->fd,&(s->newtp)); // save current serial port settings in to what will be our new settings
	cfmakeraw(&(s->newtp));
	s->newtp.c_cflag = B2400 | CS7 | PARODD | CREAD | CRTSCTS ; // Adjust the settings to suit our BK390A / 2400-7o1

	cfsetospeed(&s->newtp, B2400);
	cfsetispeed(&s->newtp, B2400);
	r = tcsetattr(s->fd, TCSANOW, &(s->newtp));
	if (r) {
		fprintf(stderr,"%s:%d: Error setting terminal (%s)\n", FL, strerror(errno));
		exit(1);
	}
}
*/


/*
 * Default parameters are 2400:8n1, given that the multimeter
 * is shipped like this and cannot be changed then we shouldn't
 * have to worry about needing to make changes, but we'll probably
 * add that for future changes.
 *
 */
void open_port( struct glb *g ) {
#ifdef __linux__
	struct serial_params_s *s = &(g->serial_params);
	char *p = g->serial_parameters_string;
	char default_params[] = "2400:7o1";
	int r; 

	if (!p) p = default_params;

	fprintf(stdout,"Attempting to open '%s'\n", s->device);
	s->fd = open( s->device, O_RDWR | O_NOCTTY | O_NDELAY );
	if (s->fd <0) {
		perror( s->device );
	}

	fcntl(s->fd,F_SETFL,0);
	tcgetattr(s->fd,&(s->oldtp)); // save current serial port settings 
	tcgetattr(s->fd,&(s->newtp)); // save current serial port settings in to what will be our new settings
	cfmakeraw(&(s->newtp));

	s->newtp.c_cflag = CLOCAL | CREAD ; 

	if (strncmp(p, "115200:", 7) == 0) s->newtp.c_cflag |= B115200; 
	else if (strncmp(p, "57600:", 6) == 0) s->newtp.c_cflag |= B57600;
	else if (strncmp(p, "38400:", 6) == 0) s->newtp.c_cflag |= B38400;
	else if (strncmp(p, "19200:", 6) == 0) s->newtp.c_cflag |= B19200;
	else if (strncmp(p, "9600:", 5) == 0) s->newtp.c_cflag |= B9600;
	else if (strncmp(p, "4800:", 5) == 0) s->newtp.c_cflag |= B4800;
	else if (strncmp(p, "2400:", 5) == 0) s->newtp.c_cflag |= B2400; //
	else {
		fprintf(stdout,"Invalid serial speed\r\n");
		exit(1);
	}


//	s->newtp.c_cc[VMIN] = 0;
//	s->newtp.c_cc[VTIME] = g->serial_timeout *10; // VTIME is 1/10th's of second

	p = strchr(p,':');
	if (p) {
		p++;
		switch (*p) {
			case '8': 
				s->newtp.c_cflag |= CS8;
				break;
			case '7': 
				s->newtp.c_cflag |= CS7;
				break;
			default: 
						 fprintf(stdout, "Meter only accepts 7 or 8 bit mode\n");
		}

		p++;
		switch (*p) {
			case 'o': 
				s->newtp.c_cflag |= (PARENB|PARODD);
				break;
			case 'n': 
				s->newtp.c_cflag &= ~(PARODD|PARENB);
				break;
			case 'e': 
				s->newtp.c_cflag |= PARENB;
				break;
			default: 
				fprintf(stdout, "Parity mode is [n]one, [o]dd, or [e]ven\n");
		}

		p++;
		switch (*p) {
			case '1': 
				s->newtp.c_cflag &= ~CSTOPB;
				break;
			case '2': 
				s->newtp.c_cflag |= CSTOPB;
				break;
			default: 
				fprintf(stdout, "Stop bits are 1, or 2 only\n");
		}

	}

	s->newtp.c_iflag &= ~(IXON | IXOFF | IXANY );

	r = tcsetattr(s->fd, TCSANOW, &(s->newtp));
	if (r) {
		fprintf(stderr,"%s:%d: Error setting terminal (%s)\n", FL, strerror(errno));
		exit(1);
	}

	fprintf(stdout,"Serial port opened, FD[%d]\n", s->fd);
#endif
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
int main ( int argc, char **argv ) {

	SDL_Event event;
	SDL_Surface *surface;
	SDL_Texture *texture;

	char linetmp[SSIZE]; // temporary string for building main line of text
	char prefix[SSIZE]; // Units prefix u, m, k, M etc
	char units[SSIZE];  // Measurement units F, V, A, R
	char mmmode[SSIZE]; // Multimeter mode, Resistance/diode/cap etc

	uint8_t d[SSIZE];
	uint8_t dt[SSIZE];      // Serial data packet
	int dt_loaded = 0;	// set when we have our first valid data
	uint8_t dps = 0;     // Number of decimal places
	struct glb g;        // Global structure for passing variables around
	int i = 0;           // Generic counter
	char temp_char;        // Temporary character
	char tfn[4096];
	bool quit = false;

	glbs = &g;

	/*
	 * Initialise the global structure
	 */
	init(&g);

	/*
	 * Parse our command line parameters
	 */
	parse_parameters(&g, argc, argv);

	/* 
	 * check paramters
	 *
	 */
	if (g.font_size < 10) g.font_size = 10;
	if (g.font_size > 240) g.font_size = 240;

	if (g.output_file) snprintf(tfn,sizeof(tfn),"%s.tmp",g.output_file);

	/*
	 * Handle the COM Port
	 */
	open_port(&g);

	/*
	 * Setup SDL2 and fonts
	 *
	 */

	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();
	TTF_Font *font = TTF_OpenFont("RobotoMono-Regular.ttf", g.font_size);
	if (!font) {
		fprintf(stderr,"Error trying to open font (RobotoMono-Regular.ttf)  :(\n");
		exit(1);
	}

	/*
	 * Get the required window size.
	 *
	 * Parameters passed can override the font self-detect sizing
	 *
	 */
	TTF_SizeText(font, "-12.34mV  ", &g.window_width, &g.window_height);
	if (g.wx_forced) g.window_width = g.wx_forced;
	if (g.wy_forced) g.window_height = g.wy_forced;

	SDL_Window *window = SDL_CreateWindow("BK390A Multimeter OSD", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g.window_width, g.window_height, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

	/* Select the color for drawing. It is set to red here. */
	SDL_SetRenderDrawColor(renderer, g.background_color.r, g.background_color.g, g.background_color.b, 255 );

	/* Clear the entire screen to our selected color. */
	SDL_RenderClear(renderer);

	SDL_Color color = { 55, 255, 55 };


	/*
	 *
	 * Parent will terminate us... else we'll become a zombie
	 * and hope that the almighty PID 1 will reap us
	 *
	 */
	while (!quit) {
		char line1[1024];
		char *p, *q;
		double v = 0.0;
		int end_of_frame_received = 0;
		int comms_error = 0;
		ssize_t bytes_read = 0;

		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
				case SDL_QUIT:
					quit = true;
					break;
			}
		} // while SDL poll

		linetmp[0] = '\0';

		/*
		 * Time to start receiving the serial block data
		 *
		 * We initially "stage" here waiting for there to
		 * be something happening on the com port.  Soon as
		 * something happens, then we move forward to trying
		 * to read the data.
		 *
		 */

		if (g.debug) { fprintf(stdout,"DATA START: "); }

		end_of_frame_received = 0;
		comms_error = 0;
		i = 0;

		do {
			uint8_t temp_char;
			bytes_read = read(g.serial_params.fd, &temp_char, 1);
			if (bytes_read == -1) {
				comms_error = 1;
				i = 0;
				break;
			}

			if (bytes_read > 0) {
				d[i] = temp_char;
				if (g.debug) { fprintf(stdout,"%02x ", d[i]); fflush(stdout); }


				i++;

				if ( temp_char == '\n' ) {
					end_of_frame_received = 1;
					break;
				}
			}
		} while ((bytes_read > 0) && (i < sizeof(d)) && (!end_of_frame_received));


		if (g.debug) { fprintf(stdout,":END [%d bytes]\r\n", i); }

		/*
		 * Validate the received data
		 *
		 */
		if (i != DATA_FRAME_SIZE) {
			if (g.debug) { fprintf(stdout,"Invalid number of bytes, expected %d, received %d, loading previous frame\r\n", DATA_FRAME_SIZE, i); }
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
		snprintf(prefix, sizeof(prefix), " ");
		units[0] = '\0';
		mmmode[0] = '\0';

		/*
		 * Decode our data.
		 *
		 * While the data sheet gives a very nice matrix for the RANGE and FUNCTION values
		 * it's probably more human-readable to break it down in to longer code on a per
		 * function selection.
		 *
		 * linetmp : contains the value we want show
		 * mmode   : contains the meter mode (resistance, volts, amps etc)
		 *  "\u00B0C" = 'C
		 *  "\u00B0F" = 'F
		 *  "\u2126"  = ohms char
		 *  "\u00B5"  = mu char (micro)
		 *
		 */


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
		snprintf(prefix, sizeof(prefix), " ");
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
				snprintf(units, sizeof(units), "V");
				snprintf(mmmode, sizeof(mmmode), "Volts");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0:
						dps = 1;
						snprintf(prefix, sizeof(prefix), "m");
						break;
					case 1: dps = 3; break;
					case 2: dps = 2; break;
					case 3: dps = 1; break;
					case 4: dps = 0; break;
				}      // test the range byte for voltages
				break; // FUNCTION_VOLTAGE

			case FUNCTION_CURRENT_UA:
				snprintf(units, sizeof(units), "A");
				snprintf(prefix, sizeof(prefix), "\u00B5");
				snprintf(mmmode, sizeof(mmmode), "Amps");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 1; break;
					case 1: dps = 0; break;
				}
				break; // FUNCTION_CURRENT_UA

			case FUNCTION_CURRENT_MA:
				snprintf(units, sizeof(units), "A");
				snprintf(prefix, sizeof(prefix), "m");
				snprintf(mmmode, sizeof(mmmode), "Amps");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 2; break;
					case 1: dps = 1; break;
				}
				break; // FUNCTION_CURRENT_MA

			case FUNCTION_CURRENT_A:
				snprintf(units, sizeof(units), "A");
				snprintf(mmmode, sizeof(mmmode), "Amps");
				dps = 2;
				break; // FUNCTION_CURRENT_A

			case FUNCTION_OHMS:
				snprintf(mmmode, sizeof(mmmode), "Resistance");
				snprintf(units, sizeof(units), "\u2126");

				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 1; break;
					case 1: dps = 3; snprintf(prefix, sizeof(prefix), "k"); break;
					case 2: dps = 2; snprintf(prefix, sizeof(prefix), "k"); break;
					case 3: dps = 1; snprintf(prefix, sizeof(prefix), "k"); break;
					case 4: dps = 3; snprintf(prefix, sizeof(prefix), "M"); break;
					case 5: dps = 2; snprintf(prefix, sizeof(prefix), "M"); break;
				}
				break; // FUNCTION_OHMS

			case FUNCTION_CONTINUITY:
				snprintf(mmmode, sizeof(mmmode), "Continuity");
				snprintf(units, sizeof(units), "\u2126");
				dps = 1;
				break; // FUNCTION_CONTINUITY

			case FUNCTION_DIODE:
				snprintf(mmmode, sizeof(mmmode), "DIODE");
				snprintf(units, sizeof(units), "V");
				dps = 3;
				break; // FUNCTION_DIODE

			case FUNCTION_FQ_RPM:
				if (!(d[BYTE_STATUS] & STATUS_JUDGE)) {
					snprintf(mmmode, sizeof(mmmode), "Frequency");
					snprintf(units, sizeof(units), "Hz");
					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 3; snprintf(prefix, sizeof(prefix), "k"); break;
						case 1: dps = 2; snprintf(prefix, sizeof(prefix), "k"); break;
						case 2: dps = 1; snprintf(prefix, sizeof(prefix), "k"); break;
						case 3: dps = 3; snprintf(prefix, sizeof(prefix), "M"); break;
						case 4: dps = 2; snprintf(prefix, sizeof(prefix), "M"); break;
						case 5: dps = 1; snprintf(prefix, sizeof(prefix), "M"); break;
					} // switch

				} else {
					snprintf(mmmode, sizeof(mmmode), "RPM");
					snprintf(units, sizeof(units), "rpm");
					switch (d[BYTE_RANGE] & 0x0F) {
						case 0: dps = 2; snprintf(prefix, sizeof(prefix), "k"); break;
						case 1: dps = 1; snprintf(prefix, sizeof(prefix), "k"); break;
						case 2: dps = 3; snprintf(prefix, sizeof(prefix), "M"); break;
						case 3: dps = 2; snprintf(prefix, sizeof(prefix), "M"); break;
						case 4: dps = 1; snprintf(prefix, sizeof(prefix), "M"); break;
						case 5: dps = 0; snprintf(prefix, sizeof(prefix), "M"); break;
					} // switch
				}
				break; // FUNCTION_FQ_RPM

			case FUNCTION_CAPACITANCE:
				snprintf(mmmode, sizeof(mmmode), "Capacitance");
				snprintf(units, sizeof(units), "F");
				switch (d[BYTE_RANGE] & 0x0F) {
					case 0: dps = 3; snprintf(prefix, sizeof(prefix), "n"); break;
					case 1: dps = 2; snprintf(prefix, sizeof(prefix), "n"); break;
					case 2: dps = 1; snprintf(prefix, sizeof(prefix), "n"); break;
					case 3: dps = 3; snprintf(prefix, sizeof(prefix), "\u00B5"); break;
					case 4: dps = 2; snprintf(prefix, sizeof(prefix), "\u00B5"); break;
					case 5: dps = 1; snprintf(prefix, sizeof(prefix), "\u00B5"); break;
					case 6: dps = 3; snprintf(prefix, sizeof(prefix), "m"); break;
					case 7: dps = 2; snprintf(prefix, sizeof(prefix), "m"); break;
				}
				break; // FUNCTION_CAPACITANCE

			case FUNCTION_TEMPERATURE:
				snprintf(mmmode, sizeof(mmmode), "Temperature");
				if (d[BYTE_STATUS] & STATUS_JUDGE) {
					snprintf(units, sizeof(units), "\u00B0C");
				} else {
					snprintf(units, sizeof(units), "\u00B0F");
				}
				dps = 0;
				break; // FUNCTION_TEMPERATURE
		} // SWITCH

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
			snprintf(linetmp, sizeof(linetmp), "O.L.");

		} else {
			if (dps < 0) dps = 0;
			if (dps > 3) dps = 3;

			switch (dps) {
				case 0: snprintf(linetmp, sizeof(linetmp), "% 05.0f%s%s", v, prefix, units); break;
				case 1: snprintf(linetmp, sizeof(linetmp), "% 06.1f%s%s", v / 10, prefix, units); break;
				case 2: snprintf(linetmp, sizeof(linetmp), "% 06.2f%s%s", v / 100, prefix, units); break;
				case 3: snprintf(linetmp, sizeof(linetmp), "% 06.3f%s%s", v / 1000, prefix, units); break;
			}
		}

		snprintf(line1, sizeof(line1), "%-40s", linetmp);
		//		snprintf(line2, sizeof(line2), "%-40s", mmmode);
		//		snprintf(line3, sizeof(line3), "V.%03d", BUILD_VER);

		/*
		 *
		 * END OF DECODING
		 */


		snprintf(line1, sizeof(line1), "%-40s", linetmp);
		//		snprintf(line2, sizeof(line2), "%-40s", mmmode);
		//		snprintf(line3, sizeof(line3), "V.%03d", BUILD_VER);

		if (!g.quiet) fprintf(stdout,"%s\r",line1); fflush(stdout);

		if (comms_error == 1) {
			snprintf(line1, sizeof(line1), "COM.FLT");
		}

		// SDL Render
		// SDL Render
		// SDL Render
		if (1) {
			SDL_RenderClear(renderer);
			surface = TTF_RenderUTF8_Shaded(font, line1, g.font_color, g.background_color);
			texture = SDL_CreateTextureFromSurface(renderer, surface);

			int texW = 0;
			int texH = 0;
			SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
			SDL_Rect dstrect = { 0, 0, texW, texH };
			SDL_RenderCopy(renderer, texture, NULL, &dstrect);
			SDL_RenderPresent(renderer);
		} // SDL render section


		if (g.output_file) {
			/*
			 * Only write the file out if it doesn't
			 * exist. 
			 *
			 */
			if (!fileExists(g.output_file)) {
				FILE *f;
				fprintf(stderr,"%s:%d: output filename = %s\r\n", FL, g.output_file);
				f = fopen(tfn,"w");
				if (f) {
					fprintf(f,"%s", linetmp);
					fprintf(stderr,"%s:%d: %s => %s\r\n", FL, linetmp, tfn);
					fclose(f);
					rename(tfn, g.output_file);
				}
			}
		} // if outputfile

	} // while(!quit)

	if (g.serial_params.fd) close(g.serial_params.fd);

	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
	TTF_CloseFont(font);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	TTF_Quit();
	SDL_Quit();

	return 0;

}
