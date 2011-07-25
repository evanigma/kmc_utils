/* Library functions for communicating with Kodak Motion Corder SR high 
 * speed digital cameras.
 *
 *
 * Dan Mueth - 08/23/99.  Version 0.3.2
 * 
 * Change Log:
 *
 * Version 0.3.2: Basic pgm and ppm for monochrome camera mostly works.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <linux/hdreg.h>
#include <time.h>               /* added for random number */
#include <fcntl.h>
#include <errno.h>
#include <scsi/sg.h>

/* #define SCSI_OFF sizeof(struct sg_header)  */ 
		/* replaced with 36 for now b/c SWIG doesn't like it */



#define SCSI_OFF                36           /* The generic header offset */
#define TESTUNITREADY_CMD       0x00
#define TESTUNITREADY_CMDLEN    6
#define TESTUNITREADY_REPLY_LEN 1
#define TESTUNITREADY_REPLYMASK 62      /* masks off bits 0, 6, and 7 */
#define CHECK_CONDITION         2       /* R R 0 0 0 0 1 R */
#define INQUIRY_CMD             0x12
#define INQUIRY_CMDLEN          6
#define INQUIRY_REPLY_LEN_SHORT 36	
#define INQUIRY_REPLY_LEN       128	
#define INQUIRY_VENDOR          8
#define READ_CMD                0x28
#define READ_CMDLEN             10      /* # of characters in read command */
#define READ_REPLY_LEN          16384   /* Maximum transfer block size */

#define MAX_IM_HEIGHT		480
#define MAX_IM_WIDTH		512

char *kmc_names[] = {"Motion Corder"};
char *devices[] =
{"/dev/sga", "/dev/sgb", "/dev/sgc", "/dev/sgd", "/dev/sge", "/dev/sgf", "/dev/sgg", "/dev/sgh", "/dev/sg0", "/dev/sg1", "/dev/sg2", "/dev/sg3", "/dev/sg4", "/dev/sg5", "/dev/sg6", "/dev/sg7", "/dev/sg8", "/dev/sg9"};


/* Device information variables */
int idcode, dataversion, hsize, vsize, nframes, fps, speed, color, trigger, year, month, date, time_hour, time_minute, time_second, sessionid, balance, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9;
/* Note these must not be unsigned char's */ 
char vendor[8];
char product[16];
char version[4];
char vendorinherent[20];
char reserved[40];

int dev_fd;                                  /* SCSI device/file descriptor */
static unsigned char cmd[SCSI_OFF + 18];     /* SCSI command buffer */
unsigned char buffer[64 * 1024 + 100];
unsigned char buffer1[10 * 1024 + 100];
unsigned char imagedata[MAX_IM_HEIGHT*MAX_IM_WIDTH];/* image data, disordered*/
unsigned char image[MAX_IM_HEIGHT*MAX_IM_WIDTH];    /* image data, in order*/
unsigned char r_image[MAX_IM_HEIGHT*MAX_IM_WIDTH];    /* red image data */
unsigned char g_image[MAX_IM_HEIGHT*MAX_IM_WIDTH];    /* green image data */
unsigned char b_image[MAX_IM_HEIGHT*MAX_IM_WIDTH];    /* blue image data */
//unsigned char imageout[MAX_IM_HEIGHT*MAX_IM_WIDTH*3]; /* RGB for GTK+ Pixmap */


int kmc_open_device(char *device_name){
//printf("Opening device: %s\n",device_name);

  dev_fd = open(device_name, O_RDWR);
    if ( dev_fd < 0 ) {
        perror("kmc_open_device(open)");
        exit(1);
    }

  return dev_fd; 
}



int kmc_close_device(int dev_fd){
  int      retval;

//  printf("Closing device with file descriptor: %d\n",dev_fd); 
  retval = close(dev_fd);
  if (retval < 0) {
     perror("kmc_close_device(close)");
     exit(1);
  }

  return 0; 
}


/* The following(handle_SCSI_cmd) was taken from the Linux 
 * SCSI-Programming-HOWTO for the Linux generic scsi interface. */
