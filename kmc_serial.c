/* Library functions for communicating with Kodak Motion Corder SR high
 * speed digital cameras using serial interface.
 *
 *
 * Dan Blair & Dan Mueth - 09/01/99.  Version 0.3.2
 *
 * Change Log:
 *
 * Version 0.3.2: Autoprobes ROM version to correct for frame rate BIOS error.
 * Version 0.3.1: Adding higher level controls.
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <sys/times.h>          /* for delay timing: times */
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <math.h>

#include "kmc_serial.h"

#define BAUDRATE B4800 			/* baudrate settings are defined in 
									<asm/termbits.h>, which is included 
									by <termios.h> */
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define SHORTEST_TRIGGER_INTERVAL 90
#define NTRIGGERS_PER_GROUP 100

/*
 * Lists of Menus.  
 * These lists are used for:
 *		1) Validating settings
 *		2) Maneuvering the lists
 * Thus, these need to be in the order presented (top-down) on the menus
 */
#define	ROM_VERSION_FIXED	7	/* Corresponding to x07 */
int N_RECORD_RATES = 10;
int	RECORD_RATES_MENU[] = {30,60,125,250,500,1000,2000,3000,5000,10000};
int	RECORD_RATES_READOUT_FIXED[] = 
				{30,60,125,250,500,1000,2000,3000,5000,10000};
int	RECORD_RATES_READOUT_EARLY[] = 
				{125,60,30,250,500,1000,2000,3000,5000,10000};

int N_DISPLAY_SIZES = 7;
char *DISPLAY_SIZES[] = {"128x40","128x80","128x120","256x120","256x240","512x240","512x480"};

int N_TRIGGER_MODES= 4;
char *TRIGGER_MODES[] = {"START","CENTER","END","RANDOM"};

int N_DISPLAY_RATES= 10;
int	DISPLAY_RATES_NTSC[] = {1,2,5,10,15,30,60,120,240,1200};
int	DISPLAY_RATES_PAL[] = {1,2,5,10,25,50,100,200,250,1000};

int N_SHUTTER_SPEEDS= 9;
int	SHUTTER_SPEEDS[] = {30,60,125,250,500,1000,2000,3000,5000,10000,20000};



/*********************************************************************/
/*****************   Basic Functions   *******************************/
/*********************************************************************/

/*********************************************************************/
/*
 * Sleep function.  Modes are not changed properly if commands
 * are blasted at the KMC.  So, for example, after changing a mode,
 * one must wait a short time before querying the mode of changing
 * the mode again.
 *
 * If we are careful to call kmc_sleep() only after pushing buttons,
 * we can still get fair performance for triggering.
 *
 * Chris at Kodak recommends 8 to 10 ms.
 *
 */
void kmc_sleep()
{
	usleep(20000);			/* 10ms is too short, 20ms works */
}



/*
 * Some things are very slow, such as changing the recording rate.
 */
void kmc_longsleep()
{
	usleep(100000);			/* 50 ms is too short, 100ms works  */
}

/*********************************************************************/
// This function is for universal reading and writing for the 
// status query commands.

char kmc_wr(int fd, char mode_wbuff[]) 
{
	int w,r;
	char mode_rbuff[1], tip;

	//printf("writing...\n");
	w = write(fd, mode_wbuff , 1);
	if ( w < 0 ){
		perror("kmc_serial(kmc_wr): write error\n");
		exit(1);
	}

	//kmc_sleep();		/* To avoid unhappiness */

	//printf("reading...\n");
	r=0; 
	while( r != 1 ){
		r = read(fd, mode_rbuff, 1);
		if ( r < 0 ){
			perror("kmc_serial(kmc_wr): read error\n");
			exit(1);
		} 
	}
	tip = mode_rbuff[0];
	kmc_sleep();		/* To avoid unhappiness */
	return(tip);
}

/*
 * See if there is any device responding on serial port
 */
int kmc_detect_serial_device(int fd)
{
	int w,r;
	int out=0;
	char mode_rbuff[1],mode_wbuff[1];

	printf("running kmc_detect_serial_device\n");

	mode_wbuff[0] = '\x81';

	//printf("writing...\n");
	w = write(fd, mode_wbuff , 1);
	if ( w < 0 ){
		perror("kmc_serial(kmc_detect_serial_device): write error\n");
		exit(1);
	}


	usleep(20000);			/* Give it time to respond */
	r = read(fd, mode_rbuff, 1);
	if ( r==1 ) out = 1;

	return out;
}

void kmc_clear_serial_buffer(int fd) 
{
	int r;
	char mode_rbuff[1], tip;

	r = read(fd, mode_rbuff, 1);
	if ( r < 0 ){
		perror("kmc_serial(kmc_wr): read error\n");
		exit(1);
		} 
	if (r == 1) {
		printf("Characters found.\n");
		while( r == 1 ){
			tip = mode_rbuff[0];
			printf("character: %c\n",tip);
			r = read(fd, mode_rbuff, 1);
			if ( r < 0 ){
				perror("kmc_serial(kmc_wr): read error\n");
				exit(1);
			} 
		}
		tip = mode_rbuff[0];
		printf("character: %c\n",tip);
	}
}

/*********************************************************************/
/*******   Initialize/Close Serial Communications   ******************/
/*********************************************************************/
/*********************************************************************/
/*
 * Initialize serial communications
 */
