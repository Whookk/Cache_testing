#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <threads.h>
#include <stdatomic.h>
#include <string.h>
#include <bits/getopt_core.h>

#define CACHE_LINE_SIZE 64
#define DEFAULT_ARRAY_SIZE 1000
#define DEFAULT_THREAD_COUNT 2

typedef int elem_t;
typedef elem_t *array_t;

typedef struct {
    double start;
    double end;
    size_t sum;
} benchmark_result;

typedef struct {
    size_t cache_line_size;
    size_t array_size;
    size_t thread_count;
} config;

typedef struct {
    array_t array;
    size_t array_size;
    benchmark_result results;
} thread_data;

double current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec) * 1000000.0 + (double)(tv.tv_usec);
}

void parse_arguments(config *cfg, int argc, char **argv) {
    int opt;
    cfg->cache_line_size = CACHE_LINE_SIZE;
    cfg->array_size = DEFAULT_ARRAY_SIZE;
    cfg->thread_count = DEFAULT_THREAD_COUNT;

    while ((opt = getopt(argc, argv, "t:c:a:")) != -1) {
        switch (opt) {
            case 't': cfg->thread_count = atoi(optarg); break;
            case 'c': cfg->cache_line_size = atoi(optarg); break;
            case 'a': cfg->array_size = atoi(optarg); break;
            default: break;
        }
    }
}

array_t initialize_array(size_t array_size, elem_t default_value) {
    array_t array = malloc(array_size * sizeof(elem_t));
    if (!array) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < array_size; ++i) {
        array[i] = default_value;
    }
    return array;
}

void display_results(const benchmark_result *res) {
    double latency = (res->end - res->start) / 1000.0;
    printf("Sum:           %ld\n", res->sum);
    printf("Latency:       %.3f ms\n", latency);
}

int sequential_access(config *cfg) {
    array_t array = initialize_array(cfg->array_size, 1);
    benchmark_result res = { .start = current_time_us(), .sum = 0 };

    for (size_t i = 0; i < cfg->array_size; ++i) {
        res.sum += array[i];
    }

    res.end = current_time_us();
    display_results(&res);
    free(array);
    return res.sum;
}

int random_access(config *cfg) {
    array_t array = initialize_array(cfg->array_size, 1);
    size_t *indexes = malloc(cfg->array_size * sizeof(size_t));
    for (size_t i = 0; i < cfg->array_size; i++) {
        indexes[i] = rand() % cfg->array_size;
    }

    benchmark_result res = { .start = current_time_us(), .sum = 0 };
    for (size_t i = 0; i < cfg->array_size; ++i) {
        res.sum += array[indexes[i]];
    }

    res.end = current_time_us();
    display_results(&res);
    free(array);
    free(indexes);
    return res.sum;
}

int cache_miss_access(config *cfg) {
    size_t step = cfg->cache_line_size / sizeof(elem_t);
    size_t effective_size = cfg->array_size * step;
    array_t array = initialize_array(effective_size, 1);

    benchmark_result res = { .start = current_time_us(), .sum = 0 };
    for (size_t i = 0; i < effective_size; i += step) {
        res.sum += array[i];
    }

    res.end = current_time_us();
    display_results(&res);
    free(array);
    return res.sum;
}

atomic_int atomic_sum = 0;

int atomic_add(void *arg) {
    thread_data *data = (thread_data *)arg;
    data->results.start = current_time_us();

    for (size_t i = 0; i < data->array_size; ++i) {
        atomic_fetch_add_explicit(&atomic_sum, data->array[i], memory_order_relaxed);
    }

    data->results.end = current_time_us();
    return 0;
}

int atomic_access(config *cfg) {
    array_t array = initialize_array(cfg->array_size, 1);
    thrd_t threads[cfg->thread_count];
    thread_data data[cfg->thread_count];
    size_t partition_size = cfg->array_size / cfg->thread_count;

    for (size_t i = 0; i < cfg->thread_count; ++i) {
        data[i].array = array;
        data[i].array_size = partition_size;
        thrd_create(&threads[i], atomic_add, &data[i]);
    }

    for (size_t i = 0; i < cfg->thread_count; ++i) {
        thrd_join(threads[i], NULL);
    }

    benchmark_result res = { .start = data[0].results.start, .end = data[0].results.end, .sum = atomic_sum };
    for (size_t i = 1; i < cfg->thread_count; ++i) {
        if (res.start > data[i].results.start) res.start = data[i].results.start;
        if (res.end < data[i].results.end) res.end = data[i].results.end;
    }

    display_results(&res);
    free(array);
    return atomic_sum;
}

int race_condition(config *cfg) {
    int sum = 0;
    thrd_t threads[cfg->thread_count];
    thread_data data[cfg->thread_count];
    size_t partition_size = cfg->array_size / cfg->thread_count;

    int race_add(void *arg) {
        thread_data *data = (thread_data *)arg;
        data->results.start = current_time_us();

        for (size_t i = 0; i < data->array_size; ++i) {
            sum++;
        }

        data->results.end = current_time_us();
        return 0;
    }

    for (size_t i = 0; i < cfg->thread_count; ++i) {
        data[i].array_size = partition_size;
        thrd_create(&threads[i], race_add, &data[i]);
    }

    for (size_t i = 0; i < cfg->thread_count; ++i) {
        thrd_join(threads[i], NULL);
    }

    benchmark_result res = { .start = data[0].results.start, .end = data[0].results.end, .sum = sum };
    for (size_t i = 1; i < cfg->thread_count; ++i) {
        if (res.start > data[i].results.start) res.start = data[i].results.start;
        if (res.end < data[i].results.end) res.end = data[i].results.end;
    }

    display_results(&res);
    return sum;
}

int volatile_access(config *cfg) {
    volatile size_t sum = 0;
    array_t array = initialize_array(cfg->array_size, 1);

    benchmark_result res = { .start = current_time_us(), .sum = 0 };
    for (size_t i = 0; i < cfg->array_size; ++i) {
        sum += array[i];
    }

    res.end = current_time_us();
    res.sum = sum;
    display_results(&res);
    free(array);
    return sum;
}

int main(int argc, char **argv) {
    config cfg;
    parse_arguments(&cfg, argc, argv);

    printf("\nSequential Access:\n");
    sequential_access(&cfg);

    printf("\nRandom Access:\n");
    random_access(&cfg);

    printf("\nCache Miss Access:\n");
    cache_miss_access(&cfg);

    printf("\nAtomic Access:\n");
    atomic_access(&cfg);

    printf("\nRace Condition:\n");
    race_condition(&cfg);

    printf("\nVolatile Access:\n");
    volatile_access(&cfg);

    return 0;
}
