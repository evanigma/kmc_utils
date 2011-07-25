/* This program communicates through the generic SCSI driver with a  
 * Kodak Motion Corder SR high speed digital cameras.
 *
 * Usage:
 *
 *     kmc_read [options] [DEVICE]
 *
 * Options are:
 *   -h	       		Show help (usage) information
 *   -v	       		Show version number
 *   -l	       		List generic scsi devices on system
 *   -L	       		List vendor and model of generic scsi devices on system 
 *   -k	       		List generic scsi devices which are Kodak Motion Corders
 *   -q	       		Short query of device information for DEVICE
 *   -Q	       		Long query of device information for DEVICE
 *   -f N      		Read frame N from DEVICE 
 *   -s N		Read multiple frames, starting at frame N 
 *   -e N		Read multiple frames, ending at frame N 
 *   -n	N		Read a total of N frames
 *   -r			Renumber frames when saving starting at 0
 *   -c			Chronological numbering is used
 *   -T			True CCD coloring (no interpolation)
 *   -t			Save/output as tiff 
 *   -p			Save/output as ppm 
 *   -g			Save/output as pgm 
 *                          (default:pgm for monochrome camera, ppm for color)
 *   -i	filename	Write long query of device to <filename> 
 *   -o	filename	Save image(images) to <filename>(<filename>#####)
 *			    (default is STDOUT)
 *
 * NOTE: When reading multiple frames, one must use exactly two of the
 *       three options -s, -e, -n. If -1 is specified as the parameter for
 *	 -s(-e) then the first(last) frame will be used.  
 *
 * NOTE: The frames are numbered depending upon the trigger mode:
 * 	   (The trigger frame is alway frame 0.) 
 *		1) Start trigger: 	0, 1,  ..., N, (N-1)
 *		2) Center trigger: 	(N/2), ... (N-1), 0, 1, ..., ((N/2)-1) 
 *		3) Stop trigger:   	1, 2, ..., (N-1), 0
 *
 * You need to be able to open the scsi device as a file.  
 * Depending on the permissions, you may need root privileges
 * to run this.
 *
 * Examples:
 *		 kmc_read -H -f 66 /dev/sga  | display
 *
 *
 * Dan Mueth - 08/23/99.  
 * Patched by Evan Fox - 07/24/11. Version 0.3.3
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
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <time.h>		/* added for random number */
#include <fcntl.h>            
#include <errno.h>
#include <scsi/sg.h>		


#define MAX_RAND                16777216        /*   256 * 256 * 256   */
#define NUMBER_LEN              5       /* The number of digits to use for
                                           numbering filenames */


/* flags, used for parsing options */
char help = 0;
char listdevices = 0;
char listdevicesvm = 0;
char listkmcs = 0;
char shortquery = 0;
char longquery = 0;
char renumber = 0;
char chronological = 0;
char readsingleframe = 0;
char readmultiframes = 0;
char trueccdcolor = 0;


/* global variables related to camera properties */
long nframes = 0;
int trigger, hsize, vsize, color;

/* global variables related to user input */
char startframeset = 0;
char endframeset = 0;
char nframesset = 0;
char save_frames = 0;
long start_frame, end_frame, n_frames;
long framenumber;
unsigned char specifiedformat[5];


/* other global variables */
unsigned char infofilename[200];	/* filename for info file (long query)*/
unsigned char filenamebase[200];	/* base filename for image file(s) */
int dev_fd;                             /* SCSI device/file descriptor */
int imagefile_fd;                       /* image file file descriptor */
char *device_name;
long grabframenums[10000];		/* list of frames to transfer */
long saveframenums[10000];		/* number to give to frams as saving */

unsigned char 		image[245761];   /* The image data, in order: 512x480*/
unsigned char 		imageformat[5];


/* obsolete */
char pgmheader = 0;



