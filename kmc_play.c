/* Test program to determine speed of SCSI transfer and image creation
 *
 * Example: gdkrgb_kmc_test1 4 50 5 /dev/sgd *
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
#include <gtk/gtk.h>
#ifndef GTK_HAVE_FEATURES_1_1_0
#include "gdkrgb.h"
#endif

#define MAXWIDTH 512
#define MAXHEIGHT 480

int width, height, dev_fd;
long frame_start=0;
long frame_stop=100;
long frame_step=2;
long nframes_displayed=0;
char devicename[8]="/dev/sgd";

static void
quit_func (GtkWidget *widget, gpointer dummy)
{
  gtk_main_quit ();
}


static void
testrgb_rgb_test (GtkWidget *drawing_area)
{
  guchar buf[MAXWIDTH * MAXHEIGHT * 3];
  gint i, j;
  gint x, y;
  gboolean dither;
  int dith_max;

  int junk;
//  unsigned char image[MAXWIDTH * MAXHEIGHT * 3];	
  guchar image[MAXWIDTH * MAXHEIGHT * 3];	
  long t1, t2;

  /* Set up ditherable */
  if (gdk_rgb_ditherable ())
    dith_max = 2;
  else
    dith_max = 1;

  /* Make sure it is a kmc */
  dev_fd = kmc_open_device(devicename);
  if (!kmc_is_kmc()) {
    fprintf(stderr, "Error: Device %s is not a kmc.\n",devicename);
    exit(1);
    }
  kmc_close_device(dev_fd);


  t1 = time(0);

  for (dither = 0; dither < dith_max; dither++) {
      for (i = frame_start; i < frame_stop; i+=frame_step) {

          nframes_displayed += 1;

          /* Open device */         
          dev_fd = kmc_open_device(devicename);

          /* Read in image */
		  printf("Frame Number: %d\n",i);
		  kmc_return_image(image,i);

          /* Close device */
          kmc_close_device(dev_fd);


	      gdk_draw_rgb_image (drawing_area->window,
                  drawing_area->style->white_gc,
                  0, 0, width , height,
                  dither ? GDK_RGB_DITHER_MAX :
                  GDK_RGB_DITHER_NONE,
                  image, width * 3);

   		 }
    }


  /* Print out time to run */
  t2 = time(0);
  printf("Time: %d\n",t2-t1);
  printf("Average frame rate: %f fps\n",((float)nframes_displayed)/(t2-t1));


}

void
new_testrgb_window (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *drawing_area;

  window = gtk_widget_new (gtk_window_get_type (),
               "GtkObject::user_data", NULL,
               "GtkWindow::type", GTK_WINDOW_TOPLEVEL,
               "GtkWindow::title", "testrgb",
               "GtkWindow::allow_shrink", FALSE,
               NULL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
              (GtkSignalFunc) quit_func, NULL);

  vbox = gtk_vbox_new (FALSE, 0);

  drawing_area = gtk_drawing_area_new ();

  /* determine image size from camera */
  dev_fd = kmc_open_device("/dev/sgd");
  kmc_return_framesize(&width,&height);
  printf("Image size: %d x %d\n",width,height);

  gtk_widget_set_usize (drawing_area, width, height);
  gtk_box_pack_start (GTK_BOX (vbox), drawing_area, FALSE, FALSE, 0);
  gtk_widget_show (drawing_area);

  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                 (GtkSignalFunc) gtk_widget_destroy,
                 GTK_OBJECT (window));

  gtk_widget_show (button);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  gtk_widget_show (window);

  testrgb_rgb_test (drawing_area);
}


int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  /* parse command line parameters */
  if (argc>=2) frame_start = atoi(argv[1]);
  if (argc>=3) frame_stop= atoi(argv[2]);
  if (argc>=4) frame_step= atoi(argv[3]);
  if (argc>=5) strcpy(devicename,argv[4]);
  printf("devicename: %s \n",devicename);
//  printf("frame_start: %d \n",frame_start);
//  printf("frame_stop: %d \n",frame_stop);
//  printf("frame_step: %d \n",frame_step);

  gdk_rgb_set_verbose (TRUE);
  
  gdk_rgb_init ();

  gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
  gtk_widget_set_default_visual (gdk_rgb_get_visual ());
  new_testrgb_window ();

  gtk_main ();

  return 0;
}