int kmc_initialize_serial(char *device, struct termios * oldtio)
{
    int fd_kmc;
    struct termios newtio;

    /*
     *  Open serial device (kmc) reading and writing and not as controlling tty
     *  because we don't want to get killed if linenoise sends CTRL-C.
     */

    //printf("Initializing device %s\n",device);

    fd_kmc = open(device, O_RDWR | O_NOCTTY );
    if (fd_kmc <0) {perror("kmc_initialize_serial(open)"); exit(-1); }

    tcgetattr(fd_kmc,oldtio); /* save current serial port settings */
    bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */


    /*
     *  BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
     *  CRTSCTS : output hardware flow control (only used if the cable has
     *            all necessary lines. See sect. 7 of Serial-HOWTO)
     *  CS8     : 8n1 (8bit,no parity)
     *  CSTOPB  : Sets stop bit at 2 stop bits
     *  CLOCAL  : local connection, no modem contol
     *  CREAD   : enable receiving characters
     */
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD | CSTOPB;

    /*
     *    IGNPAR  : ignore bytes with parity errors
     *    ICRNL   : map CR to NL (otherwise a CR input on the other computer
     *              will not terminate input)
     *    otherwise make device raw (no other input processing)
     */
    newtio.c_iflag = IGNPAR | ICRNL;

    /*
     * Raw output.
     */
    newtio.c_oflag = 0;

    /*
     *  Initialize all control characters.
     *  Default values can be found in /usr/include/termios.h, and
     *  are given in the comments, but we don't need them here.
     */

    /*
     *  These commands were included as part of the serial programming howto.
     *  I am leaving them out for now, just due to the fact that I don't
     *  really know how to use them
     */
    newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */
    newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
    newtio.c_cc[VERASE]   = 0;     /* del */
    newtio.c_cc[VKILL]    = 0;     /* @ */
    newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
    newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 0;     /* blocking read until 1 character arrives */    newtio.c_cc[VSWTC]    = 0;     /* '\0' */
    newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */
    newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
    newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
    newtio.c_cc[VEOL]     = 0;     /* '\0' */
    newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
    newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
    newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
    newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
    newtio.c_cc[VEOL2]    = 0;     /* '\0' */

    tcflush(fd_kmc, TCIFLUSH);
    tcsetattr(fd_kmc,TCSANOW,&newtio);

    return fd_kmc;
}

/* ************************************************************* */
/*
 *  Restore old serial communications settings and close device
 */
void close_serial_device(int fd_kmc, struct termios * oldtio)
{
    tcsetattr(fd_kmc,TCSANOW,oldtio);
    close(fd_kmc);
}


/*********************************************************************/
/*****************   Query Functions   *******************************/
/*********************************************************************/


/************************************************/
/*	
 *	Get Display mode.
 *	Returns 0 for NOT DISPLAY mode 
 *	Returns 1 for DISPLAY mode 
 */
int kmc_get_display_mode(int fd){

	char mode_wbuff[1],out ;

	mode_wbuff[0] = '\x83';

	out = kmc_wr(fd, mode_wbuff);

	if( out == '\x00')
		return 1;
	if( out == '\xFF')
		return 0;  

} 

int kmc_query_display_mode(int fd){

	char mode_wbuff[1],out ;

	mode_wbuff[0] = '\x83';

	out = kmc_wr(fd, mode_wbuff);

	if( out == '\x00'){
		printf("DISPLAY MODE\n");
		return 0;
	}
	if( out == '\xFF'){
		printf("LIVE MODE\n");
		return 1;  
	}
} 

/*********************************************************************/
/*	Get Record Ready mode.
 *	Returns 0 for NOT RECORD READY mode 
 *	Returns 1 for RECORD READY mode 
 */
int kmc_get_record_ready(int fd)
{
	char mode_wbuff[1], out ;

	mode_wbuff[0] = '\x84';

	out = kmc_wr(fd, mode_wbuff);

	if( out == '\x00' ) return 0;
	if( out == '\xFF' ) return 1;

}

/*********************************************************************/
 
void kmc_query_record_ready(int fd)
{
	int val;

	val = kmc_get_record_ready(fd);

	if ( val == 0 ) printf("NOT RECORD READY\n");
	if ( val == 1 ) printf("RECORD READY\n");
}

/*********************************************************************/

int kmc_get_setup_mode(int fd)
{
	char mode_wbuff[1], out ;
	int retval=-1;

	mode_wbuff[0] = '\x85';

	out = kmc_wr(fd, mode_wbuff);

	if(out == '\x00') retval=0;
	if(out == '\xFF') retval=1;
	if ( (out != '\x00') && (out != '\xFF') )  retval=-1;

	if (retval == -1) {
		printf("Invalid setup mode.\n");
		exit(0);
	}

	return retval;
}

/*********************************************************************/

void kmc_query_setup_mode(int fd)
{
	int val;

	val = kmc_get_setup_mode(fd);

	if (val == 0) printf("NOT SETUP MODE\n");
	if (val == 1) printf("SETUP MODE\n");
}

/*********************************************************************/

void kmc_query_display_status(int fd)
{
	char mode_wbuff[1], out;

	mode_wbuff[0] = '\x86';

	out = kmc_wr(fd, mode_wbuff);

	if(out == '\x00')
		printf(" STOP mode\n");
	if(out == '\x01')
		printf(" PLAY mode\n");
	if(out == '\x02')
		printf(" REV mode\n");
	if(out == '\x03')
		printf(" STEP mode\n");
	if(out == '\x04')
		printf(" you are at the first frame in the buffer....\n");

	/* should probably make a call to input function here */
}