/* Process a complete SCSI cmd. Use the generic SCSI interface. */
static int handle_SCSI_cmd(unsigned cmd_len,           /* command length */
                             unsigned in_size,         /* input data size */
                             unsigned char *i_buff,    /* input buffer */
                             unsigned out_size,        /* output data size */
                             unsigned char *o_buff     /* output buffer */
                             )
{
    int status = 0;
    struct sg_header *sg_hd;

    /* safety checks */
    if (!cmd_len) return -1;            /* need a cmd_len != 0 */
    if (!i_buff) return -1;             /* need an input buffer != NULL */
#ifdef SG_BIG_BUFF
    if (SCSI_OFF + cmd_len + in_size > SG_BIG_BUFF) return -1;
    if (SCSI_OFF + out_size > SG_BIG_BUFF) return -1;
#else
    if (SCSI_OFF + cmd_len + in_size > 4096) return -1;
    if (SCSI_OFF + out_size > 4096) return -1;
#endif

    if (!o_buff) out_size = 0;      /* no output buffer, no output size */

    /* generic SCSI device header construction */
    sg_hd = (struct sg_header *) i_buff;
    sg_hd->reply_len   = SCSI_OFF + out_size;
    sg_hd->twelve_byte = cmd_len == 12;
    sg_hd->result = 0;
#if     0
    sg_hd->pack_len    = SCSI_OFF + cmd_len + in_size; /* not necessary */
    sg_hd->pack_id;     /* not used */
    sg_hd->other_flags; /* not used */
#endif

    /* send command */
    status = write( dev_fd, i_buff, SCSI_OFF + cmd_len + in_size );
    if ( status < 0 || status != SCSI_OFF + cmd_len + in_size ||
                       sg_hd->result ) {
        /* some error happened */
        fprintf( stderr, "write(generic) result = 0x%x cmd = 0x%x\n",
                    sg_hd->result, i_buff[SCSI_OFF] );
        perror("");
        return status;
    }

    if (!o_buff) o_buff = i_buff;       /* buffer pointer check */

    /* retrieve result */
    status = read( dev_fd, o_buff, SCSI_OFF + out_size);

    if ( status < 0 || status != SCSI_OFF + out_size || sg_hd->result ) {
        /* some error happened */
        fprintf( stderr, "read(generic) status = 0x%x, result = 0x%x, "
                         "cmd = 0x%x\n",
                         status, sg_hd->result, o_buff[SCSI_OFF] );
        fprintf( stderr, "read(generic) sense "
                "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
                sg_hd->sense_buffer[0],         sg_hd->sense_buffer[1],
                sg_hd->sense_buffer[2],         sg_hd->sense_buffer[3],
                sg_hd->sense_buffer[4],         sg_hd->sense_buffer[5],
                sg_hd->sense_buffer[6],         sg_hd->sense_buffer[7],
                sg_hd->sense_buffer[8],         sg_hd->sense_buffer[9],
                sg_hd->sense_buffer[10],        sg_hd->sense_buffer[11],
                sg_hd->sense_buffer[12],        sg_hd->sense_buffer[13],
                sg_hd->sense_buffer[14],        sg_hd->sense_buffer[15]);
        if (status < 0)
            perror("");
    }

    /* Look if we got what we expected to get */
    if (status == SCSI_OFF + out_size) status = 0; /* got them all */

    return status;  /* 0 means no error */
}


/* Send a TEST UNIT READY and read response */
int get_device_status(void)
{
    unsigned char Statusbuffer[ SCSI_OFF + TESTUNITREADY_REPLY_LEN ];
    unsigned char cmdblk [ TESTUNITREADY_CMDLEN ] =
	     {  TESTUNITREADY_CMD,  /* command */
       		             	0,  /* lun/reserved */
               		     	0,  /* page code */
                    		0,  /* reserved */
     		    		0,  /* allocation length */
                    		0 };/* reserved/flag/link */

    memcpy( cmd + SCSI_OFF, cmdblk, sizeof(cmdblk) );

    if (handle_SCSI_cmd(sizeof(cmdblk), 0, cmd,
                        sizeof(Statusbuffer) - SCSI_OFF, Statusbuffer )) {
        fprintf( stderr, "Status check failed\n" );
        exit(2);
    }


    /* Mask off reserved bits */
    Statusbuffer[0] = Statusbuffer[0] & TESTUNITREADY_REPLYMASK;

    /* printf("Status: (hex):%x (decimal):%d\n",Statusbuffer[0],Statusbuffer[0]); */

    if( Statusbuffer[0] & CHECK_CONDITION) {
		printf("Check Condition\n");
		return 0;
		}
    else return 1;
}



/* Get device information using read() and write() */
int get_device_info(void)
{
    unsigned char *pagestart;
    unsigned char tmp;
    unsigned char Inqbuffer[ SCSI_OFF + INQUIRY_REPLY_LEN ];
    unsigned char cmdblk [ INQUIRY_CMDLEN ] =
        { INQUIRY_CMD,  /* command */
                    0,  /* lun/reserved */
                    0,  /* page code */
                    0,  /* reserved */
    INQUIRY_REPLY_LEN,  /* allocation length */
                    0 };/* reserved/flag/link */

    memcpy( cmd + SCSI_OFF, cmdblk, sizeof(cmdblk) );

    /*
     * +------------------+
     * | struct sg_header | <- cmd
     * +------------------+
     * | copy of cmdblk   | <- cmd + SCSI_OFF
     * +------------------+
     */


    if (handle_SCSI_cmd(sizeof(cmdblk), 0, cmd,
                        sizeof(Inqbuffer) - SCSI_OFF, Inqbuffer )) {
        fprintf( stderr, "Inquiry failed\n" );
        exit(2);
    }

    pagestart = Inqbuffer + SCSI_OFF;

    tmp = pagestart[16];
    pagestart[16] = 0;
    sprintf(vendor,"%s",pagestart+8);
//    printf("...............vendor: %s\n",vendor);
    pagestart[16] = tmp;
    

    tmp = pagestart[32];
    pagestart[32] = 0;
    sprintf(product,"%s",pagestart+16);
//    printf("...............product: %s\n",product);
    pagestart[32] = tmp;

    tmp = pagestart[36];
    pagestart[36] = 0;
    sprintf(version,"%s",pagestart+32);
//    printf("...............version: %s\n",version);
    pagestart[36] = tmp;

    tmp = pagestart[56];
    pagestart[56] = 0;
    sprintf(vendorinherent,"%s",pagestart+36);
//    printf("...............vendorinherent: %s\n",vendorinherent);
    pagestart[56] = tmp;

    tmp = pagestart[96];
    pagestart[96] = 0;
    sprintf(reserved,"%s",pagestart+56);
    pagestart[96] = tmp;

    idcode = pagestart[96];
    dataversion = pagestart[97];
    hsize = pagestart[98]*256+pagestart[99];
    vsize = pagestart[100]*256+pagestart[101];
    nframes = pagestart[102]*256*256 + pagestart[103]*256+pagestart[104];
    fps = pagestart[105]*256+pagestart[106];
    speed = pagestart[107]*256+pagestart[108];
    trigger = (pagestart[109] & 0xf0) >> 4;
    color = pagestart[109] & 0x0f;
    year = pagestart[110]*100 + pagestart[111];
    month = pagestart[112];
    date = pagestart[113];
    time_hour = pagestart[114];
    time_minute = pagestart[115];
    time_second = pagestart[116];
    sessionid = pagestart[117];
    balance = pagestart[118];


    return 0;

}


