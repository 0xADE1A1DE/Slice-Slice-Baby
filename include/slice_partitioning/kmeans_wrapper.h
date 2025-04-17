#ifndef __KMEANS_WRAPPER__
#define __KMEANS_WRAPPER__

#include "../config.h"
#include "../util/util.h"

typedef void *Pointer;

int predict_slice_index(double *centroids, int *data);
double *get_centroids(int *data, int len);

#endif //__KMEANS_WRAPPER__