/*********************************************************************/

void kmc_get_trigger_mode(int fd, char *triggermode)
{
	char mode_wbuff[1], out;

	mode_wbuff[0] = '\x87';
	out = kmc_wr(fd, mode_wbuff);

	if(out == '\x00') sprintf(triggermode,"Start");
	if(out == '\x01') sprintf(triggermode,"Center");
	if(out == '\x02') sprintf(triggermode,"End");
	if(out == '\x03') sprintf(triggermode,"Random");
}

void kmc_query_trigger_mode(int fd)
{
	int rmode;
	char triggermode[20];

	kmc_get_trigger_mode(fd,triggermode);
	printf("Trigger mode: %s\n",triggermode);

	if ( strncmp(triggermode,"random",6) == 0) {
		rmode = kmc_get_random_mode(fd);
		if(rmode == 1) printf("Random Trigger mode: 1 frame burst\n");
		if(rmode == 2) printf("Random Trigger mode: 10 frame burst\n");
		if(rmode == 3) printf("Random Trigger mode: 50 frame burst\n");
	}
}

/*********************************************************************/
/* 	Get the Random Mode:
 *		Returns 1 for 1 frame
 *		Returns 2 for 10 frames
 *		Returns 3 for 50 frames
 */
int kmc_get_random_mode(int fd)
{
	int rmode;
	char mode_wbuff[1], out;

	mode_wbuff[0] = '\x88';

	out = kmc_wr(fd, mode_wbuff);

	if(out == '\x00') rmode = 1;
	if(out == '\x01') rmode = 2;
	if(out == '\x02') rmode = 3;

	return(rmode); 
}

void kmc_query_random_mode(int fd)
{
	int rmode;

	rmode=kmc_get_random_mode(fd);
	
	if (rmode == 1) printf("Random mode: 1 frame\n");
	if (rmode == 2) printf("Random mode: 10 frames\n");
	if (rmode == 3) printf("Random mode: 50 frames\n");

}

/*********************************************************************/

int kmc_get_rom_version_increase(int fd)
{
	int version_increase;
	char mode_wbuff[1], out;

	mode_wbuff[0] = '\x81';
	out = kmc_wr(fd, mode_wbuff);

	version_increase = out - '\x00';
	return version_increase;
}

void kmc_query_rom_version_increase(int fd)
{
	int version_increase;

	version_increase = kmc_get_rom_version_increase(fd);
	printf("ROM version increase: %d\n",version_increase);	
}

/*********************************************************************/

int kmc_get_n_memory_boards(int fd)
{
	int nboards=-1;
	char mode_wbuff[1], out;

	mode_wbuff[0] = '\x82';
	out = kmc_wr(fd, mode_wbuff);

	if(out == '\x00') nboards = 0;
	if(out == '\x01') nboards = 1;
	if(out == '\x02') nboards = 2;

	return nboards;
}


void kmc_query_n_memory_boards(int fd)
{
	int nboards;

	nboards = kmc_get_n_memory_boards(fd);
	printf("Number of additional memory boards: %d\n",nboards);
}

/****************************************************/
int kmc_get_record_rate(int fd)
{
	int pos, rom_version, fps;
	char mode_wbuff[1], out;

	mode_wbuff[0] = '\x89';

	out = kmc_wr(fd, mode_wbuff);

	/*
	 * Some models have the 30fps and 125fps switched, so have
	 * to be a little more generic here...
	 */
	if(out == '\x00') pos = 0;
	if(out == '\x01') pos = 1;
	if(out == '\x02') pos = 2;
	if(out == '\x03') pos = 3;
	if(out == '\x04') pos = 4;
	if(out == '\x05') pos = 5;
	if(out == '\x06') pos = 6;
	if(out == '\x07') pos = 7;
	if(out == '\x08') pos = 8;
	if(out == '\x09') pos = 9;

	rom_version = kmc_get_rom_version_increase(fd);
	if (rom_version < ROM_VERSION_FIXED )
		fps = RECORD_RATES_READOUT_EARLY[pos];	
	if (rom_version >= ROM_VERSION_FIXED )
		fps = RECORD_RATES_READOUT_FIXED[pos];	

	return fps;
}

/****************************************************/

void kmc_query_record_rate(int fd)
{
	int fps;

	fps = kmc_get_record_rate(fd);
	printf("Recording Rate: %d fps\n",fps);
}

/*********************************************************************/

void kmc_get_display_size(int fd, char *displaysize)
{
	char mode_wbuff[1], out;

	mode_wbuff[0] = '\x8A';
	out = kmc_wr(fd, mode_wbuff);

	if(out == '\x00') sprintf(displaysize,"128x40");
	if(out == '\x01') sprintf(displaysize,"128x80");
	if(out == '\x02') sprintf(displaysize,"128x120");
	if(out == '\x03') sprintf(displaysize,"256x120");
	if(out == '\x04') sprintf(displaysize,"256x240");
	if(out == '\x05') sprintf(displaysize,"512x240");
	if(out == '\x06') sprintf(displaysize,"512x480");

}


void kmc_query_display_size(int fd)
{
	char displaysize[20];

	kmc_get_display_size(fd, displaysize);
	printf("Display size: %s\n",displaysize);
}


/*********************************************************************/