/* Returns 1 for "color" or 0 for "monochrome" for camera type */
/* For external library use */
int kmc_is_color()
{
    get_device_info();
    return color;
}


/* Returns 1 if the device is a kmc, or 0 otherwise */
/* For external library use */
int kmc_is_kmc()
{
    int isakmc;

    get_device_info();
    isakmc = is_a_kmc();
    return isakmc;
}


/* Returns the trigger value.  
 *	0 ==> Start Trigger
 *	1 ==> Center Trigger
 *	2 ==> Stop Trigger						*/
int kmc_get_triggerval()
{
    get_device_info();

    /* The following is for TESTING and should be REMOVED. */
//    printf("trigger: %d\n",trigger);
//    trigger = 1;
//    printf("changed trigger to: %d\n",trigger);
    /* End region to be REMOVED. */

    return trigger;
}


/* Returns the number of frames stored in camera */
int kmc_get_nframes()
{
    get_device_info();

    /* The following is for TESTING and should be REMOVED. */
//    printf("nframes: %d\n",trigger);
//    nframes = 100;
//    printf("changed nframes to : %d\n",nframes);
    /* End region to be REMOVED. */

    return nframes;
}


/* Outputs full scsi device information.
 * If <filename> is empty, prints long query to STDOUT.
 * If <filename> is not empty, writes long query to file <filename>.	*/
int kmc_long_query(char *filename)
{
    int		filenamelength;
    FILE	*fp;
    int		status = 0;

    /* Read device and set global variables for device properties */
    get_device_info();

    /* Determine destination, open file if necessary, and set file pointer fp */
    filenamelength = strlen(filename);
    if (filenamelength != 0) {
	fp = fopen(filename,"w");
	if (fp < 0) {
	    perror("kmc_long_query(fopen)");
	    exit(1);
            }
	}
    else fp = stdout;

    /* Print long query to file or stdout */
    fprintf(fp,"Vendor:                    %.8s\n",vendor);
    fprintf(fp,"Product:                   %.16s\n",product);
    fprintf(fp,"Version:                   %.4s\n",version);
    fprintf(fp,"Vendor Inherent:           %.20s\n",vendorinherent);
    fprintf(fp,"Reserved:                  %.40s\n",reserved);
    fprintf(fp,"ID Code:                   %d\n",idcode);
    fprintf(fp,"Data Version:              %d\n",dataversion);
    fprintf(fp,"Frame X Size:              %d\n",hsize);
    fprintf(fp,"Frame Y Size:              %d\n",vsize);
    fprintf(fp,"Total Number of Frames:    %d\n",nframes);
    fprintf(fp,"Frame Rate(fps):           %d\n",fps);
    fprintf(fp,"Shutter Speed:             %d\n",speed);
    fprintf(fp,"Trigger Flag:              %d\n",trigger);
    fprintf(fp,"Color Flag:                %d\n",color);
    fprintf(fp,"Year:                      %d\n",year);
    fprintf(fp,"Month:                     %d\n",month);
    fprintf(fp,"Date:                      %d\n",date);
    fprintf(fp,"Hour:                      %d\n",time_hour);
    fprintf(fp,"Minute:                    %d\n",time_minute);
    fprintf(fp,"Second:                    %d\n",time_second);
    fprintf(fp,"Session ID:                %d\n",sessionid);
    fprintf(fp,"White Balance:             %d\n",balance);

    /* Close file <filename> if <filename> was specified */ 
    if (filenamelength != 0) {
	status = fclose(fp);
        if ( status < 0 ) {
            perror("kmc:write_gray_image(fclose)");
            exit(1);
            }
        }

    return 0;
}


int kmc_short_query()
{
//    int status, i;
//    unsigned char *cmd;
    unsigned char *pagestart;
    unsigned char tmp;

    unsigned char Inqbuffer[ SCSI_OFF + INQUIRY_REPLY_LEN_SHORT ];
    unsigned char cmdblk [ INQUIRY_CMDLEN ] =
        { INQUIRY_CMD,  /* command */
                    0,  /* lun/reserved */
                    0,  /* page code */
                    0,  /* reserved */
    INQUIRY_REPLY_LEN_SHORT,  /* allocation length */
                    0 };/* reserved/flag/link */

    memcpy( cmd + SCSI_OFF, cmdblk, sizeof(cmdblk) );

    /*
     * +------------------+
     * | struct sg_header | <- cmd
     * +------------------+
     * | copy of cmdblk   | <- cmd + SCSI_OFF
     * +------------------+
     */


    if (handle_SCSI_cmd(sizeof(cmdblk), 0, cmd,
                        sizeof(Inqbuffer) - SCSI_OFF, Inqbuffer )) {
        fprintf( stderr, "Inquiry failed\n" );
        exit(2);
    }

    pagestart = Inqbuffer + SCSI_OFF;

    tmp = pagestart[16];
    pagestart[16] = 0;
    sprintf(vendor,"%s",pagestart+8);
    pagestart[16] = tmp;
    
    tmp = pagestart[32];
    pagestart[32] = 0;
    sprintf(product,"%s",pagestart+16);
    pagestart[32] = tmp;

    printf("Vendor: %s\n",vendor);
    printf("Product: %s\n",product);


    pagestart = buffer + 8;

    return 0;
}


