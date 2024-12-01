#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <string.h>
// #include <math.h>

#define FALSE 0
#define TRUE 1
typedef int bool;

pthread_mutex_t lock;

typedef struct s_element
{
    int key;
    unsigned char data[96];
} ELEMENT;

typedef struct s_thread_arg
{
    ELEMENT *arr;
    int l;
    int r;
} THREAD_ARG;

int available_threads;

ELEMENT *read_elements_from_file(const char *filename, size_t *count)
{
    FILE *fd = fopen(filename, "rb");

    if (fd == NULL)
    {
        perror("Failed to open file");
        return NULL;
    }

    fseek(fd, 0, SEEK_END);
    size_t file_size = ftell(fd);
    rewind(fd);

    size_t n = file_size / sizeof(ELEMENT);

    if (n == 0)
    {
        fprintf(stderr, "Error: file too small\n");
        fclose(fd);
        return NULL;
    }

    ELEMENT *arr = (ELEMENT *)malloc(n * sizeof(ELEMENT));

    size_t read_count = fread(arr, sizeof(ELEMENT), n, fd);
    if (read_count != n)
    {
        perror("Failed to read structs from file");
        free(arr);
        fclose(fd);
        exit(EXIT_FAILURE);
    }

    *count = n;
    fclose(fd);

    return arr;
}

void print_arr(ELEMENT *arr, int i, int n)
{
    for (; i < n; i++)
    {
        printf("%d ", arr[i].key);
    }
    printf("\n");
}

void *parallel_merge_sort(void *args);

void merge(ELEMENT *arr, int l, int m, int r)
{
    // printf("Merging %d %d %d\n", l, m, r);
    // print_arr(arr, l, r - l + 1);

    int i, j, k;
    int n1 = m - l + 1;
    int n2 = r - m;

    ELEMENT *tmp = (ELEMENT *)malloc(n1 * sizeof(ELEMENT));
    for (i = 0; i < n1; i++)
        tmp[i] = arr[l + i];

    i = 0;
    j = m + 1;
    k = l;

    while (i < n1 && j <= r)
    {
        if ((tmp + i)->key <= (arr + j)->key)
        {
            arr[k++] = tmp[i++];
        }
        else
        {
            arr[k++] = arr[j++];
        }
    }

    while (i < n1)
    {
        arr[k++] = tmp[i++];
    }

    free(tmp);
}

void seq_merge_sort(ELEMENT *arr, int l, int r)
{
    if (l < r)
    {
        int m = l + (r - l) / 2;

        seq_merge_sort(arr, l, m);
        seq_merge_sort(arr, m + 1, r);

        merge(arr, l, m, r);
    }
}

pthread_t next_sort_step(ELEMENT *arr, int l, int r)
{
    bool threaded = FALSE;
    pthread_t tid = 0;

    pthread_mutex_lock(&lock);
    if (available_threads > 0)
    {
        available_threads--;
        threaded = TRUE;
    }
    pthread_mutex_unlock(&lock);

    if (threaded)
    {
        THREAD_ARG *new_args = (THREAD_ARG *)malloc(sizeof(THREAD_ARG));
        new_args->arr = arr;
        new_args->l = l;
        new_args->r = r;

        if (pthread_create(&tid, NULL, parallel_merge_sort, new_args) != 0)
        {
            perror("Failed to create thread");
            free(new_args);
            exit(EXIT_FAILURE);
        }
    }
    else
        seq_merge_sort(arr, l, r);

    return tid;
}

void *parallel_merge_sort(void *args)
{
    THREAD_ARG *arg = (THREAD_ARG *)args;

    if (arg->l < arg->r)
    {
        int m = arg->l + (arg->r - arg->l) / 2;

        pthread_t tid1 = next_sort_step(arg->arr, arg->l, m);
        pthread_t tid2 = next_sort_step(arg->arr, m + 1, arg->r);

        if (tid1 != 0)
        {
            pthread_join(tid1, NULL);
            pthread_mutex_lock(&lock);
            available_threads++;
            pthread_mutex_unlock(&lock);
        }
        if (tid2 != 0)
        {
            pthread_join(tid2, NULL);
            pthread_mutex_lock(&lock);
            available_threads++;
            pthread_mutex_unlock(&lock);
        }

        merge(arg->arr, arg->l, m, arg->r);
    }

    free(args);
    return NULL;
}