int kmc_get_shutter_speed(int fd)
{
	int fps, shutter_speed_number, shutter_speed_index,
				shutter_speed, i;
	char mode_wbuff[1], out;
	int pos=-1;

	mode_wbuff[0] = '\xA6';

	out = kmc_wr(fd, mode_wbuff);

	/*
	 * Note the shutter speed returned is a number
	 * from 1 to 7, and one must also know the recording
	 * frame rate to calculate the actual shutter speed.
	 */
	if(out == '\x00') shutter_speed_number = 1;
	if(out == '\x01') shutter_speed_number = 2;
	if(out == '\x02') shutter_speed_number = 3;
	if(out == '\x03') shutter_speed_number = 4;
	if(out == '\x04') shutter_speed_number = 5;
	if(out == '\x05') shutter_speed_number = 6;
	if(out == '\x06') shutter_speed_number = 7;
	//printf("Shutter Speed Number: %d\n",shutter_speed_number);

	fps =  kmc_get_record_rate(fd);
	//printf("Record Rate: %d\n",fps);

	for (i=0; i<N_RECORD_RATES ; i++) {
		if ( RECORD_RATES_MENU[i] == fps ) 
			pos=i;
		}
	shutter_speed_index = shutter_speed_number+pos-1;
	//printf("Shutter Speed Index: %d\n",shutter_speed_index);
	shutter_speed = SHUTTER_SPEEDS[shutter_speed_index];

	return shutter_speed;
}


void kmc_query_shutter_speed(int fd)
{
	int		shutter_speed;

	shutter_speed = kmc_get_shutter_speed(fd);

	printf("Shutter speed: 1/%d\n",shutter_speed);

}

/*********************************************************************/

int kmc_get_playback_rate(int fd, int palvideo)
{
	int pr=-1;
	char mode_wbuff[1], out;
	int index;

	mode_wbuff[0] = '\xA7';

	out = kmc_wr(fd, mode_wbuff);

	index = out - '\x00';
	if (palvideo == 0) pr = DISPLAY_RATES_NTSC[index];
	if (palvideo != 0) pr = DISPLAY_RATES_PAL[index];

	return pr;
}


void kmc_query_playback_rate(int fd, int palvideo)
{
	int pr;

	pr = kmc_get_playback_rate(fd,palvideo);
	printf("Playback Rate: %d\n",pr);
}


/*
 * Full Query of All Parameters 
 */
void kmc_serial_full_query(int fd, int palvideo)
{
	kmc_query_mode(fd);
//	kmc_query_display_mode(fd);
//	kmc_query_record_ready(fd);
//	kmc_query_setup_mode(fd);
	kmc_query_display_size(fd);
	kmc_query_record_rate(fd);
	kmc_query_shutter_speed(fd);
	kmc_query_trigger_mode(fd);
	kmc_query_playback_rate(fd,palvideo);
	kmc_query_n_memory_boards(fd);
	kmc_query_rom_version_increase(fd);
}


/*************************************************************************/
/*****************  Functions to Press Control Panel Buttons *************/
/*************************************************************************/

void kmc_press_mode_button(int fd)
{         

	char w_buff[1];
	int wr_fd;   

	if( kmc_get_record_ready(fd) == 1 ){
		fprintf(stderr,"you cannot change modes while in RECORD READY\n\n");
		return;
	}

	w_buff[0]= '\x71';

	wr_fd = write(fd, w_buff,1);

	if (wr_fd < 0 ){
		perror("write error please retry\n");
		exit(1);
	}

	kmc_sleep();
}      

/*********************************************************************/

void kmc_press_record_ready_button(int fd)
{
	char w_buff[1];
	int wr_fd;

	w_buff[0] = '\x73';

	wr_fd = write(fd, w_buff,1);
	if (wr_fd < 0 ){
		perror("write error please retry\n");
		exit(1);
	}

	kmc_sleep();
}

/*********************************************************************/

void kmc_press_trigger_button(int fd)
{
	int wr_fd;
	char w_buff[1];

	w_buff[0] = '\x74';

	wr_fd = write(fd, w_buff,1);

	if (wr_fd < 0 ){
		perror("write error please retry\n");
		return;
	}
}

void kmc_press_trigger_button_blocking(int fd)
{
	printf("Recording ...\n");

	kmc_press_trigger_button_blocking(fd);

	while ( kmc_get_record_ready(fd) == 1) {
		/* This does not work right... */
		//printf(".");
		sleep(1);
	}
	printf("Done\n");
}

/*********************************************************************/

void kmc_press_play_button(int fd)
{
	int wr_fd;
	char w_buff[1];

	w_buff[0] = '\x65';

	wr_fd = write(fd, w_buff,1);
	if (wr_fd < 0 ){
		perror("write error please retry\n");
		exit(1);
	}

	kmc_sleep();

}

/*********************************************************************/

void kmc_press_stop_escape_button(int fd)
{
	int wr_fd;
	char w_buff[1];

	w_buff[0] = '\x68';

	wr_fd = write(fd, w_buff,1);

	if (wr_fd < 0 ){
		perror("write error please retry\n");
		exit(1);
	}

	kmc_sleep();
}

/*********************************************************************/

void kmc_press_up_button(int fd)
{
	int wr_fd;
	char w_buff[1];

	w_buff[0] = '\x6A';

	wr_fd = write(fd, w_buff,1);

	if (wr_fd < 0 ){
		perror("write error please retry\n");
		exit(1);
	}

	kmc_sleep();
}

