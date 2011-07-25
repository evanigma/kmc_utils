/* Test program to determine speed of SCSI transfer and image creation
 * 
 * Dan Mueth - 05/14/99. 
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
 */


#include <stdio.h>
#include <stdlib.h>

#define MAXWIDTH 512
#define MAXHEIGHT 480
#define NFRAMES 40

char devicename[10]="/dev/sga";

int
main (int argc, char **argv)
{
  int dev_fd;
  int width, height, i;
  long t1,t2;
  unsigned char image[MAXWIDTH * MAXHEIGHT * 3];


  /* Parse parameters */
  if (argc==1) {
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "    kmc_speedtest [DEVICE]\n\n");
    exit(1);
    }
  if (argc>=2) strcpy(devicename,argv[1]);

  /* Open device */
  dev_fd = kmc_open_device(devicename);

  /* Determine if it is a kmc */
  if (!kmc_is_kmc()) {
    fprintf(stderr, "Error: Device %s is not a kmc.\n",devicename);
    exit(1);
    }

  /* Determine frame size */
  kmc_return_framesize(&width,&height);
  printf("Frame size: %d x %d\n",width,height);

  /* Time download and image creation. */
  printf("Downloading data for %d images from camera and making images...\n",NFRAMES);
  t1 = time(0);
  for (i=0; i<NFRAMES; i++){
    kmc_return_image(image,i);
    }
  t2 = time(0);
  printf("Time: %d seconds\n",t2-t1);
  printf("Average frame rate: %f fps\n",((float)NFRAMES)/(t2-t1));

  /* Time download */
  printf("Downloading data for %d images from camera but not making images\n",NFRAMES);
  t1 = time(0);
  for (i=0; i<NFRAMES; i++){
    get_frame(i);
    }
  t2 = time(0);
  printf("Time: %d seconds\n",t2-t1);
  printf("Average frame rate: %f fps\n",((float)NFRAMES)/(t2-t1));

  /* Close device */
  kmc_close_device(dev_fd);
}

