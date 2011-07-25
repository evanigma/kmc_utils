/*    This is kmc_control. kmc_control is part of the kmc_utils
 *    package for controlling and reading information and images 
 *    from Kodak Motion Corder(KMC) high speed digital cameras.
 *    
 *    kmc_control is the command line interface for sending commands and 
 *    receiving information over the RS-232C (serial) interface. It does 
 *    not allow one to transfer images from the KMC to a computer, which
 *    is done with kmc_read.
 *    
 *    kmc_control gives the user low level access such as pressing 
 *    individual buttons, or the user can use higher level commands to  
 *    change variables, change modes, play, record, etc. with a single
 *    command from any mode of the KMC.
 *
 *    Change Log:
 *		   Version 0.3.2  Added autoprobing of ROM to correct rec.rate error.
 *		   Version 0.3.1  Added mode switching. 
 *						  Fixed device selection.
 *						  Added variable setting. 
 *                        Added fix for older ROM version error.
 *		   Version 0.3.0  Merged with kmc_utils.
 *         version 0.0.2  Split library out into kmc_serial. 
 *
 *   
 *   Usage:
 *
 *      kmc_control [-d DEVICE] [OPTIONS] 
 *
 *
 *
 */
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <linux/hdreg.h>
#include <time.h>               /* added for random number */
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>			/* for delay timing: times */
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <math.h>

#define DEFAULT_DEVICE "/dev/ttyS0"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

char help = 0;
char *device_name;
int mode;

/*********************************************************************/
/********************** Help and Error Messaging**********************/
/*********************************************************************/
void morehelptext(char *helptext)
{
     fprintf(stderr, "\nThese are %s\n\n", helptext);
     fputs("Usage:"
           "\t\n"           
           "\t m                  determine the mode\n"
           "\t r                  determine if REC READY or not\n"
           "\t s                  show if LIVE or in SETUP mode\n"
           "\t p                  shows the playback status [STOP PLAY REV STEP]\n" 
           "\t t                  shows how you are triggering\n" 
           "\t u                  shows the number of memory boards\n"
           "\t f                  shows the recording rate\n"
           "\t d                  shows the display size\n"
           "\t e                  shows the shutter speed\n"
           "\t y                  gives the playback display rate\n",stderr);
 
}

void helptext(char *helptext)
{
     fprintf(stderr, "This is help for %s\n\n", helptext);
     fputs("Usage:"
           "\t\n"           
           "\t H                  Show help (This menu) information\n"
	   "\t q                  Show Addtional commands\n"
           "\t v                  Show Version Number\n"
           "\t R                  Record Ready Button\n"
           "\t T                  Trigger Button\n"
           "\t M                  Mode Button\n"
           "\t P                  Play Button\n"
           "\t S                  Stop Button\n"
           "\t D                  Down Arrow Button\n"
           "\t E                  Menu/Enter Button\n"
           "\t U                  Up ArrowButton\n"
           "\t Q                  Exit command for interactive mode\n",stderr);
           
}