/*********************************************************************/

void kmc_press_menu_enter_button(int fd )
{
	int wr_fd;
	char w_buff[1];

	w_buff[0] = '\x72';

	wr_fd = write(fd, w_buff,1);
	if (wr_fd < 0 ){
		perror("write error please retry\n");
		exit(1);
	}

    kmc_sleep();	 
}


/*********************************************************************/

void kmc_press_down_button(int fd)
{
	int wr_fd;
	char w_buff[1];

	w_buff[0] = '\x69';

	wr_fd = write(fd, w_buff,1);
	if (wr_fd < 0 ){
		perror("write error please retry\n");
		exit(1);
	}

	kmc_sleep();
}

/*************************************************************************/
/***************************  Action Functions  **************************/
/*************************************************************************/

void kmc_play(int fd)
{

	kmc_set_mode_to_DISPLAY(fd);
	kmc_press_play_button(fd);

}

void kmc_stop(int fd)
{

	kmc_press_stop_escape_button(fd);

}


void kmc_record(int fd)
{

	kmc_set_mode_to_RECORD_READY(fd);
	kmc_press_trigger_button(fd);

}

/*************************************************************************/
/************************  Higher Level Functions  ***********************/
/*************************************************************************/

/*
 * Move up or down a list
 */
void kmc_move_in_list(int fd, int relative_position)
{
	int i;

	if (relative_position > 0) {
		for (i=0; i<relative_position; i++) {
			kmc_press_down_button(fd);
			//printf("moving down\n");
			kmc_sleep();
		}
	}
	if (relative_position < 0) {
		for (i=0; i<(-relative_position); i++) {
			kmc_press_up_button(fd);
			//printf("moving up\n");
			kmc_sleep();
		}
	}
}

/* 
 * Determine which mode the KMC is in.
 * Define modes:
 *					1	Display
 *					2	Live
 *					3	Record Ready
 *					4	Display Parameters
 *					5	Record Parameters (Not RR)
 *					6	Record Parameters (RR)
 */
int kmc_get_mode(int fd)
{
	int smode, rmode, dmode;
	int mode=0;

	dmode = kmc_get_display_mode(fd);
	rmode = kmc_get_record_ready(fd);
	smode = kmc_get_setup_mode(fd);
	
	if ( (dmode == 1) && (rmode == 0) && (smode == 0) )
		mode = 1;	
	if ( (dmode == 0) && (rmode == 0) && (smode == 0) )
		mode = 2;	
	if ( (dmode == 0) && (rmode == 1) && (smode == 0) )
		mode = 3;	
	if ( (dmode == 1) && (rmode == 0) && (smode == 1) )
		mode = 4;	
	if ( (dmode == 0) && (rmode == 0) && (smode == 1) )
		mode = 5;	
	if ( (dmode == 0) && (rmode == 1) && (smode == 1) )
		mode = 6;	

	if (mode == 0) {
		perror("Mode was not set properly");
		exit(1);
		}
	return mode;		
}

void kmc_query_mode(int fd)
{
	int mode;
	
	mode = kmc_get_mode(fd);

	printf("Mode: ");
	if (mode == 1) 
		printf("DISPLAY\n");
	if (mode == 2) 
		printf("LIVE\n");
	if (mode == 3) 
		printf("RECORD READY\n");
	if (mode == 4) 
		printf("DISPLAY PARAMETERS\n");
	if (mode == 5) 
		printf("RECORD PARAMETERS (NOT RECORD READY)\n");
	if (mode == 6) 
		printf("RECORD PARAMETERS (RECORD READY)\n");
}


void kmc_set_mode_to_DISPLAY(int fd)
{
	int dmode, rmode, smode;

	dmode = kmc_get_display_mode(fd);
	rmode = kmc_get_record_ready(fd);
	smode = kmc_get_setup_mode(fd);

	if ( kmc_get_setup_mode(fd) == 1)
		kmc_press_stop_escape_button(fd);
	if ( kmc_get_record_ready(fd) == 1)
		kmc_press_record_ready_button(fd);
	if ( kmc_get_display_mode(fd) == 0)
		kmc_press_mode_button(fd);

	if ( kmc_get_mode(fd) != 1 ) {
		perror("kmc_set_mode_to_DISPLAY: Did not set mode correctly.");
		exit(0);
		}
}


void kmc_set_mode_to_LIVE(int fd)
{
	int dmode, rmode, smode;

	dmode = kmc_get_display_mode(fd);
	rmode = kmc_get_record_ready(fd);
	smode = kmc_get_setup_mode(fd);

	if ( kmc_get_setup_mode(fd) == 1)
		kmc_press_stop_escape_button(fd);
	if ( kmc_get_record_ready(fd) == 1)
		kmc_press_record_ready_button(fd);
	if ( kmc_get_display_mode(fd) == 1)
		kmc_press_mode_button(fd);

	if ( kmc_get_mode(fd) != 2 ) {
		perror("kmc_set_mode_to_LIVE: Did not set mode correctly.");
		exit(0);
		}
}


void kmc_set_mode_to_RECORD_READY(int fd)
{
	int dmode, rmode, smode;

	dmode = kmc_get_display_mode(fd);
	rmode = kmc_get_record_ready(fd);
	smode = kmc_get_setup_mode(fd);

	if ( kmc_get_setup_mode(fd) == 1)
		kmc_press_stop_escape_button(fd);
	if ( kmc_get_display_mode(fd) == 1)
		kmc_press_mode_button(fd);
	if ( kmc_get_record_ready(fd) == 0)
		kmc_press_record_ready_button(fd);

	if ( kmc_get_mode(fd) != 3 ) {
		perror("kmc_set_mode_to_RECORD_READY: Did not set mode correctly.");
		exit(0);
		}
}