void usage(char *errtext)
{
    fprintf(stderr, "Error: kmc_read - %s\n", errtext);
    fputs("Usage: kmc_read [options] [DEVICE]\n"
          "\tOptions:\n"
          "\t-h              Show help (usage) information\n"
          "\t-v              Show version number\n"
          "\t-l              List generic scsi devices on system\n"
          "\t-L              List vendor and model of generic scsi devices on system\n"
          "\t-k              List generic scsi devices which are Kodak Motion Corders\n"
          "\t-q              Short query of device information for DEVICE\n"
          "\t-Q              Long query of device information for DEVICE\n"
          "\t-f N            Read frame N from DEVICE\n"
	  "\t-s N            Read multiple frames, starting at frame N\n"
	  "\t-e N            Read multiple frames, ending at frame N\n"
	  "\t-n N            Read a total of N frames\n"
	  "\t-r              Renumber frames when saving starting at 0\n"
	  "\t-c              Chronological numbering is used\n"
	  "\t-T              True CCD coloring (no interpolation)\n"
	  "\t-t              Save/output as tiff\n"
	  "\t-p              Save/output as ppm\n"
	  "\t-g              Save/output as pgm\n"
	  "\t                    (default:pgm for monochrome camera, ppm for color)\n"
          "\t-i filename     Write long query of device to <filename>\n"
          "\t-o filename     Save image(images) to <filename>(<filename>#####)\n"
          "\t                    (default is STDOUT)\n", stderr);
    exit(2);
}


int make_random_frame()
{
    char c[16384];		/* 128x128 */
    int pixval=0;			/* random number for value of all pixels */
    int tt;
    int index;

/*    printf("save_frame: hi\n"); */

	/* The following is very sloppy but works and is temporary */
    tt = time(0);
/*    printf("time %d\n",tt); */
    srand(tt);
/*    pixval = 1+(int) (255.0*rand()/(MAX_RAND+1.0)); */

   /* NOTE: I THINK I MESSED THIS NEXT LINE UP */
    pixval =  255*rand()/MAX_RAND; 

/*    pixval = pixval%5; */
/*    pixval += 50; */
/*    printf("Pixel value: %d\n",pixval); */
/*    printf("Pixel values are: %d\n",pixval); */
/*  55 is a 7 */

    /* Set pixel values */
    for (index=0; index<16384; index++) {
	c[index]=pixval;
	}

    /* Print data */
    for (index=0; index<16384; index++) {
	printf("%c%c%c",c[index],c[index],c[index]);
	}

    return 0;
}


int write_image_to_file(unsigned char *filename)
{
  int 		status = 0;
  unsigned char header[20];

  printf("Saving file to disk: %s\n",filename);

  /* save the frame to file */
  imagefile_fd = creat(filename,0644);
      if ( imagefile_fd < 0 ) {
            perror("kmc_read(open)");
            exit(1);
            }

  sprintf( header, "P5\n%d %d\n255\n", hsize, vsize);
  status = write( imagefile_fd, &header[0], strlen(header) );
  if ( status < 0 || status != strlen(header) ) {
        /* some error happened */
        perror("kmc_write(image)");
        return status;
        }

  status = write( imagefile_fd, image, hsize*vsize ); 
  if ( status < 0 || status != hsize*vsize ) {
        /* some error happened */
        perror("kmc_write(image)");
        return status;
        }

  close(imagefile_fd);
  return 0;
}