void usage(char *errtext)
{
     if (strncmp(errtext,"",1) == 1) fprintf(stderr,  "Error: kmc_control - %s\n", errtext);
     fputs("Usage: kmc_control [-d DEVICE] [options]\n"
           "Options:\n"
           "\t -H                               Show help (usage) information\n"
           "\t -v                               Show version number\n"
           "\t -I                               Begin interactive mode\n"
           "\t -Q                               Full query\n"
           "\t -p                               Play\n"
           "\t -P                               PAL video standard\n"
           "\t -s                               Stop\n"
           "\t -r                               Record\n"
           "\t -c                               Clear Serial Buffer\n"
           "\t -n NUMBER                        Number of Trigger Pulses\n"
           "\t -t DELAY                         Time delay b/tw Trigger Pulses(in ms)\n"
           "\t -x                               Press Trigger Button until in LIVE mode\n"
	       "\t --pp, --pplay                    Press Play Button\n"
	       "\t --ps, --pstop, --pescape         Press Stop/Escape Button\n"
	       "\t --pu, --pup                      Press Up Button\n"
	       "\t --pd, --pdown                    Press Down Button\n"
	       "\t --pm, --pmode                    Press Mode Button\n"
	       "\t --pe, --penter, --pmenu          Press Enter/Menu Button\n"
	       "\t --pr, --precordready             Press Record Button\n"
	       "\t --pt, --ptrigger                 Press Trigger Button\n"
	       "\t --qdate                          Query Date\n"
	       "\t --qmode                          Query Mode\n"
	       "\t --qdisplaymode                   Query Display Mode\n"
	       "\t --qrecordready                   Query Record Ready\n"
	       "\t --qsetup                         Query Setup Mode\n"
	       "\t --qdisplaystatus                 Query Display Status\n"
	       "\t --qtrigger                       Query Trigger Mode\n"
	       "\t --qrandom                        Query Random Trigger Mode\n"
	       "\t --qmemory                        Query Memory\n"
	       "\t --qrom                           Query Version Increase\n"
	       "\t --qrecordrate                    Query Record Rate\n"
	       "\t --qsize                          Query Display Size\n"
	       "\t --qshutter                       Query Shutter Speed\n"
	       "\t --qplayrate                      Query Playback Rate\n"
	       "\t --qcount                         Query Frame Count\n"
	       "\t --qtime                          Query Time\n"
	       "\t --qedge                          Query Edge Enhancement\n"
	       "\t --qzoom                          Query Zoom\n"
	       "\t --qreticle                       Query Reticle\n"
	       "\t --qgamma                         Query Gamma\n"
	       "\t --qdisplay                       Query Display\n"
	       "\t --md, --mdisplay                 Set mode to Display\n"
	       "\t --ml, --mlive                    Set mode to Live\n"
	       "\t --mrr, --mrecordready            Set mode to Record Ready\n"
	       "\t --mdp, --mdisplayparameters      Set mode to Display Parameters\n"
	       "\t --mrp, --mrecordparameters       Set mode to Record Parameters\n"
	       "\t --mpr, --mrecordparametersrr     Set mode to Record Parameters(RR)\n"
           "\t --sr=FPS, --setrecordrate=FPS    Set Record Rate to FPS\n"
           "\t --sp=FPS, --setplayrate=FPS      Set Playback Rate to FPS\n"
           "\t --ss=SCSIID, --setscsiid=SCSIID  Set SCSI ID to SCSIID\n"
           "\t --sh=SPEED, --setshutter=SPEED   Set Shutter Speed to SPEED\n"
           "\t --sd=SIZE, --setdisplaysize=SIZE Set Display Size to SIZE\n"
           "\t --st=MODE, --settrigger=MODE     Set Trigger Mode to MODE\n"
           , stderr);
}


/* ************************************************************* */
/*
 * Interactive Mode
 */
kmc_control_interactive(int fd_kmc, struct termios * oldtio)
{
	char 	z;
	int		zz;

	z = '\x00';
	printf("\t\n\n*************Now in interactive mode****************\n\n");
  
	zz = kmc_query_display_mode(fd_kmc);
 
	printf("\nIf you are having trouble in LIVE mode I\n");
	printf("would suggest pressing the ESC/STOP button\n");
 
	while ( z != 'Q'){
		printf("\nPlease enter a command (help: H) : ");
		scanf("%s", &z);
		switch (z) {

			case 'H':
				helptext("Interactive Mode");
				break;
  
			case 'q':
				morehelptext("additional commands");
				break;

			case 'v':
				fprintf(stderr, "kmc_control version 0.3.2\n");
				break;
         
			case 'R':
				kmc_press_record_ready_button(fd_kmc);
				break;

			case 'T':
				kmc_press_trigger_button(fd_kmc);
				break;

			case 'M':
				kmc_press_mode_button(fd_kmc);
				break;

			case 'P':
				kmc_press_play_button(fd_kmc);
				break;

			case 'S':
				kmc_press_stop_escape_button(fd_kmc,zz);
				break;

			case 'D':
				kmc_press_down_button(fd_kmc,zz);
				break;

			case 'E':
				kmc_press_menu_enter_button(fd_kmc,zz);
				break;

			case 'U':
				kmc_press_up_button(fd_kmc,zz);
				break;
    
			case 'm':
				kmc_query_display_mode(fd_kmc);
				break;

			case 'r':
				kmc_query_record_ready(fd_kmc);
				break;

			case 's':
				kmc_query_setup_mode(fd_kmc);
				break;

			case 'p':
				kmc_query_display_status(fd_kmc);
				break;

			case 't':
				kmc_query_trigger_mode(fd_kmc);
				break;

			case 'u':
				kmc_query_n_memory_boards(fd_kmc);
				break;

			case 'f':
				kmc_query_record_rate(fd_kmc);
				break;

			case 'd':
				kmc_query_display_size(fd_kmc);
				break;

			case 'e':
				kmc_query_shutter_speed(fd_kmc);
				break;

			case 'y':
				kmc_query_playback_rate(fd_kmc);
				break;
		}
	}
	close_serial_device(fd_kmc, &oldtio);
	exit(0);
}