void kmc_set_mode_to_DISPLAY_PARAMETERS(int fd)
{
	int dmode, rmode, smode;

	dmode = kmc_get_display_mode(fd);
	rmode = kmc_get_record_ready(fd);
	smode = kmc_get_setup_mode(fd);

	/* Must leave setup for mode_button to work */
	if ( kmc_get_setup_mode(fd) == 1)
		kmc_press_stop_escape_button(fd);

	if ( kmc_get_record_ready(fd) == 1)
		kmc_press_record_ready_button(fd);
	if ( kmc_get_display_mode(fd) == 0)
		kmc_press_mode_button(fd);
	if ( kmc_get_setup_mode(fd) == 0)
		kmc_press_menu_enter_button(fd);

	if ( kmc_get_mode(fd) != 4 ) {
		perror("kmc_set_mode_to_DISPLAY_PARAMETERS: Did not set mode correctly.");
		exit(0);
		}
}

void kmc_set_mode_to_RECORD_PARAMETERS(int fd)
{
	int dmode, rmode, smode;

	dmode = kmc_get_display_mode(fd);
	rmode = kmc_get_record_ready(fd);
	smode = kmc_get_setup_mode(fd);

	/* Must leave setup for mode_button to work */
	if ( kmc_get_setup_mode(fd) == 1)
		kmc_press_stop_escape_button(fd);

	if ( kmc_get_record_ready(fd) == 1)
		kmc_press_record_ready_button(fd);
	if ( kmc_get_display_mode(fd) == 1)
		kmc_press_mode_button(fd);
	if ( kmc_get_setup_mode(fd) == 0)
		kmc_press_menu_enter_button(fd);

	if ( kmc_get_mode(fd) != 5 ) {
		perror("kmc_set_mode_to_RECORD_PARAMETERS: Did not set mode correctly.");
		exit(0);
		}
}


void kmc_set_mode_to_RECORD_PARAMETERS_RR(int fd)
{
	int dmode, rmode, smode;

	dmode = kmc_get_display_mode(fd);
	rmode = kmc_get_record_ready(fd);
	smode = kmc_get_setup_mode(fd);

	/* Must leave setup for mode_button to work */
	if ( kmc_get_setup_mode(fd) == 1)
		kmc_press_stop_escape_button(fd);

	if ( kmc_get_display_mode(fd) == 1)
		kmc_press_mode_button(fd);
	if ( kmc_get_record_ready(fd) == 0)
		kmc_press_record_ready_button(fd);
	if ( kmc_get_setup_mode(fd) == 0)
		kmc_press_menu_enter_button(fd);

	if ( kmc_get_mode(fd) != 6 ) {
		perror("kmc_set_mode_to_RECORD_PARAMETERS_RR: Did not set mode correctly.");
		exit(0);
		}
}


void kmc_set_shutter_speed(int fd, int shutter_speed)
{

	printf("Sorry.  Shutter speed setting is not yet coded.\n");

}


void kmc_set_scsi_id(int fd, int scsiid)
{

	printf("Sorry.  SCSI ID setting is not yet coded.\n");

}


void kmc_set_display_size(int fd, char *new_displaysize)
{
	int i, relative_position;
	int	new_position=-1;
	int	prev_position=-1;
	char prev_displaysize[20];
	char final_displaysize[20];

	//printf("Setting display size to %s\n\n",new_displaysize);

	/* 
	 * Get previous display size.
	 */
	kmc_get_display_size(fd, prev_displaysize);
	printf("Old display size: %s\n",prev_displaysize);


	/* 
	 * Make sure the DISPLAY SIZE string is a valid option.
	 * Also find position of old and new values in list.
	 */
	for (i=0; i<N_DISPLAY_SIZES; i++) {
		if ( strncmp(new_displaysize,DISPLAY_SIZES[i],strlen(new_displaysize)) == 0 ) 
			new_position=i;
		if ( strncmp(prev_displaysize,DISPLAY_SIZES[i],strlen(prev_displaysize)) == 0 ) 
			prev_position=i;
		}

	if ( new_position == -1 ) {
		printf("Error: Invalid display size: %s\nValid display sizes: ",new_displaysize);
		for (i=0; i<N_DISPLAY_SIZES; i++) 
			printf("%s ",DISPLAY_SIZES[i]);
		printf("\n");
		exit(0);
		}	

	relative_position = -(new_position-prev_position);	/* order is reversed */

	/*
	printf("Previous position: %d\n",prev_position);
	printf("New position:      %d\n",new_position);
	printf("Previous display size: %s\n",DISPLAY_SIZES[prev_position]);
	printf("New display size:      %s\n",DISPLAY_SIZES[new_position]);
	printf("Relative position:    %d\n",relative_position);
	*/


	if (relative_position != 0) {
		/*
		 * Go to Display Size screen
		 */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */

		kmc_set_mode_to_RECORD_PARAMETERS(fd);
		kmc_press_down_button(fd);
		kmc_press_down_button(fd);
		kmc_press_menu_enter_button(fd);

		/*
		 * Select correct setting
		 */
		kmc_move_in_list(fd,relative_position);	
		kmc_press_menu_enter_button(fd);
		kmc_press_stop_escape_button(fd);

		/*
		 * Make sure the new Display Size is correct
		 */
		kmc_longsleep();
		kmc_get_display_size(fd,final_displaysize);
		printf("New display size: %s\n",final_displaysize);
		if ( strncmp(new_displaysize,final_displaysize,6) != 0 ) {
			printf("Error: Display size was not changed properly.\n");
			exit(0);
		}
	}
}