/* Parses the parameters for grabbing multiple images */
int set_multi_vars()
{
	long 	i;
	long	cfirst;

	/*
	 * Read camera trigger settings and number of frames. 
	 * This is needed to verify the validity of the settings and to
	 * generate missing values.	
	 */
	trigger = kmc_get_triggerval();
	nframes = kmc_get_nframes();    
//	trigger = 1;
//	nframes = 10;
//	printf("trigger: %d\n",trigger);
//	printf("nframes: %d\n",nframes);


	/* 
	 * Check parameters passed on command line for trigger-independent
	 * constraints.
	 */
	if (startframeset && (start_frame < 0) ) {
		printf("Error: start frame(%d) must be >= 0 \n",start_frame); 
		exit(1);
		}
	if (startframeset && (start_frame >= nframes) ) {
		printf("Error: start frame(%d) must be <= total number of frames on camera \n",start_frame,nframes); 
		exit(1);
		}
	if (endframeset && (end_frame < 0) ) {
		printf("Error: end frame(%d) must be >= 0\n",end_frame); 
		exit(1);
		}
	if (endframeset && (end_frame >= nframes) ) {
		printf("Error: end frame(%d) must be <= total number of frames on camera minus one (%d)\n",end_frame,nframes-1); 
		exit(1);
		}
	if (nframesset && (n_frames < 0) ) {
		printf("Error: number of frames(%d) must be >=0\n",n_frames); 
		exit(1);
		}
	if (nframesset && (n_frames > nframes) ) {
		printf("Error: number of frames(%d) must be <= number of frames on camera(%d)\n", n_frames, nframes); 
		exit(1);
		}

	/*
	 * Check constraints which depend upon triggering and determine 
	 * parameters which were not set on command line.
	 */
	if (chronological || ((trigger == 0) || (trigger == 3)) ) {
		/* Frames ordered with first chronological frame 0 and last is N-1*/
		if (startframeset && endframeset) {
			if ( end_frame < start_frame ) {
				printf("Error: end frame(%d) must be >= start frame(%d)\n", end_frame, start_frame); 
				exit(1);
				}
			n_frames = end_frame - start_frame + 1;
			}
		if (startframeset && nframesset) {
			if ( (start_frame+n_frames-1) > (nframes-1) ) {
				printf("Error: cannot grab %d frames starting at %d. Last frame is %d.\n", n_frames, start_frame, nframes-1); 
				exit(1);
				}
			end_frame = start_frame + n_frames - 1;
			}
		if (endframeset && nframesset) {
			if ( (end_frame - n_frames + 1) < 0 ) {
				printf("Error: cannot grab %d frames ending at %d. First frame must be >0.\n", n_frames, end_frame); 
				exit(1);
				}
			start_frame = end_frame - n_frames + 1;
			}
		}

	cfirst = nframes/2;		/* This is the first frame if center-triggered. */
	if ( !chronological && (trigger == 1) ) {
		/* Center Trigger: first frame is cfirst=nframes/2, last is cfirst-1 */
		if (startframeset && endframeset) {
			if ( ((start_frame-cfirst+nframes)%nframes) > ((end_frame-cfirst+nframes)%nframes) ) {
				printf("Error: start frame (%d) must preceed end frame (%d) chronologically\n", start_frame, end_frame); 
				exit(1);
				}
			if (start_frame != (end_frame+1) ) n_frames = (end_frame - start_frame + 1 + nframes) % nframes; 
			if (start_frame == (end_frame+1) ) n_frames = nframes; 
			}	
		if (startframeset && nframesset) {
			if ((((start_frame-cfirst+nframes)%nframes)+n_frames-1) >= nframes){
				printf("Error: cannot transfer (%d) frames starting at frame (%d). (%d is last frame.)\n", n_frames, start_frame, cfirst-1);
				exit(1);
				}
			end_frame = (start_frame + n_frames - 1) % nframes;
			}	
		if (endframeset && nframesset) {
			if ((((end_frame-cfirst+nframes)%nframes)-n_frames+1) < 0) {
				printf("Error: cannot transfer (%d) frames ending at frame (%d)\n", n_frames, end_frame);
				exit(1);
				}
			start_frame = (end_frame - n_frames + 1 + nframes) % nframes;
			}	
		}
	if ( !chronological && (trigger == 2) ) {
		/*
		 * Stop Trigger.
		 * First frame is 1. Second to last frame is N-1. Last frame is 0.
		 */
		if (startframeset && endframeset) {
			if ( (end_frame < start_frame) && (end_frame != 0) ) {
				printf("Error: end frame(%d) must be >= start frame(%d) or 0(last frame)\n", end_frame, start_frame); 
				exit(1);
				}
			if ( (start_frame == 0) && (end_frame != 0) ) {
				printf("Error: end frame (%d) is before start frame (%d), which is the last frame\n",end_frame, start_frame);
				exit(1);
				}
			if (end_frame != 0) n_frames = end_frame - start_frame + 1;
			if ( (end_frame == 0) && (start_frame != 0) ) n_frames = nframes - start_frame + 1;
			if ( (end_frame == 0) && (start_frame == 0) ) n_frames = 1;
			}
		if (startframeset && nframesset) {
			if ( (start_frame+n_frames-1) > (nframes) ) {
				printf("Error: cannot grab %d frames starting at %d. Last two frames are %d, 0.\n", n_frames, start_frame, nframes-1); 
				exit(1);
				}
			if ( (start_frame == 0) && (n_frames > 1) ) {
				printf("Error: cannot grab %d frames starting at frame %d, which is the last frame.\n", n_frames, start_frame);
				exit(1);
				}
			end_frame = start_frame + n_frames - 1;
			if (end_frame == nframes) end_frame = 0;
			}
		if (endframeset && nframesset) {
			if ( ((end_frame - n_frames + 1) < 0) && (end_frame != 0) ) {
				printf("Error: cannot grab %d frames ending at %d. First frame must be >1.\n", n_frames, end_frame); 
				exit(1);
				}
			if (end_frame != 0) start_frame = end_frame - n_frames + 1;
			if (end_frame == 0) start_frame = nframes - n_frames + 1; 
			}
		}



	/* Now set the list of images to download, and list of default numbers
	 * to save as.  If renumbering is opted for, it will be done later. */
	if ( (trigger == 0) || (trigger == 3) ) {  /* Start or Random Trigger */
		/* independent of chronological */
		for (i=0; i<n_frames; i++) {
			grabframenums[i] = start_frame + i;
			saveframenums[i] = start_frame + i;
			}
		}
	if ((trigger == 1) && (!chronological)) { /* Center Trigger, not chron. */
		for (i=0; i<n_frames; i++) {
			grabframenums[i] = (start_frame + i) % nframes;
			saveframenums[i] = (start_frame + i) % nframes;
			}
		}
	if ((trigger == 1) && (chronological)) {  /* Center Trigger, chron. */
		for (i=0; i<n_frames; i++) {
			grabframenums[i] = (start_frame + i + cfirst) % nframes;
			saveframenums[i] = start_frame + i;
			}
		}
	if ((trigger == 2) && (!chronological)) { /* Stop Trigger, not chron. */
		for (i=0; i<n_frames; i++) {
			grabframenums[i] = (start_frame + i) % nframes;
			saveframenums[i] = (start_frame + i) % nframes;
			}
		}
	if ((trigger == 2) && (chronological)) {  /* Stop Trigger, chron. */
		for (i=0; i<n_frames; i++) {
			grabframenums[i] = (start_frame + i + 1) % nframes;
			saveframenums[i] = start_frame + i;
			}
		}


	/* Renumber frames if desired, starting at 0 for first frame saved */ 
	if (renumber) {
		for (i=0; i<n_frames; i++) {
			saveframenums[i] = i;
			}
		}

//	printf("trigger: %d\n",trigger);
//	printf("nframes: %d\n",nframes);
//	printf("n_frames: %d\n",n_frames);
//	for (i=0; i<n_frames; i++) printf("%d --> %d\n",grabframenums[i], saveframenums[i] );

	return 0;
}