int make_image_from_color_imagedata()
 {

/* The pixels in a color CCD are arranged with one color per pixel as:
 *  B      G      B      G      B      G       B     .    B     G      
 *  G      R      G      R      G      R       G     .    G     R
 *  B      G      B      G      B      G       B     .    B     G
 *  G      R      G      R      G      R       G     .    G     R    
 *  .      .      .      .      .      .       .     .    .     .            */

/* Note: 
 *	There are twice as many green elements as red or blue.  Thus, a 
 *  "true" image (no interpolation or manipulation) will look green.         */

/* The CCD pixels are read out in the order:
 *  0      1      4      5      8      .      2X-4      2X-3
 *  2      3      6      7      10     .      2X-2      2X-1
 *  2X   2X+1   2X+4    2X+5   2X+8    .      4X-4      4X-3
 *  2X+2 2X+3   2X+6    2X+7   2X+10   .      4X-2      4X-1
 *  4X   4X+1   4X+4    4X+5   4X+8    .      6X-4      6X-3
 *  .      .      .      .      .      .       .         .                   */

  int 		i,j;
  long		rindex, windex;

	/* Re-order pixels from CCD readout order to cartesian and assign
	 * each pixel to the appropriate component color image.                  */	
	/* Note: this can be sped up by looping by 1/2x1/2 and removing if's */
	for ( j=0; j<vsize; j++ ) {
		for ( i=0; i<hsize; i++ ) {
			rindex = (hsize*(j-(j%2)) + (2*i) - (i%2) + (2*(j%2)));
			windex = i+(hsize*j);
			//	printf("i: %d     j: %d    index: %d\n",i,j,rindex); 
			// image[i+hsize*j] = imagedata[rindex];
			if ( (rindex%4) == 0 ) b_image[windex] = imagedata[rindex];	
			if ( (rindex%4) == 1 ) g_image[windex] = imagedata[rindex];	
			if ( (rindex%4) == 2 ) g_image[windex] = imagedata[rindex];	
			if ( (rindex%4) == 3 ) r_image[windex] = imagedata[rindex];	
			}
		}

   return 0;
 }


int make_image_from_tiff_imagedata()
{
  printf("make_image_from_color_imagedata(): does not exist yet\n");
  return 0;
}


int make_image_from_gray_imagedata()
{

/* The CCD pixels are read out in the order:
 *  0      1      4      5      8      .      2X-4      2X-3
 *  2      3      6      7      10     .      2X-2      2X-1
 *  2X   2X+1   2X+4    2X+5   2X+8    .      4X-4      4X-3
 *  2X+2 2X+3   2X+6    2X+7   2X+10   .      4X-2      4X-1
 *  4X   4X+1   4X+4    4X+5   4X+8    .      6X-4      6X-3
 *  .      .      .      .      .      .       .         .		*/

   int 		i,j;
   long		index;

	/* Re-order pixels from CCD readout order to cartesian */	
	for ( j=0; j<vsize; j++ ) {
		for ( i=0; i<hsize; i++ ) {
			index = (hsize*(j-(j%2)) + (2*i) - (i%2) + (2*(j%2)));
			/* printf("i: %d     j: %d    index: %d\n",i,j,index); */
			image[i+hsize*j] = imagedata[index];
			}
		}
	return 0;
}


/* Make a fake frame.  Useful for testing purposes. */
/* This function can be a stand-in for get_frame during testing. */
int make_fake_frame(int height, int width, int colorflag)
{
    int		i,pixval;

    /* Set global variables which would normally be set by reading device */
    hsize = width;
    vsize = height;
    color = colorflag;

    /* this will force it to always return the same image */
//    srand(1);

    pixval = rand() % 255;
    for (i=0; i<MAX_IM_HEIGHT*MAX_IM_WIDTH; i++) {
	imagedata[i] = (pixval+i) % 255;
	}
}


int get_tiff_frame(int frame)
{
  printf("get_tiff_frame does not yet exist.\n");
  return 0;
}