void kmc_set_trigger_mode(int fd, char *new_triggermode)
{
	int i, relative_position;
	int	new_position=-1;
	int	prev_position=-1;
	char prev_triggermode[20];
	char final_triggermode[20];

	/* 
	 * Get previous trigger mode.
	 */
	kmc_get_trigger_mode(fd,prev_triggermode);
	//printf("Trigger mode: %s\n",prev_triggermode);

	/* 
	 * Make sure the "new_triggermode" string is a valid option.
	 * Also find position of old and new values in list.
	 */
	for (i=0; i<N_TRIGGER_MODES; i++) {
		if ( strncasecmp(new_triggermode,TRIGGER_MODES[i],strlen(new_triggermode)) == 0 ) 
			new_position=i;
		if ( strncasecmp(prev_triggermode,TRIGGER_MODES[i],strlen(prev_triggermode)) == 0 ) 
			prev_position=i;
		}

	if ( new_position == -1 ) {
		printf("Error: Invalid trigger mode: %s\nValid trigger modes: ",new_triggermode);
		for (i=0; i<N_TRIGGER_MODES; i++) 
			printf("%s ",TRIGGER_MODES[i]);
		printf("\n");
		exit(0);
		}	

	relative_position = new_position-prev_position;	/* order is reversed */
	//printf("relative position: %d\n",relative_position);

	if (relative_position != 0) {
		/*
		 * Go to Trigger Mode screen
		 */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */

		kmc_set_mode_to_RECORD_PARAMETERS(fd);
		kmc_press_menu_enter_button(fd);

		/*
		 * Select correct setting
		 */
		kmc_move_in_list(fd,relative_position);	
		kmc_press_menu_enter_button(fd);
		if ( strncasecmp(new_triggermode,"RANDOM",strlen(new_triggermode)) == 0 ) {
			/*
			 * For random trigger, be careful not to change the burst mode
			 */
			kmc_press_menu_enter_button(fd);	
			kmc_press_menu_enter_button(fd);
		}
		kmc_press_stop_escape_button(fd);

		/*
		 * Make sure the new Trigger Mode is correct
		 */
		kmc_longsleep();
		kmc_get_trigger_mode(fd,final_triggermode);
		printf("New Trigger Mode: %s\n",final_triggermode);
		if ( strncasecmp(new_triggermode,final_triggermode,3) != 0 ) {
			printf("Error: Trigger Mode was not changed properly.\n");
			exit(0);
		}
	}
}


void kmc_set_record_rate(int fd, int fps)
{
	int i, prev_fps, new_fps;
	int	new_position=-1;
	int	prev_position=-1;
	int relative_position;

	/* 
	 * Get previous record rate.
	 */
	prev_fps = kmc_get_record_rate(fd);

	printf("Previous record rate: %d\n",prev_fps);


	/* 
	 * Make sure the FPS value is a valid option 
	 * Also find position of old and new value in list.
	 */
	for (i=0; i<N_RECORD_RATES ; i++) {
		if ( RECORD_RATES_MENU[i] == fps ) 
			new_position=i;
		if ( RECORD_RATES_MENU[i] == prev_fps ) 
			prev_position=i;
		}
	if ( new_position == -1 ) {
		printf("Error: Invalid record rate: %d\nValid record rates: ",fps);
		for (i=0; i<N_RECORD_RATES ; i++) 
			printf("%d ",RECORD_RATES_MENU[i]);
		printf("\n");
		exit(0);
		}	

	relative_position = new_position-prev_position;

	/*
	printf("Setting record rate to: %d\n",fps);
	printf("Starting at position: %d\n",prev_position);
	printf("Going to position:    %d\n",new_position);
	printf("Relative position:    %d\n",relative_position);
	*/

	if (relative_position != 0) {
		/*
		 * Go to Record Frame Rate screen
		 */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */
		kmc_press_stop_escape_button(fd);	/* In case already in setup */
		kmc_set_mode_to_RECORD_PARAMETERS(fd);
		kmc_press_down_button(fd);
		kmc_press_menu_enter_button(fd);

		/*
		 * Select correct setting
		 */
		kmc_move_in_list(fd,relative_position);	
		kmc_press_menu_enter_button(fd);
		kmc_press_stop_escape_button(fd);
		kmc_clear_serial_buffer(fd);

		/*
		 * Make sure the new Record Rate is correct
		 */
		kmc_longsleep();
		new_fps = kmc_get_record_rate(fd);
		printf("New frame rate: %d\n",new_fps);
		if (new_fps != fps) {
			printf("Error: Record rate was not changed properly.\n");
			exit(0);
		}
	}
}