int read_multiple_frames()
{
    long 	i;
    unsigned char filename[200];
    unsigned char numberstring1[11];
    unsigned char numberstring2[11];

    /* Check the parameters related to multiple grabbing and set some
     * variables which will be used below.				*/
    set_multi_vars();

    /* Determine which image format the user wants the images in. */
    determine_image_format();

    for ( i=0; i<n_frames; i++) {

  	/* Set filename */
   	sprintf(numberstring1,"0000000000");
   	sprintf(numberstring2,"0000000000");
	sprintf(numberstring1,"%d",saveframenums[i]);
	memcpy(numberstring2+(10-strlen(numberstring1)),numberstring1,strlen(numberstring1));
  	sprintf(filename,"%s%s.%s",filenamebase,numberstring2+(10-NUMBER_LEN),imageformat);
  
	/* Read image to file */
        printf("Saving frame %d with file index %d as %s: %s\n",grabframenums[i],saveframenums[i],imageformat,filename);
	kmc_get_image(grabframenums[i], filename, imageformat, trueccdcolor);
	} 
 
    return 0;
}


/* Determines the format that the image should be saved as.
 * Default depends upon the camera:
 * 	monochrome camera	pgm is default
 * 	color camera		ppm is default
 * If the user specifies a format, use this format:
 * 	-g			save as pgm
 *	-p			save as ppm
 *	-t			save as tiff
 */
int determine_image_format()
{

  color = kmc_is_color();

  /* If user supplied format on command line, use that format */
  if (strlen(specifiedformat) != 0) {
	sprintf(imageformat,"%s",specifiedformat);
	}

  /* Otherwise, use default format determined by camera type */
  if (strlen(specifiedformat) == 0) {
	if (color) sprintf(imageformat,"ppm");
	else  sprintf(imageformat,"pgm");
	}
}


int read_one_frame()
{

	determine_image_format();     
	kmc_get_image(framenumber, filenamebase, imageformat, trueccdcolor);

}


