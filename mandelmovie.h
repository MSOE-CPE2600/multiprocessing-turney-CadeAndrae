#ifndef MANDELMOVIE_H
#define MANDELMOVIE_H

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>
#include "jpegrw.h"

#define MAX_THREADS 20

typedef struct {
    imgRawImage *img;
    double xmin, xmax, ymin, ymax;
    int max_iterations;
    int start_row, end_row;
} ThreadData;

void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads);
void *compute_image_region(void *arg);
int iterations_at_point(double x, double y, int max);
int iteration_to_color(int iters, int max);
void show_help();

#endif