int get_frame(int frame)
{
    int 		nblocks;
    int         	blocksize = READ_REPLY_LEN;
    int 		blocknumber = 0;
    unsigned char 	Readbuffer[ SCSI_OFF + READ_REPLY_LEN ];
    unsigned char 	cmdblk [ READ_CMDLEN ] =
        { READ_CMD,     	/* command */
                    0,  	/* lun/reserved */
		    0,  	/* Frame No. (1st byte) */
		    0,  	/* Frame No. (2nd byte) */
		    0,  	/* Frame No. (3rd byte) */
                    0,  	/* Transfer Block No. */
                    0,  	/* reserved */
                    0,  	/* Transfer Data Length (1st byte) */
                    0,  	/* Transfer Data Length (2nd byte) */
                    0 };	/* reserved/flag/link */


    /* Set the Frame No. */
    cmdblk[2] = (frame & 0x00ff0000) >> 16;
    cmdblk[3] = (frame & 0x0000ff00) >> 8;
    cmdblk[4] = (frame & 0x000000ff);		
 
/*    printf("cmdblk[2]:  (hex)%x      (decimal)%d\n",cmdblk[2],cmdblk[2]);
    printf("cmdblk[3]:  (hex)%x      (decimal)%d\n",cmdblk[3],cmdblk[3]);
    printf("cmdblk[4]:  (hex)%x      (decimal)%d\n",cmdblk[4],cmdblk[4]); 

    printf("value: %d\n",(long)cmdblk[2]*256*256 + (long)cmdblk[3]*256 + cmdblk[4]);
*/

    /* Set the Transfer Data Length */
    cmdblk[7] = (blocksize & 0xff00) >> 8;
    cmdblk[8] = (blocksize & 0x00ff);

/*  printf("cmdblk[7]:  (hex)%x      (decimal)%d\n",cmdblk[7],cmdblk[7]);
    printf("cmdblk[8]:  (hex)%x      (decimal)%d\n",cmdblk[8],cmdblk[8]);
    printf("value: %d\n",(long)cmdblk[7]*256 + cmdblk[8]); */

    /* Get the image height and width */
    get_device_info();

 /* printf("frames sizes is %d by %d\n",hsize,vsize);
    printf("reading frame %d\n",frame);		*/

    nblocks = 1+hsize*vsize/blocksize;

    for (blocknumber = 0; blocknumber<nblocks; blocknumber++) {
        if (blocknumber != nblocks-1) {		/* Tranfer Full Sized Block */
 /* 	  printf("\nWill get block number: %d\n",blocknumber);
          printf("Will get full sized block: %d\n",blocksize); 
	  printf("Will write to position: %d\n",blocksize*(blocknumber)); */

	  cmdblk[5] = blocknumber;
	  memcpy( cmd + SCSI_OFF, cmdblk, sizeof(cmdblk) );


          if (handle_SCSI_cmd(sizeof(cmdblk), 0, cmd,
                        sizeof(Readbuffer) - SCSI_OFF, Readbuffer )) {
              fprintf( stderr, "Read failed\n" );
              exit(2);
              }

	  memcpy( imagedata+(blocknumber*blocksize), Readbuffer+SCSI_OFF, blocksize);

           }
	else {			/* Transfer Partial Sized Block */	
	   blocksize = hsize*vsize-blocksize*(nblocks-1);
	   if (blocksize != 0) {
	/* 	printf("get partial sized block: %d\n",blocksize);
	        printf("will write to position: %d\n",blocksize*(blocknumber)); */

    		cmdblk[7] = (blocksize & 0xff00) >> 8;
    		cmdblk[8] = (blocksize & 0x00ff);

	  	cmdblk[5] = blocknumber;
	  	memcpy( cmd + SCSI_OFF, cmdblk, sizeof(cmdblk) );

          	if (handle_SCSI_cmd(sizeof(cmdblk), 0, cmd,
                        sizeof(Readbuffer) - SCSI_OFF, Readbuffer )) {
              	fprintf( stderr, "Read failed\n" );
              	exit(2);
              	}
	  	memcpy( imagedata+((blocknumber)*READ_REPLY_LEN), Readbuffer+SCSI_OFF, blocksize);

		}
           }
	}
 
  return 0;
}


int write_tiff(char *filename)
{
  printf("write_as_tiff(): does not exist yet\n");
  return 0;
}


int write_color_image(char *filename, char *format)

{
  int           status = 0;
  unsigned char header[20];
  int 		filenamelength, i, j;
  int 		fd;                /* file descriptor for output image file */
  long		index;
  int           color;
  unsigned char m[1];

  filenamelength = strlen(filename);

  /* Open file (if output is to a file) */
  if (filenamelength != 0) {
	fd = creat(filename,0644);
	if ( fd < 0 ) {
            perror("kmc:write_color_image(creat)");
            exit(1);
            }
	}

  /* Create header */
  if (strncmp(format,"pgm",3) == 0) sprintf(header, "P5\n%d %d\n255\n", hsize, vsize);
  if (strncmp(format,"ppm",3) == 0) sprintf(header, "P6\n%d %d\n255\n", hsize, vsize);

  /* Write header (to file or STDOUT) */
  if (filenamelength == 0) printf("%s",header);
  if (filenamelength != 0) { 
	status = write( fd, &header[0], strlen(header) );
	if ( status < 0 || status != strlen(header) ) {
            /* some error happened */
            perror("kmc:write_color_image(write)");
            return status;
            }
	}

  /* Write image (to file or STDOUT) */
  /* PGM output */
  /* This section is wrong */
  if (strncmp(format,"pgm",3) == 0) {
    /* Make gray image from color image */
    if (filenamelength != 0) printf("Warning: Converting color images to gray must be tested!\n");
    for ( j=0; j<vsize; j++ ) {
      for ( i=0; i<hsize; i++ ) {
        index = i+j*hsize;
        image[index] = (r_image[index]+g_image[index]+b_image[index])/3;
        }
      }

	/* to file */
	if (filenamelength != 0){
	    status = write( fd, image, hsize*vsize );
   	    if ( status != hsize*vsize ) {
        	/* some error happened */
        	perror("write_color_image(write)");
        	return status;
        	}
	    }

	/* to STDOUT */
	if (filenamelength == 0){
            for (i=0; i<hsize*vsize; i++){
                printf("%c",image[i]);
                }
	    }
	}
  
  /* PPM output */
  if (strncmp(format,"ppm",3) == 0) {
    /* to file */
	/* This can be sped up too after it is well tested */
    if (filenamelength != 0){
      for ( j=0; j<vsize; j++ ) {
        for ( i=0; i<hsize; i++ ) {
          index = i+j*hsize;
          status = write( fd, r_image+index, 1 );
          status = write( fd, g_image+index, 1 );
          status = write( fd, b_image+index, 1 );
          if ( status != 1 ) {
            /* some error happened */
            perror("write_color_image(write)");
        	return status;
            }
          }
        }
      }
	/* to STDOUT */
    if (filenamelength == 0){
      for (i=0; i<hsize*vsize; i++){
        printf("%c%c%c",r_image[i],g_image[i],b_image[i]);
        }
      }
    }
  
  /* Close file (if output is to a file) */
  if (filenamelength != 0) {
	status = close(fd);	
	if ( status < 0 ) {
            perror("kmc:write_color_image(close)");
            exit(1);
            }
	}
  return 0;
}


