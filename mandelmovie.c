/**************************************************************
Filename: mandelmovie.c 
Description: This program generates a zooming animation of the 
Mandelbrot set using multi-processing. The final result is a 
sequence of images that can be combined into a 4K 30 FPS movie.
Author: Cade Andrae
Date: 11/26/24
Compile Instructions: gcc -o mandelmovie mandelmovie.c jpegrw.c -ljpeg -lm
Test Instructions:
- ./mandelmovie -x -0.743643 -y 0.131825 -s 4 -W 3840 -H 2160 -m 1000 -n 300 -p <num_processes>
-  ffmpeg -framerate 30 -i mandel%d.jpg -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow mandelzoom.mp4
**************************************************************/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "jpegrw.h"

static int iteration_to_color(int i, int max);
static int iterations_at_point(double x, double y, int max);
static void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max);
static void show_help();

int main(int argc, char *argv[]) {
    char c;
    double xcenter = -0.743643;
    double ycenter = 0.131825;
    double xscale = 4.0;                                // Start at the default scale
    int image_width = 3840;                             // 4K width
    int image_height = 2160;                            // 4K height
    int max_iterations = 1000;
    int num_images = 300;                               // Create 300 images for 30 FPS, 10 seconds of video
    int num_processes = sysconf(_SC_NPROCESSORS_ONLN);  // Default to all available CPU threads
    char outfile_base[256] = "mandel";
    int preview_final = 0;                              // Flag for previewing the final image

    while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:p:n:hP")) != -1) {
        switch (c) {
            case 'x':
                xcenter = atof(optarg);
                break;
            case 'y':
                ycenter = atof(optarg);
                break;
            case 's':
                xscale = atof(optarg);
                break;
            case 'W':
                image_width = atoi(optarg);
                break;
            case 'H':
                image_height = atoi(optarg);
                break;
            case 'm':
                max_iterations = atoi(optarg);
                break;
            case 'o':
                strncpy(outfile_base, optarg, sizeof(outfile_base) - 1);
                outfile_base[sizeof(outfile_base) - 1] = '\0';
                break;
            case 'p':
                num_processes = atoi(optarg);
                break;
            case 'n':
                num_images = atoi(optarg);
                break;
            case 'P':
                preview_final = 1;
                break;
            case 'h':
                show_help();
                exit(1);
                break;
        }
    }

    double yscale = xscale / image_width * image_height;                        	        // Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
    double final_scale = 1e-3;                                                              // Final scale for a deeper zoom
    double zoom_factor = pow(final_scale / xscale, 1.0 / num_images);

    // If preview_final, generate only the last image
    if (preview_final) {
        double final_scale = xscale * pow(zoom_factor, num_images - 1);
        double ymin = ycenter - final_scale / 2;
        double ymax = ycenter + final_scale / 2;
        double xmin = xcenter - final_scale / 2;
        double xmax = xcenter + final_scale / 2;

        char final_outfile[256];
        size_t max_base_length = sizeof(final_outfile) - strlen("_final.jpg") - 1;
        if (strlen(outfile_base) > max_base_length) {
            fprintf(stderr, "Error: Base filename too long. Truncating.\n");
            strncpy(final_outfile, outfile_base, max_base_length);
            final_outfile[max_base_length] = '\0';
        } else {
            strcpy(final_outfile, outfile_base);
        }
        strcat(final_outfile, "_final.jpg");

        imgRawImage *img = initRawImage(image_width, image_height);                         // Create a raw image of the appropriate size.
        compute_image(img, xmin, xmax, ymin, ymax, max_iterations);                         // Compute the Mandelbrot image
        storeJpegImageFile(img, final_outfile);                                             // Save the image in the stated file.
        freeRawImage(img);                                                                  // free the mallocs


        printf("Generated final preview image: %s\n", final_outfile);
        exit(0);
    }

    printf("mandelmovie: x=%lf y=%lf xscale=%lf yscale=%lf max=%d images=%d processes=%d\n",
           xcenter, ycenter, xscale, yscale, max_iterations, num_images, num_processes);

    pid_t pids[num_processes];
    int images_per_process = num_images / num_processes;
    int remainder_images = num_images % num_processes;                                      // For uneven division of images

    for (int p = 0; p < num_processes; ++p) {
        if ((pids[p] = fork()) == 0) {                                                      // Child process
            int start = p * images_per_process;
            int end = start + images_per_process;
            if (p == num_processes - 1) {
                end += remainder_images;                                                    // Last process gets extra images
            }
            for (int i = start; i < end; ++i) {
                double scale = xscale * pow(zoom_factor, i);
                char outfile[256];
                if (snprintf(outfile, sizeof(outfile), "%s%d.jpg", outfile_base, i) >= sizeof(outfile)) {
                    fprintf(stderr, "Error: Output filename too long or truncated.\n");
                    exit(EXIT_FAILURE);
                }

                double ymin = ycenter - scale / 2;
                double ymax = ycenter + scale / 2;
                double xmin = xcenter - scale / 2;
                double xmax = xcenter + scale / 2;

                imgRawImage *img = initRawImage(image_width, image_height);                 // Create a raw image of the appropriate size.
                compute_image(img, xmin, xmax, ymin, ymax, max_iterations);                 // Compute the Mandelbrot image
                storeJpegImageFile(img, outfile);                                           // Save the image in the stated file.
                freeRawImage(img);                                                          // free the mallocs
                printf("Generated: %s\n", outfile);
            }
            exit(0);
        }
    }

    // Parent process waits for all children to complete
    for (int p = 0; p < num_processes; ++p) {
        waitpid(pids[p], NULL, 0);
    }
    printf("All images generated. Use ffmpeg to create the movie:\n");
    printf("ffmpeg -framerate 30 -i %s%%d.jpg -pix_fmt yuv420p mandelzoom.mp4\n", outfile_base);
    return 0;
}