int main(int argc, char *argv[])
{
	char c;
	int status = 0;

	if (argc < 2) 
		usage("too few arguments");
	while ((c = getopt(argc, argv, "hvlLkqQf:s:e:n:rcTtpgi:o:")) != EOF) {
		switch (c) {
	case 'h':
		help = 1;
		fprintf(stderr, "Usage: \n");
		fprintf(stderr, "      kmc_read [options] [DEVICE]\n\n");
		fprintf(stderr, " Options:\n");
		fprintf(stderr, "   -h              Show usage (help) information\n");
		fprintf(stderr, "   -v              Show version number\n");
		fprintf(stderr, "   -l              List generic scsi devices on system\n");
		fprintf(stderr, "   -L              List vendor and model of generic scsi devices on system\n");
		fprintf(stderr, "   -k              List generic scsi devices which are Kodak Motion Corders\n");
		fprintf(stderr, "   -q              Short query of device information for DEVICE\n");
		fprintf(stderr, "   -Q              Long query of device information for DEVICE\n");
		fprintf(stderr, "   -f N            Read frame N from DEVICE\n");
		fprintf(stderr, "   -s N            Read multiple frames, starting at frame N\n");
		fprintf(stderr, "   -e N            Read multiple frames, ending at frame N\n");
		fprintf(stderr, "   -n N            Read a total of N frames\n");
		fprintf(stderr, "   -r              Renumber frames when saving starting at 0\n");
		fprintf(stderr, "   -c              Chronological numbering is used\n");
		fprintf(stderr, "   -T              True CCD coloring (no interpolation)\n");
		fprintf(stderr, "   -t              Save/output as tiff\n");
		fprintf(stderr, "   -p              Save/output as ppm\n");
		fprintf(stderr, "   -g              Save/output as pgm\n");
		fprintf(stderr, "                       (default:pgm for monochrome camera, ppm for color)\n");
		fprintf(stderr, "   -i filename     Write long query of device to <filename>\n");
		fprintf(stderr, "   -o filename     Save image(images) to filename(filename####)\n");
		fprintf(stderr, "                       (default is STDOUT)\n\n");
		break;
	case 'v':
		fprintf(stderr, "kmc_read version 0.3.2\n");
		exit(0);
	case 'l':
		listdevices = 1;
		break;
	case 'L':
		listdevicesvm = 1;
		break;
	case 'k':
		listkmcs= 1;
		break;
	case 'q':
		shortquery = 1;
		break;
	case 'Q':
		longquery = 1;
		break;
	case 'f':
		readsingleframe = 1;
		framenumber = atoi(optarg);
		break;
	case 's':
		startframeset = 1;
	    readmultiframes = 1;
		start_frame = atoi(optarg);
		break;
	case 'e':
	    endframeset = 1;
	    readmultiframes = 1;
		end_frame = atoi(optarg);
		break;
	case 'n':
	    nframesset = 1;
	    readmultiframes = 1;
		n_frames = atoi(optarg);
		break;
	case 'r':
		renumber = 1;
		break;
	case 'c':
		chronological = 1;
		break;
	case 'T':
		trueccdcolor = 1;
		break;
	case 't':
		sprintf(specifiedformat,"tiff",4);
		break;
	case 'p':
		sprintf(specifiedformat,"ppm",3);
		break;
	case 'g':
		sprintf(specifiedformat,"pgm",3);
		break;
	case 'i':
		sprintf(infofilename,"%s", optarg); 
		break;
	case 'o':
		save_frames = 1;
		sprintf(filenamebase,"%s", optarg); 
		break;
	default:
		fprintf(stderr, "Unknown option '-%c' (ascii %02xh)\n", c, c);
		usage("bad option");
        };
    };

    if (help) {
        exit(0);
        }

    if (listdevices) {
        kmc_list_devices(0);
        exit(0);
        }

    if (listkmcs) {
        kmc_list_devices(1);
        exit(0);
        }

    if (listdevicesvm) {
        kmc_list_devices(2);
        exit(0);
        }

    if (optind >= argc)
        usage("no device name given");


    dev_fd = kmc_open_device(device_name = argv[optind]);


/* put get_device status back when not testing */
	status |= get_device_status();


    if ( (startframeset+endframeset+nframesset) == 2) status |= set_multi_vars();
	if ( (startframeset+endframeset+nframesset) == 1) { 
		printf("Error: insufficient description for multiple download\n");
		exit(1);
		}
	if ( (startframeset+endframeset+nframesset) == 3) {
		printf("Error: only two of the three options -s, -e, -n can be used\n");
		exit(1);
		}



    /* Save long query information if user selected this option */
    if (strlen(infofilename) != 0) kmc_long_query(infofilename);


	if (shortquery)
		status |= kmc_short_query();
	if (longquery)
		status |= kmc_long_query("");
	if (readsingleframe)
		status |= read_one_frame();  
/*		status |= make_random_frame();  */
	if (readmultiframes)
		status |= read_multiple_frames();


	kmc_close_device(dev_fd);

	return status ? 1 : 0;
}