/*********************************************************************/
int main(int argc, char **argv)
{
	int digit_optind = 0;
	int fd_kmc, res, n, m, num, trigger_interval;
	char write_buff[1], r_buff,c;
	char cur_option[40];	
	int fps, scsiid, shutterspeed;
	char displaysize[20];
	char triggermode[20];
	int initialized=0;
	struct termios oldtio;
	int Ntriggers=100;		/* default value if none is set */
	int palvideo=0;

	if( argc <= 1 ){
		usage("too few arguments");
		exit(0); 
	}


	while (1)
	{
		int this_option_iptind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = 	
							/* (char *name, has_arg, int *flag, int val)  */
							/* has_arg: 0 (none), 1 (yes), 2(optional)    */
							/* flag: 									  */
							/* val: value to return or load into flag var */
		{
			/* Action options */

			/* Press Button options */
			{"pp", 0, 0, 0},
			{"pplay", 0, 0, 0},
			{"ps", 0, 0, 0},
			{"pstop", 0, 0, 0},
			{"pescape", 0, 0, 0},
			{"pd", 0, 0, 0},
			{"pdown", 0, 0, 0},
			{"pu", 0, 0, 0},
			{"pup", 0, 0, 0},
			{"pm", 0, 0, 0},
			{"pmode", 0, 0, 0},
			{"pe", 0, 0, 0},
			{"penter", 0, 0, 0},
			{"pmenu", 0, 0, 0},
			{"pr", 0, 0, 0},
			{"precordready", 0, 0, 0},
			{"pt", 0, 0, 0},
			{"ptrigger", 0, 0, 0},

			/* Query options */
			{"qdate", 0, 0, 0},
			{"qmode", 0, 0, 0},
			{"qdisplaymode", 0, 0, 0},
			{"qrecordready", 0, 0, 0},
			{"qsetup", 0, 0, 0},
			{"qdisplaystatus", 0, 0, 0},
			{"qtrigger", 0, 0, 0},
			{"qrandom", 0, 0, 0},
			{"qmemory", 0, 0, 0},
			{"qrom", 0, 0, 0},
			{"qrecordrate", 0, 0, 0},
			{"qsize", 0, 0, 0},
			{"qshutter", 0, 0, 0},
			{"qplayrate", 0, 0, 0},
			{"qcount", 0, 0, 0},
			{"qtime", 0, 0, 0},
			{"qedge", 0, 0, 0},
			{"qzoom", 0, 0, 0},
			{"qreticle", 0, 0, 0},
			{"qgamma", 0, 0, 0},
			{"qdisplay", 0, 0, 0},

			/* Set mode options*/
			{"md", 0, 0, 0},
			{"mdisplay", 0, 0, 0},
			{"ml", 0, 0, 0},
			{"mlive", 0, 0, 0},
			{"mrr", 0, 0, 0},
			{"mrecordready", 0, 0, 0},
			{"mdp", 0, 0, 0},
			{"mdisplayparameters", 0, 0, 0},
			{"mrp", 0, 0, 0},
			{"mrecordparameters", 0, 0, 0},
			{"mpr", 0, 0, 0},
			{"mrecordparametersrr", 0, 0, 0},

			/* Set variable options */
			{"sr", 1, 0, 0},
			{"setrecordrate", 1, 0, 0},
			{"sp", 1, 0, 0},
			{"setplayrate", 1, 0, 0},
			{"ss", 1, 0, 0},
			{"setscsiid", 1, 0, 0},
			{"sh", 1, 0, 0},
			{"setshutter", 1, 0, 0},
			{"sd", 1, 0, 0},
			{"setdisplaysize", 1, 0, 0},
			{"st", 1, 0, 0},
			{"settrigger", 1, 0, 0},
			{0,0,0,0} 
		};

	c = getopt_long(argc, argv, "d:HvIpPsrQct:n:x", long_options, &option_index);
	if (c == -1)
		break;


	/* Make sure it is initialized first */
	switch (c)
		{
		case 'd':
			fd_kmc = kmc_initialize_serial(optarg,&oldtio);
			initialized=1;
			break;

		default:
			if (initialized==0) {
				fd_kmc = kmc_initialize_serial(DEFAULT_DEVICE,&oldtio);
				initialized=1;
			}
			break;
		}

	/* Now deal with all non-initilization related paramaters */
	switch (c)
		{
		case 'd':
			break;			

		case 0:			/* Long options */
			sprintf(cur_option,"%s",long_options[option_index].name);

//			printf ("option %s", long_options[option_index].name);
//			if (optarg)
//				printf (" with arg %s", optarg);
//			printf ("\n");


			/* *****   Action options   ***** */


			/* *****   Press Button options   ***** */
			if ( (strcmp(cur_option,"pp") == 0) || 
				 (strcmp(cur_option,"pplay") == 0) )
				kmc_press_play_button(fd_kmc);

			if ( (strcmp(cur_option,"ps") == 0) || 
				 (strcmp(cur_option,"pstop") == 0) ||
				 (strcmp(cur_option,"pescape") == 0) )
				kmc_press_stop_escape_button(fd_kmc);

			if ( (strcmp(cur_option,"pd") == 0) || 
				 (strcmp(cur_option,"pdown") == 0) )
				kmc_press_down_button(fd_kmc);

			if ( (strcmp(cur_option,"pu") == 0) || 
				 (strcmp(cur_option,"pup") == 0) )
				kmc_press_up_button(fd_kmc);

			if ( (strcmp(cur_option,"pm") == 0) || 
				 (strcmp(cur_option,"pmode") == 0) )
				kmc_press_mode_button(fd_kmc);

			if ( (strcmp(cur_option,"pe") == 0) || 
				 (strcmp(cur_option,"penter") == 0) ||
				 (strcmp(cur_option,"pmenu") == 0) )
				kmc_press_menu_enter_button(fd_kmc);

			if ( (strcmp(cur_option,"pr") == 0) || 
				 (strcmp(cur_option,"precordready") == 0) )
				kmc_press_record_ready_button(fd_kmc);

			if ( (strcmp(cur_option,"pt") == 0) || 
				 (strcmp(cur_option,"ptrigger") == 0) )
				kmc_press_trigger_button(fd_kmc);


			/* ******   Query options   ***** */
			if (strcmp(cur_option,"qdate") == 0)
				printf("Insert function call here.\n");

			if (strcmp(cur_option,"qmode") == 0)
				kmc_query_mode(fd_kmc);

			if (strcmp(cur_option,"qdisplaymode") == 0)
				kmc_query_display_mode(fd_kmc);

			if (strcmp(cur_option,"qrecordready") == 0)
				kmc_query_record_ready(fd_kmc);

			if (strcmp(cur_option,"qsetup") == 0)
				kmc_query_setup_mode(fd_kmc);

			if (strcmp(cur_option,"qdisplaystatus") == 0)
				kmc_query_display_status(fd_kmc);

			if (strcmp(cur_option,"qtrigger") == 0)
				kmc_query_trigger_mode(fd_kmc);

			if (strcmp(cur_option,"qrandom") == 0)
				kmc_query_random_mode(fd_kmc);

			if (strcmp(cur_option,"qmemory") == 0)
				kmc_query_n_memory_boards(fd_kmc);

			if (strcmp(cur_option,"qrom") == 0)
				kmc_query_rom_version_increase(fd_kmc);

			if (strcmp(cur_option,"qrecordrate") == 0)
				kmc_query_record_rate(fd_kmc);

			if (strcmp(cur_option,"qsize") == 0)
				kmc_query_display_size(fd_kmc);

			if (strcmp(cur_option,"qshutter") == 0)
				kmc_query_shutter_speed(fd_kmc);

			if (strcmp(cur_option,"qplayrate") == 0)
				kmc_query_playback_rate(fd_kmc,palvideo);

			if (strcmp(cur_option,"qcount") == 0)
				printf("Insert function call here.\n");

			if (strcmp(cur_option,"qtime") == 0)
				printf("Insert function call here.\n");

			if (strcmp(cur_option,"qedge") == 0)
				printf("Insert function call here.\n");

			if (strcmp(cur_option,"qzoom") == 0)
				printf("Insert function call here.\n");

			if (strcmp(cur_option,"qreticle") == 0)
				printf("Insert function call here.\n");

			if (strcmp(cur_option,"qdisplay") == 0)
				printf("Insert function call here.\n");


			/* *****   Set mode options   ***** */

			if ( (strcmp(cur_option,"md") == 0) || 
				 (strcmp(cur_option,"mdisplay") == 0) )
				kmc_set_mode_to_DISPLAY(fd_kmc);
				
			if ( (strcmp(cur_option,"ml") == 0) || 
				 (strcmp(cur_option,"mlive") == 0) )
				kmc_set_mode_to_LIVE(fd_kmc);
				
			if ( (strcmp(cur_option,"mrr") == 0) || 
				 (strcmp(cur_option,"mrecordready") == 0) )
				kmc_set_mode_to_RECORD_READY(fd_kmc);

			if ( (strcmp(cur_option,"mdp") == 0) || 
				 (strcmp(cur_option,"mdisplayparameters") == 0) )
				kmc_set_mode_to_DISPLAY_PARAMETERS(fd_kmc);

			if ( (strcmp(cur_option,"mrp") == 0) || 
				 (strcmp(cur_option,"mrecordparameters") == 0) )
				kmc_set_mode_to_RECORD_PARAMETERS(fd_kmc);

			if ( (strcmp(cur_option,"mpr") == 0) || 
				 (strcmp(cur_option,"mrecordparametersrr") == 0) )
				kmc_set_mode_to_RECORD_PARAMETERS_RR(fd_kmc);


			/* *****   Set variable options   ***** */

			if ( (strcmp(cur_option,"sr") == 0) ||
			     (strcmp(cur_option,"setrecordrate") == 0) ) {
				fps = atoi(optarg);
				//printf("Desired record rate: %d\n",fps);
				kmc_set_record_rate(fd_kmc,fps);
				}

			if ( (strcmp(cur_option,"sp") == 0) ||
			     (strcmp(cur_option,"setplayrate") == 0) ) {
				fps = atoi(optarg);
				kmc_set_play_rate(fd_kmc,fps,palvideo);
				}

			if ( (strcmp(cur_option,"ss") == 0) ||
			     (strcmp(cur_option,"setscsiid") == 0) ) {
				scsiid = atoi(optarg);
				printf("Desired SCSI ID: %d\n",scsiid);
				kmc_set_scsi_id(fd_kmc,scsiid);
				}

			if ( (strcmp(cur_option,"sh") == 0) ||
			     (strcmp(cur_option,"setshutter") == 0) ) {
				shutterspeed = atoi(optarg);
				printf("Desired shutter speed: %d\n",shutterspeed);
				kmc_set_shutter_speed(fd_kmc,shutterspeed);
				}

			if ( (strcmp(cur_option,"sd") == 0) ||
			     (strcmp(cur_option,"setdisplaysize") == 0) ) {
				sprintf(displaysize,"%s",optarg);
				kmc_set_display_size(fd_kmc,displaysize);
				}

			if ( (strcmp(cur_option,"st") == 0) ||
			     (strcmp(cur_option,"settrigger") == 0) ) {
				sprintf(triggermode,"%s",optarg);
				kmc_set_trigger_mode(fd_kmc,triggermode);
				}

			break;


		case 'H':
			usage("");
			break;

		case 'v':
			fprintf(stderr, "kmc_control version 0.3.2\n");
			break;

		case 'Q':
			kmc_serial_full_query(fd_kmc,palvideo);
			break;

		case 'I':
			kmc_control_interactive(fd_kmc,&oldtio);
			break;

		case 'p':
			kmc_play(fd_kmc);
			break;

		case 'P':
			palvideo = 1;
			break;

		case 's':
			kmc_stop(fd_kmc);
			break;

		case 'r':
			kmc_record(fd_kmc);
			break;

		case 'c':
			kmc_clear_serial_buffer(fd_kmc);
			break;

		case 'n':
			Ntriggers = atoi(optarg);
			break;

		case 't':
			trigger_interval = atoi(optarg);
			kmc_time_delay_trigger(fd_kmc,trigger_interval,Ntriggers);
			break;

		case 'x':
			kmc_trigger_until_live(fd_kmc);
			break;

		default:
			//printf("?? getopt returned character code 0%o ??\n",c);
		}
	}
	if (optind < argc)
		{
		printf ("non-option ARGV-elements: ");
		while (optind < argc)
		printf ("%s ", argv[optind++]);
		printf ("\n");
	}

	close_serial_device(fd_kmc,&oldtio);
}

