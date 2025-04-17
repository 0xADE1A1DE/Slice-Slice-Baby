
#include "slice_partitioning/kmeans_wrapper.h"

// Euclidean distance function for kmeans (n-dimensional)
double d_distance(const Pointer a, const Pointer b, int dimensions)
{
    double *pa = (double *)a;
    double *pb = (double *)b;
    double dist = 0.0;
    for (int i = 0; i < dimensions; i++)
    {
        dist += (pa[i] - pb[i]) * (pa[i] - pb[i]);
    }
    return sqrt(dist);
}

// Centroid calculation function for kmeans (n-dimensional)
void d_centroid(const Pointer *objs, const int *clusters, size_t num_objs, int cluster, Pointer centroid, int dimensions)
{
    double *cent = (double *)centroid;
    double *sum = (double *)calloc(dimensions, sizeof(double));
    int count = 0;

    for (size_t i = 0; i < num_objs; i++)
    {
        if (clusters[i] == cluster)
        {
            double *obj = (double *)objs[i];
            for (int j = 0; j < dimensions; j++)
            {
                sum[j] += obj[j];
            }
            count++;
        }
    }

    if (count > 0)
    {
        for (int j = 0; j < dimensions; j++)
        {
            cent[j] = sum[j] / count;
        }
    }

    free(sum);
}

// Wrapper function to pass dimensions to distance method
double distance_wrapper(const Pointer a, const Pointer b)
{
    extern int dimensions;
    return d_distance(a, b, dimensions);
}

// Wrapper function to pass dimensions to centroid method
void centroid_wrapper(const Pointer *objs, const int *clusters, size_t num_objs, int cluster, Pointer centroid)
{
    extern int dimensions;
    d_centroid(objs, clusters, num_objs, cluster, centroid, dimensions);
}

void print_centroids(double *centroids, int n_clusters, int dimensions)
{
    printf("Centroids:\n");
    for (int i = 0; i < n_clusters; i++)
    {
        printf("Cluster %d: ", i);
        for (int j = 0; j < dimensions; j++)
        {
            printf("%lf ", centroids[i * dimensions + j]);
        }
        printf("\n");
    }
}

int predict_slice_index(double *centroids, int *data)
{
    double min_dist = INFINITY;
    int closest_cluster = -1;

    for (int i = 0; i < LLC_SLICES; i++)
    {
        double dist = 0.0;
        for (int j = 0; j < LLC_SLICES; j++)
        {
            dist += (centroids[i * LLC_SLICES + j] - (double)data[j]) * (centroids[i * LLC_SLICES + j] - (double)data[j]);
        }
        dist = sqrt(dist);
        if (dist < min_dist)
        {
            min_dist = dist;
            closest_cluster = i;
        }
    }

    return closest_cluster;
}

// Function to compute and print the average distance between centroids
double average_centroid_distance(double *centroids, int n_clusters, int dimensions)
{
    double total_distance = 0.0;
    int num_distances = 0;
    double min_distance = -1.0;

    for (int i = 0; i < n_clusters; i++)
    {
        for (int j = i + 1; j < n_clusters; j++)
        {
            double dist = d_distance(&centroids[i * dimensions], &centroids[j * dimensions], dimensions);

#if VERBOSITY >= 2
            printf("Distance between centroid %d and %d: %f\n", i, j, dist);
#endif

            // Update the minimum distance if this is the smallest we've encountered
            if (min_distance < 0 || dist < min_distance)
            {
                min_distance = dist;
            }

            total_distance += dist;
            num_distances++;
        }
    }

#if VERBOSITY >= 0
    printf("Minimum centroid distance: %f\n", min_distance);
#endif

    return total_distance / num_distances;
}

int dimensions;

// returns centroids from data collected by running kmeans.
double *get_centroids(int *data, int len)
{
    int cols = LLC_SLICES + 1;
    int rows = len / cols;
    dimensions = cols - 1;
    int n_clusters = cols - 1;

    double *data_temp = (double *)calloc(rows * cols, sizeof(double));

    for (int i = 0; i < rows * cols; ++i)
    {
        data_temp[i] = data[i];
    }

    double *initial_centers = (double *)calloc(n_clusters * dimensions, sizeof(double));

    // Compute initial cluster centers
    for (int i = 0; i < n_clusters; i++)
    {
        for (int j = 0; j < dimensions; j++)
        {
            initial_centers[i * dimensions + j] = 0.0;
        }
    }

    // Compute means for initial centers
    int *counts = (int *)calloc(n_clusters, sizeof(int));
    for (int i = 0; i < n_clusters; i++)
    {
        counts[i] = 0;
    }
    for (int i = 0; i < rows; i++)
    {
        int real_slice = (int)data_temp[i * cols];
        for (int j = 0; j < dimensions; j++)
        {
            initial_centers[real_slice * dimensions + j] += data_temp[i * cols + j + 1];
        }
        counts[real_slice]++;
    }
    for (int i = 0; i < n_clusters; i++)
    {
        for (int j = 0; j < dimensions; j++)
        {
            initial_centers[i * dimensions + j] /= counts[i];
        }
    }

    double avg_distance = average_centroid_distance(initial_centers, n_clusters, dimensions);
    printf("Average centroid distance: %f\n", avg_distance);

    if (avg_distance <= ((double)MEASURE_SAMPLES * 0.7))
    {
        fprintf(stderr, "get_centroids(): Calibration error, centroids too close (%f), exiting\n", avg_distance);
        exit(1);
    }

    free(counts);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // If you wish you can uncomment the below to get the initial centroids / profile vector using kmeans, but I found taking an average was fine.
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // kmeans_config config;
    // config.num_objs = rows;
    // config.k = n_clusters;
    // config.max_iterations = 100;
    // config.distance_method = distance_wrapper;
    // config.centroid_method = centroid_wrapper;

    // config.objs = (Pointer *)calloc(config.num_objs, sizeof(Pointer));
    // config.centers = (Pointer *)calloc(config.k, sizeof(Pointer));
    // config.clusters = (int *)calloc(config.num_objs, sizeof(int));

    // // Populate objs
    // for (size_t i = 0; i < config.num_objs; i++)
    // {
    //     config.objs[i] = &(data_temp[i * cols + 1]);
    // }

    // // Populate centroids
    // for (size_t i = 0; i < config.k; i++)
    // {
    //     config.centers[i] = &(initial_centers[i * dimensions]);
    // }

    // // Run kmeans
    // kmeans_result result = kmeans(&config);

    // if (result != KMEANS_OK)
    // {
    //     fprintf(stderr, "get_centroids(): KMeans algorithm did not converge.\n");
    //     exit(1);
    // }

    // // Free memory
    // free(data_temp);
    // free(config.objs);
    // free(config.clusters);
    // free(config.centers);

    return initial_centers;
}