/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/
int iterations_at_point(double x, double y, int max) {
    double x0 = x;
    double y0 = y;
    int iter = 0;

    while ((x * x + y * y <= 4) && iter < max) {
        double xt = x * x - y * y + x0;
        double yt = 2 * x * y + y0;
        x = xt;
        y = yt;
        iter++;
    }

    return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/
void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max) {
    int width = img->width;
    int height = img->height;

    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            double x = xmin + i * (xmax - xmin) / width;
            double y = ymin + j * (ymax - ymin) / height;
            int iters = iterations_at_point(x, y, max);
            setPixelCOLOR(img, i, j, iteration_to_color(iters, max));
        }
    }
}

//Convert a iteration number to a color.
int iteration_to_color(int iters, int max) {
    if (iters == max) return 0x000000;

    double t = (double)iters / max;
    unsigned char red = (unsigned char)(9 * (1 - t) * pow(t, 3) * 255);
    unsigned char green = (unsigned char)(15 * pow((1 - t), 2) * pow(t, 2) * 255);
    unsigned char blue = (unsigned char)(8.5 * pow((1 - t), 3) * t * 255);

    return (red << 16) | (green << 8) | blue;
}

// Show help message
void show_help() {
    printf("Usage: mandelmovie [options]\n");
    printf("Options:\n");
    printf("  -x <coord>  X coordinate of image center. Default: -0.743643\n");
    printf("  -y <coord>  Y coordinate of image center. Default: 0.131825\n");
    printf("  -s <scale>  Initial scale. Default: 4\n");
    printf("  -W <width>  Image width in pixels. Default: 3840 (4K)\n");
    printf("  -H <height> Image height in pixels. Default: 2160 (4K)\n");
    printf("  -m <max>    Max iterations. Default: 1000\n");
    printf("  -o <base>   Output filename base. Default: mandel\n");
    printf("  -p <procs>  Number of processes. Default: all CPU threads\n");
    printf("  -n <images> Number of images. Default: 300\n");
    printf("  -P          Preview the final image only.\n");
    printf("  -h          Show help.\n");
}