int write_gray_image(char *filename, char *format)
{
  int           status = 0;
  unsigned char header[20];
  int 		filenamelength, i, j;
  int 		fd;                /* file descriptor for output image file */
  

  filenamelength = strlen(filename);

  /* Open file (if output is to a file) */
  if (filenamelength != 0) {
	fd = creat(filename,0644);
	if ( fd < 0 ) {
            perror("kmc:write_gray_image(creat)");
            exit(1);
            }
	}

  /* Create header */
  if (strncmp(format,"pgm",3) == 0) sprintf(header, "P5\n%d %d\n255\n", hsize, vsize);
  if (strncmp(format,"ppm",3) == 0) sprintf(header, "P6\n%d %d\n255\n", hsize, vsize);

  /* Write header (to file or STDOUT) */
  if ((filenamelength == 0)&(filename!="\0")) printf("%s",header);
  if (filenamelength != 0) { 
	status = write( fd, &header[0], strlen(header) );
	if ( status < 0 || status != strlen(header) ) {
            /* some error happened */
            perror("kmc:write_gray_image(write)");
            return status;
            }
	}

  /* Write image (to file or STDOUT) */
  /* PGM output */
  if (strncmp(format,"pgm",3) == 0) {
	/* to file */
	if (filenamelength != 0){
	    status = write( fd, image, hsize*vsize );
   	    if ( status != hsize*vsize ) {
        	/* some error happened */
        	perror("write_gray_image(write)");
        	return status;
        	}
	    }

	/* to STDOUT */
    if ((filenamelength == 0)&(filename!="\0")){
            for (i=0; i<hsize*vsize; i++){
                printf("%c",image[i]);
                }
	    }
	}
  
  /* PPM output */
  if (strncmp(format,"ppm",3) == 0) {
	/* to file */
	if (filenamelength != 0){
            for (i=0; i<hsize*vsize; i++){
            	for (j=0; j<3; j++){
                      
		  status = write(fd, image+i, 1);
   	    	    if ( status != 1 ) {
        		/* some error happened */
			printf("i: %d  j: %d\n",i,j);
			printf("status: %d\n",status);
        		perror("write_gray_image(write)");
        		return status;
        	    	}
		    }
                }
	    }


	/* to STDOUT */
    if ((filenamelength == 0)&(filename!="\0")){
            for (i=0; i<hsize*vsize; i++){
                printf("%c%c%c\n",image[i],image[i],image[i]);
                }
	    }
	}
  


  /* Close file (if output is to a file) */
  if (filenamelength != 0) {
	status = close(fd);	
	if ( status < 0 ) {
            perror("kmc:write_gray_image(close)");
            exit(1);
            }
	}

  return 0;
}