void execute(const char *in_name, const char *out_name, int max_threads)
{
    available_threads = max_threads;

    size_t n;
    ELEMENT *arr = read_elements_from_file(in_name, &n);

    THREAD_ARG* args = (THREAD_ARG*)malloc(sizeof(THREAD_ARG));
    args->arr = arr;
    args->l = 0;
    args->r = n - 1;

    pthread_mutex_init(&lock, NULL);

    // printf("Before sort: ");
    // print_arr(arr, 0, n);

    parallel_merge_sort(args);

    // printf("After sort: ");
    // print_arr(arr, 0, n);

    FILE *out = fopen(out_name, "wb");

    for (int i = 0; i < n; i++)
    {
        size_t num_written = fwrite(&arr[i], sizeof(ELEMENT), 1, out);
        if (num_written != 1)
        {
            perror("Error writing to file");
            fclose(out);
            return;
        }
    }

    pthread_mutex_destroy(&lock);

    free(arr);
    fflush(out);
    fclose(out);
}

// int main(int argc, char *argv[])
// {
//     const char *sample_sizes[] = {"10", "100", "1000", "10000", "50000"};
//     const int threads_arr[] = {1, 2, 3, 4, 5, 6, 7, 8};

//     const char *analysis_filename = "analysis.txt";

//     FILE *analysis_out = fopen(analysis_filename, "w");
//     fprintf(analysis_out, "Performance analysis:\n\n");

//     for (int i = 0; i < 5; i++)
//     {

//         const char *sample_size = sample_sizes[i];

//         char filename[255];
//         strcpy(filename, "sample_");
//         strcat(filename, sample_size);
//         strcat(filename, ".dat");

//         char out_filename[255];
//         strcpy(out_filename, "sample_");
//         strcat(out_filename, sample_size);
//         strcat(out_filename, ".out");

//         int n = 100;

//         fprintf(analysis_out, "========================================\n");
//         fprintf(analysis_out, "INPUT LENGTH: %s\n\n", sample_size);
//         for (int j = 0; j < 8; j++)
//         {
//             int threads = threads_arr[j];

//             int runtimes[n];

//             clock_t total_clocks = clock();
//             for (int i = 0; i < n; i++)
//             {
//                 clock_t clocks = clock();
//                 execute(filename, out_filename, threads);
//                 clocks = clock() - clocks;

//                 runtimes[i] = clocks;
//             }

//             total_clocks = clock() - total_clocks;

//             clock_t avg_clocks = total_clocks / n;

//             clock_t sum_dev_clocks = 0;
//             for (int i = 0; i < n; i++)
//             {
//                 sum_dev_clocks += pow(runtimes[i] - avg_clocks, 2);
//             }

//             float std_dev_clock = sqrt((float)sum_dev_clocks / n - 1);
//             float conf_interval_ms = 1.96 * (std_dev_clock / sqrt(n)) * 1000 / CLOCKS_PER_SEC;
//             float avg_ms = (float)avg_clocks * 1000 / CLOCKS_PER_SEC;

//             fprintf(analysis_out, "Threads: %d\n", threads);
//             fprintf(analysis_out, "Average time: %lfms +/- %lf\n", avg_ms, conf_interval_ms);
//             fprintf(analysis_out, "\n");
//         }
//     }

//     return 0;
// }

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <input_file> <output_file> <num_threads>\n", argv[0]);
        return 1;
    }

    int max_threads = atoi(argv[3]);
    if (max_threads == 0)
        max_threads = get_nprocs();

    const char *in_filename = argv[1];
    const char *out_filename = argv[2];

    execute(in_filename, out_filename, max_threads);

    return 0;
}