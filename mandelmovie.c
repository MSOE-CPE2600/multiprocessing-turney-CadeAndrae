/**************************************************************
Filename: mandelmovie.c 
Description: This program generates a zooming animation of the 
Mandelbrot set using multi-processing and multi-threading. The 
final result is a sequence of images that can be combined into 
a 4K 30 FPS movie.
Author: Cade Andrae
Date: 11/26/24
Compile Instructions: gcc -o mandelmovie mandelmovie.c jpegrw.c -ljpeg -lm
Test Instructions:
- ./mandelmovie -x -0.743643 -y 0.131825 -s 4 -W 3840 -H 2160 -m 1000 -n 300 -p <num_processes> -t <num_threads>
-  ffmpeg -framerate 30 -i mandel%d.jpg -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow mandelzoom.mp4
**************************************************************/

#include "mandelmovie.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
    char c;
    double xcenter = -0.743643;
    double ycenter = 0.131825;
    double xscale = 4.0;
    int image_width = 3840;                             // 4K width
    int image_height = 2160;                            // 4K height
    int max_iterations = 2000;
    int num_images = 300;                               // Create 300 images for 30 FPS, 10 seconds of video
    int num_processes = sysconf(_SC_NPROCESSORS_ONLN);  // Default to all available CPU threads
    int num_threads = 1;
    char outfile_base[256] = "mandel";
    int preview_final = 0;                              // Flag for previewing the final image

    while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:p:n:t:Ph")) != -1) {
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
            case 't':
                num_threads = atoi(optarg);
                if (num_threads < 1 || num_threads > MAX_THREADS) {
                    fprintf(stderr, "Error: Number of threads must be between 1 and %d.\n", MAX_THREADS);
                    exit(1);
                }
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
    double final_scale = 1e-11;                                                             // Final scale for a deeper zoom
    double zoom_factor = pow(final_scale / xscale, 1.0 / num_images);
    
    // If preview_final, generate only the last image
    if (preview_final) {
        double scale = xscale * pow(zoom_factor, num_images - 1);
        double ymin = ycenter - scale / 2;
        double ymax = ycenter + scale / 2;
        double xmin = xcenter - scale / 2;
        double xmax = xcenter + scale / 2;

        char final_outfile[256];
        if (snprintf(final_outfile, sizeof(final_outfile), "%s_final.jpg", outfile_base) >= sizeof(final_outfile)) {
            fprintf(stderr, "Error: Output filename too long. Truncating.\n");
            exit(EXIT_FAILURE);
        }

        imgRawImage *img = initRawImage(image_width, image_height);                         // Create a raw image of the appropriate size.
        compute_image(img, xmin, xmax, ymin, ymax, max_iterations, num_threads);            // Compute the Mandelbrot image
        storeJpegImageFile(img, final_outfile);                                             // Save the image in the stated file.
        freeRawImage(img);                                                                  // free the mallocs

        printf("Generated final preview image: %s\n", final_outfile);
        exit(0);
    }

    printf("mandelmovie: x=%lf y=%lf xscale=%lf yscale=%lf max=%d images=%d processes=%d threads=%d\n",
           xcenter, ycenter, xscale, yscale, max_iterations, num_images, num_processes, num_threads);

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
                    fprintf(stderr, "Error: Output filename too long for buffer. Truncating.\n");
                    exit(EXIT_FAILURE);
                }

                double ymin = ycenter - scale / 2;
                double ymax = ycenter + scale / 2;
                double xmin = xcenter - scale / 2;
                double xmax = xcenter + scale / 2;

                imgRawImage *img = initRawImage(image_width, image_height);                         // Create a raw image of the appropriate size.
                compute_image(img, xmin, xmax, ymin, ymax, max_iterations, num_threads);            // Compute the Mandelbrot image
                storeJpegImageFile(img, final_outfile);                                             // Save the image in the stated file.
                freeRawImage(img);                                                                  // free the mallocs
                printf("Generated: %s\n", outfile);
            }
            exit(0);
        }
    }
    // Wait for all child processes to complete
    for (int p = 0; p < num_processes; ++p) {
        waitpid(pids[p], NULL, 0);
    }
    printf("All images generated. Use ffmpeg to create the movie:\n");
    printf("ffmpeg -framerate 30 -i %s%%d.jpg -pix_fmt yuv420p mandelzoom.mp4\n", outfile_base);
    return 0;
}

// Thread function to compute a portion of the image
void *compute_image_region(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    for (int j = data->start_row; j < data->end_row; ++j) {
        for (int i = 0; i < data->img->width; ++i) {
            double x = data->xmin + i * (data->xmax - data->xmin) / data->img->width;
            double y = data->ymin + j * (data->ymax - data->ymin) / data->img->height;
            int iters = iterations_at_point(x, y, data->max_iterations);
            setPixelCOLOR(data->img, i, j, iteration_to_color(iters, data->max_iterations));
        }
    }
    return NULL;
}

// Function to compute the image using multi-threading
void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads) {
    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    int rows_per_thread = img->height / num_threads;

    for (int t = 0; t < num_threads; ++t) {
        thread_data[t].img = img;
        thread_data[t].xmin = xmin;
        thread_data[t].xmax = xmax;
        thread_data[t].ymin = ymin;
        thread_data[t].ymax = ymax;
        thread_data[t].max_iterations = max;
        thread_data[t].start_row = t * rows_per_thread;
        thread_data[t].end_row = (t == num_threads - 1) ? img->height : (t + 1) * rows_per_thread;

        pthread_create(&threads[t], NULL, compute_image_region, &thread_data[t]);
    }

    for (int t = 0; t < num_threads; ++t) {
        pthread_join(threads[t], NULL);
    }
}
/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/
int iterations_at_point(double x, double y, int max) {
    double x0 = x, y0 = y;
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

// Convert a iteration number to a color.
int iteration_to_color(int iters, int max) {
    if (iters == max) return 0x000000;
    unsigned char red = (unsigned char)((iters * 7) % 256);
    unsigned char green = (unsigned char)((iters * 13) % 256);
    unsigned char blue = (unsigned char)((iters * 17) % 256);

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
    printf("  -t <threads> Number of threads per image (1-20). Default: 1\n");
    printf("  -P          Preview the final image only.\n");
    printf("  -h          Show help.\n");
}
