#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#define MATRIX_SIZE 512
#define NUM_THREADS 4
#define ARRAY_SIZE 1000000


volatile LONG shared_var = 0;
volatile LONG atomic_shared_var = 0;

DWORD WINAPI race_condition_demo(LPVOID arg) {
    for (int i = 0; i < 1000000; i++) {
        shared_var++;
        InterlockedIncrement(&atomic_shared_var);
    }
    return 0;
}


void sequential_access(int *array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        array[i] = (int)i;
    }
}


void random_access(int *array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        size_t index = rand() % size;
        array[index] = (int)i;
    }
}


void matrix_multiplication_cache(int (*a)[MATRIX_SIZE], int (*b)[MATRIX_SIZE], int (*c)[MATRIX_SIZE]) {
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            c[i][j] = 0;
            for (int k = 0; k < MATRIX_SIZE; k++) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}


void matrix_multiplication_simple(int (*a)[MATRIX_SIZE], int (*b)[MATRIX_SIZE], int (*c)[MATRIX_SIZE]) {
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            c[i][j] = 0;
            for (int k = 0; k < MATRIX_SIZE; k++) {
                c[i][j] += a[i][k] * b[j][k]; 
            }
        }
    }
}

int main() {
 
    int *array = (int *)malloc(ARRAY_SIZE * sizeof(int));
    if (!array) {
        perror("Failed to allocate memory");
        return 1;
    }

    clock_t start, end;

 
    start = clock();
    sequential_access(array, ARRAY_SIZE);
    end = clock();
    printf("Sequential access time: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

  
    start = clock();
    random_access(array, ARRAY_SIZE);
    end = clock();
    printf("Random access time: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    free(array);

  
    int (*a)[MATRIX_SIZE] = malloc(sizeof(int[MATRIX_SIZE][MATRIX_SIZE]));
    int (*b)[MATRIX_SIZE] = malloc(sizeof(int[MATRIX_SIZE][MATRIX_SIZE]));
    int (*c_cache)[MATRIX_SIZE] = malloc(sizeof(int[MATRIX_SIZE][MATRIX_SIZE]));
    int (*c_simple)[MATRIX_SIZE] = malloc(sizeof(int[MATRIX_SIZE][MATRIX_SIZE]));

    if (!a || !b || !c_cache || !c_simple) {
        perror("Failed to allocate memory for matrices");
        return 1;
    }

    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            a[i][j] = rand() % 100;
            b[i][j] = rand() % 100;
        }
    }

    
    start = clock();
    matrix_multiplication_cache(a, b, c_cache);
    end = clock();
    printf("Matrix multiplication (cache-intensive) time: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    
    start = clock();
    matrix_multiplication_simple(a, b, c_simple);
    end = clock();
    printf("Matrix multiplication (simple) time: %f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    free(a);
    free(b);
    free(c_cache);
    free(c_simple);

    
    HANDLE threads[NUM_THREADS];

    shared_var = 0;
    atomic_shared_var = 0;

   
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = CreateThread(NULL, 0, race_condition_demo, NULL, 0, NULL);
        if (threads[i] == NULL) {
            perror("Failed to create thread");
            return 1;
        }
    }

   
    WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);

    
    for (int i = 0; i < NUM_THREADS; i++) {
        CloseHandle(threads[i]);
    }

    
    printf("Shared variable (without atomic): %ld\n", shared_var);
    printf("Shared variable (atomic): %ld\n", atomic_shared_var);

    return 0;
}