void kmc_set_play_rate(int fd, int fps, int palvideo)
{
	int i, prev_playrate, new_playrate;
	int	new_position=-1;
	int	prev_position=-1;
	int relative_position;
	int *display_rates;

	/* 
	 * Set display_rates list depending on PAL or NTSC
	 */
	if (palvideo == 0) display_rates = &DISPLAY_RATES_NTSC;
	if (palvideo == 1) display_rates = &DISPLAY_RATES_PAL;

	/* 
	 * Get previous record rate.
	 */
	prev_playrate = kmc_get_playback_rate(fd, palvideo);

	printf("Previous display rate: %d\n",prev_playrate);

	/* 
	 * Make sure the FPS value is a valid option 
	 * Also find position of old and new value in list.
	 */
	for (i=0; i<N_DISPLAY_RATES ; i++) {
		if ( display_rates[i] == fps ) 
			new_position=i;
		if ( display_rates[i] == prev_playrate) 
			prev_position=i;
		}
	if ( new_position == -1 ) {
		printf("Error: Invalid play rate: %d\nValid play rates: ",fps);
		for (i=0; i<N_DISPLAY_RATES ; i++) 
			printf("%d ",display_rates[i]);
		printf("\n");
		exit(0);
		}	

	relative_position = -(new_position-prev_position);

	if (relative_position != 0) {
		/*
		 * Make sure it is in DISPLAY mode 
		 */
		kmc_set_mode_to_DISPLAY(fd);

		/*
		 * Select correct setting
		 */
		kmc_move_in_list(fd,relative_position);	

		/*
		 * Make sure the new Record Rate is correct
		 */
		new_playrate = kmc_get_playback_rate(fd,palvideo);
		printf("New play rate: %d\n",new_playrate);
		if (new_playrate != fps) {
			printf("Error: Play rate was not changed properly.\n");
			exit(0);
		}
	}
}


/*
 * Press trigger button until mode is LIVE.  Useful for random trigger
 * mode when one must fill the memory with images before transfering any
 * to a PC.
 */
void kmc_trigger_until_live(int fd_kmc)
{
	int	current_mode,i;

	current_mode = kmc_get_mode(fd_kmc);
			/*
			 * Modes:
			 *			1	Display
			 *			2	Live
			 *			3	Record Ready
			 *			4	Display Parameters
			 *			5	Record Parameters (Not RR)
			 *			6	Record Parameters (RR)
			 */
	if (current_mode != 3) printf("Not in REC READY mode.\n");
	if (current_mode == 3) printf("Pressing trigger button until KMC returns to LIVE mode...\n");
	while (kmc_get_mode(fd_kmc) == 3) {
		/*
		 * Press trigger in groups, since kmc_get_mode() is slow 
		 */
		for (i=0; i<NTRIGGERS_PER_GROUP; i++)
			{
			kmc_press_trigger_button(fd_kmc);
            usleep(10000);		/* 100 ms is minimum reliable, but we 
								   can push it harder here. */
		}		
	}
	printf("Done.\n");
}



/*
 * Trigger every trigger_interval milliseconds
 * Note: It appears to always start at frame 0.
 */
int kmc_time_delay_trigger(int fd_kmc, int trigger_interval, int Ntriggers)
{
    long i, t1, t2, total_time;
    float av_time;
    struct tms cur_times;
    long nticks, nticks1, nticks2, nticksdiff;
    float tick_time;
    long ticktimes[Ntriggers];
    long shortest_time=0;
    long longest_time=0;
    long time_int;
    int current_mode=3;
    int previous_mode=3;


    if (trigger_interval < SHORTEST_TRIGGER_INTERVAL) {
        printf("Error: Trigger interval( %d ms ) too short\n",trigger_interval);
    	printf("NOTE: Triggering with a period less than 100 ms is unreliable.\n");
        return 0;
        }
    else {
    	/*
	     * Disclaimer
	     */
	    printf("WARNING: The delay times are not precise and may depend on system...\n");
	    printf("NOTE: The system must be triggered until memory is full and mode becomes LIVE\n");



        /*
         * Make sure it is in RANDOM trigger mode
         */
        kmc_set_trigger_mode(fd_kmc,"random");
        usleep(100000);

        /*
         * Make sure it is in record ready
         */
        kmc_set_mode_to_RECORD_READY(fd_kmc);

        /*
         * Trigger at intervals, recording trigger times
         */
        nticks1 = times(&cur_times);
        t1 = time(0);
        printf("Triggering %d times at intervals of %d ms...\n",Ntriggers,trigger_interval);
        for (i=0; i<Ntriggers; i++){
            usleep(trigger_interval*1000);
            kmc_press_trigger_button(fd_kmc);
            ticktimes[i]=times(&cur_times);
        }


        /*
         * Calculate trigger intervals
         */
        for (i=0; i<(Ntriggers-1); i++){
            time_int = ticktimes[i+1]-ticktimes[i];
            if ( i == 0) shortest_time = time_int;
            if (time_int > longest_time)  longest_time = time_int;
            if (time_int < shortest_time) shortest_time = time_int;
        }

        /*
         * Report on timing results
         */
        nticks2 = times(&cur_times);
        t2 = time(0);

        tick_time = ((float)(nticks2-nticks1))/100.0;
        printf("Total time using clock ticks: %f seconds\n",tick_time);

        total_time = t2-t1;
        printf("Total time using seconds: %d seconds \n",total_time);

        av_time = (float)tick_time/Ntriggers;
        printf("Longest time:  %ld ms \n",longest_time*10);
        printf("Shortest time: %ld ms \n",shortest_time*10);
        printf("Average time:  %.2f ms \n",av_time*1000);

        return 1;
    }
}


void kmc_test()
{
	printf("hi\n");
}