int interp_color_image(){

  int	i,j; 
  long	index;


  /* Note: char's are automatically promoted to int's for math */

  /* Interpolation away from right and bottom edges (blue and some green)*/
  for ( i=0; i<((hsize/2)-1); i++ ) {			/* looping over 4-somes */
    for ( j=0; j<((vsize/2)-1); j++ ) {
      index = (2*i)+(2*j*hsize);
	  /* Blue: Position #2 */
      b_image[index+1] = (b_image[index]+b_image[index+2])/2.0;
      /* Blue: Position #3 */
      b_image[index+hsize] = (b_image[index]+b_image[index+(2*hsize)])/2.0; 
      /* Blue: Position #4 */
      b_image[index+hsize+1] = (b_image[index]+b_image[index+2]+b_image[index+(2*hsize)]+b_image[index+(2*hsize)+2])/4.0;
      /* Green: Position #4 */
      g_image[index+1+hsize] = (g_image[index+1]+g_image[index+hsize]+g_image[index+hsize+2]+g_image[index+1+(2*hsize)])/4.0;
      }
    }

  /* Interpolation away from left and top edges (red and some green) */
  for ( i=1; i<(hsize/2); i++ ) {			/* looping over 4-somes */
    for ( j=1; j<(vsize/2); j++ ) {
      index = (2*i)+(2*j*hsize);
      /* Red: Position #1 */
//      r_image[index] = (r_image[index-hsize]+r_image[index-1]+r_image[index+1]+r_image[index+hsize])/4.0;
      r_image[index] = (r_image[index-hsize-1]+r_image[index-hsize+1]+r_image[index+hsize-1]+r_image[index+hsize+1])/4.0;
      /* Red: Position #2 */
      r_image[index+1] = (r_image[index+1-hsize]+r_image[index+1+hsize])/2.0;
      /* Red: Position #3 */
      r_image[index+hsize] = (r_image[index+hsize-1]+r_image[index+hsize+1])/2.0;
      /* Green: Position #1 */
      g_image[index] = (g_image[index-hsize]+g_image[index-1]+g_image[index+1]+g_image[index+hsize])/4.0;
      }
    }

  /* Interpolate along right edge (except corners) */
  for ( j=1; j<((vsize/2)-1); j++ ) {               /* looping over 4-somes */
    index = (hsize-2)+(2*j*hsize);
    /* Blue: Position #2 */
    b_image[index+1] = b_image[index];
    /* Blue: Position #3 */
    b_image[index+hsize] = (b_image[index]+b_image[index+(2*hsize)])/2.0;     
    /* Blue: Position #4 */
    b_image[index+1+hsize] = (b_image[index]+b_image[index+(2*hsize)])/2.0;
    /* Green: Position #4 */
	g_image[index+1+hsize] = (g_image[index+1]+g_image[index+hsize]+g_image[index+1+(2*hsize)])/3.0;
    }

  /* Interpolate along left edge (except corners) */
  for ( j=1; j<((vsize/2)-1); j++ ) {               /* looping over 4-somes */
    index = 2*j*hsize;
    /* Red: Position #1 */
	r_image[index] = (r_image[index-hsize+1]+r_image[index+hsize+1])/2.0;
    /* Red: Position #2 */
	r_image[index+1] = (r_image[index-hsize+1]+r_image[index+hsize+1])/2.0;
    /* Red: Position #3 */
	r_image[index+hsize] = r_image[index+hsize+1];
	/* Green: Position #1 */
	g_image[index] = (g_image[index-hsize]+g_image[index+1]+g_image[index+hsize])/3.0;
	}

  /* Interpolate along bottom edge (except corners) */
  for ( i=1; i<((hsize/2)-1); i++ ) {               /* looping over 4-somes */
    index = (2*i)+((vsize-2)*hsize);
    /* Blue: Position #2 */
    b_image[index+1] = (b_image[index]+b_image[index+2])/2.0;
    /* Blue: Position #3 */
    b_image[index+hsize] = b_image[index];
    /* Blue: Position #4 */
    b_image[index+1+hsize] = (b_image[index]+b_image[index+2])/2.0;
	/* Green: Position #4 */
	g_image[index+1+hsize] = (g_image[index+1]+g_image[index+hsize]+g_image[index+hsize+2])/3.0;
    }

  /* Interpolate along top edge (except corners) */
  for ( i=1; i<((hsize/2)-1); i++ ) {               /* looping over 4-somes */
    index = (2*i);
    /* Red: Position #1 */
	r_image[index] = (r_image[index+hsize-1]+r_image[index+hsize+1])/2.0;
    /* Red: Position #2 */
	r_image[index+1] = r_image[index+hsize+1];
    /* Red: Position #3 */
	r_image[index+hsize] = (r_image[index+hsize-1]+r_image[index+hsize+1])/2.0;
	/* Green: Position #1 */
	g_image[index] = (g_image[index-1]+g_image[index+1]+g_image[index+hsize])/3.0;
    }

  /* Interpolate left top corner */
  index = 0;
  /* Red: Position #1 */
  r_image[index] = r_image[index+1+hsize];
  /* Red: Position #2 */
  r_image[index+1] = r_image[index+1+hsize];
  /* Red: Position #3 */
  r_image[index+hsize] = r_image[index+1+hsize];
  /* Green: Position #1 */
  g_image[index] = (g_image[index+1]+g_image[index+hsize])/2.0;

  /* Interpolate right top corner */
  index = hsize-2;
  /* Blue: Position #2 */
  b_image[index+1] = b_image[index];
  /* Blue: Position #3 */
  b_image[index+hsize] = (b_image[index]+b_image[index+(2*hsize)])/2.0;     
  /* Blue: Position #4 */
  b_image[index+hsize+1] = (b_image[index]+b_image[index+(2*hsize)])/2.0;     
  /* Red: Position #1 */
  r_image[index] = (r_image[index+hsize-1]+r_image[index+hsize+1])/2.0;
  /* Red: Position #2 */
  r_image[index+1] = r_image[index+hsize+1];
  /* Red: Position #3 */
  r_image[index+hsize] = r_image[index+1+hsize];
  /* Green: Position #1 */
  g_image[index] = (g_image[index-1]+g_image[index+1]+g_image[index+hsize])/3.0;
  /* Green: Position #4 */
  g_image[index+1+hsize] = (g_image[index+1]+g_image[index+hsize]+g_image[index+1+(2*hsize)])/3.0;

  /* Interpolate right bottom corner */
  index = (hsize-2) + ((vsize-2)*hsize);
  /* Blue: Position #2 */
  b_image[index+1] = b_image[index];
  /* Blue: Position #3 */
  b_image[index+hsize] = b_image[index];
  /* Blue: Position #4 */
  b_image[index+hsize+1] = b_image[index];
  /* Green: Position #4 */
  g_image[index+1+hsize] = (g_image[index+1]+g_image[index+hsize])/2.0;

  /* Interpolate left bottom corner */
  index = ((vsize-2)*hsize);
  /* Blue: Position #2 */
  b_image[index+1] = (b_image[index]+b_image[index+2])/2.0;
  /* Blue: Position #3 */
  b_image[index+hsize] = b_image[index];
  /* Blue: Position #4 */
  b_image[index+hsize+1] = (b_image[index]+b_image[index+2])/2.0;
  /* Red: Position #1 */
  r_image[index] = (r_image[index-hsize+1]+r_image[index+hsize+1])/2.0;
  /* Red: Position #2 */
  r_image[index+1] = (r_image[index-hsize+1]+r_image[index+hsize+1])/2.0;
  /* Red: Position #3 */
  r_image[index+hsize] = r_image[index+hsize+1];
  /* Green: Position #1 */
  g_image[index] = (g_image[index-hsize]+g_image[index+1]+g_image[index+hsize])/3.0;
  /* Green: Position #4 */
  g_image[index+1+hsize] = (g_image[index+1]+g_image[index+hsize]+g_image[index+hsize+2])/3.0;

  /* We may still have texture because the original green elements were asymmetric. ? */

  return 0;

}


/* Get image (from camera or randomly generated) and save or print to 
 * STDOUT in either pgm, ppm, or tiff format.	
 * frame:	frame number to get. -1 indicates generate a fake image.
 * filename:	The filename to save as. An empty string indicates STDOUT.
 *		A null string indicates no output.
 * format:	"pgm", "ppm", or "tiff".			   	 
 * trueccdcolor: 0 for no (interpolate), 1 for yes (no interpolation) */
int kmc_get_image(long frame, char *filename, char *format, int trueccdcolor) {

  int 	tiff_format;

  tiff_format = (strncmp(format,"tiff",3) == 0);

  /* set color by hand - for testing - erase this later */
  /*color=0;*/

  /* test validity of input parameters */

  /* For tiffs (monochrome or color cameras) */
  if (tiff_format) {
  	get_tiff_frame(frame);            /* note it doesn't make fake tiffs */
  	make_image_from_tiff_imagedata();	/* does it need to reformat? */
  	write_tiff(filename);
	}

  /* Determine if camera is color or gray */
  get_device_info();

  /* For monochrome cameras (pgm and ppm outputs) */
  if ( (!tiff_format) && (color == 0) ){
  	/* get image */
  	if (frame < 0 )  make_fake_frame(256,240,0);
  	if (frame >= 0)  get_frame(frame);

  	/* create image from imagedata (pixels must be re-ordered) */
	 make_image_from_gray_imagedata();
  
  	/* save/output image */
	write_gray_image(filename,format);
	}

  /* For color cameras(pgm and ppm outputs) */
  if ( (!tiff_format) && (color == 1) ){
  	/* get image */
  	if (frame < 0 )  make_fake_frame(100,100,0);
  	if (frame >= 0)  get_frame(frame);

  	/* create image from imagedata (pixels must be re-ordered) */
	 make_image_from_color_imagedata();

	/* Interpolate each component color space */
	if (trueccdcolor == 0) interp_color_image();
  
  	/* save/output image */
	write_color_image(filename,format);
    }

  return 0;
}



/* Determine if the device described in global variables [vendor,product]
 * is a Kodak Motion Corder. 						   */
int is_a_kmc()
{
    int 	i;
    int 	isakmc=0; 

    for (i = 0; i < sizeof(kmc_names) / sizeof(char *); i++) {
	if (!strncmp(product,kmc_names[i],strlen(kmc_names[i]))) isakmc = 1;
       }
    return(isakmc);
} 



/* Print out information about generic scsi devices on the system: 
 * If action is 0, it lists all known generic scsi devices on system.
 * If action is 1, it lists only the Kodak Motion Corders on the system. 
 * If action is 2, it lists the vendor and model for all generic devices. */
int kmc_list_devices(int action)
{
    int i;
    int serrno;

    for (i = 0; i < sizeof(devices) / sizeof(char *); i++) {
        dev_fd = open(devices[i], O_RDWR);
        serrno = errno;
        if ( dev_fd == -1 ) {
            if (  ! strcmp(sys_errlist[serrno],"Permission denied") ) {
                fprintf( stderr, "%s: Permission denied\n",devices[i]);
                }
            continue;
            }
        get_device_info();

	if (action == 0) printf("%s\n",devices[i]); 
	if ( (action == 1) && is_a_kmc() ) printf("%s\n",devices[i]); 
	if (action == 2) printf("%s \t%.8s \t%.16s\n",devices[i],vendor, product);
        close(dev_fd);
    }
    return 0;
}


int kmc_test(int n)
{
   printf("This is kmc_test.\n");
   printf("kmc_test: received an integer: %d\n",n);
}


int kmc_test2(int n)
{
   printf("This is kmc_test2.\n");
   return 213;
}


int kmc_test3()
{
   printf("vendor: %s\n",vendor);
}


/* For gray images, replicate into RGB triplets for GTK+ Pixmap */
void make_imageout(unsigned char *imageout)
{
    int i;

    for (i=0; i<hsize*vsize; i++) {
	imageout[3*i] = image[i];	
	imageout[3*i+1] = image[i];	
	imageout[3*i+2] = image[i];	
	}
}


/* Library routine for grabbing an image from another C program */
int kmc_return_image(unsigned char *imageout, int framenumber)
{

   kmc_get_image(framenumber,"\0","ppm",0);
   make_imageout(imageout);
   return 0;
}


/* Library routine for returning frame size */
int kmc_return_framesize(int *width, int *height)
{
   get_device_info();
   *height = vsize;
   *width = hsize;
   return 0;
}


