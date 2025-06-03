#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t turnoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turnoCond = PTHREAD_COND_INITIALIZER;

pthread_cond_t esperaCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t estacionMutex = PTHREAD_MUTEX_INITIALIZER;

int* capacidadEstaciones;
int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;
int turnoAuto = 1;

char* tareas[] = {"BATERÍA", "MOTOR", "DIRECCIÓN", "SISTEMA DE NAVEGACIÓN"};

void* autoRoutine(void* arg);

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        perror("Faltan argumentos\n");
        return EXIT_FAILURE;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Error al leer el archivo\n");
        return EXIT_FAILURE;
    }

    fscanf(file, "%d", &nAutos);
    fscanf(file, "%d", &nEstaciones);
    fscanf(file, "%d", &capacidadXEstacion);
    fclose(file);

    capacidadEstaciones = malloc(sizeof(int) * nEstaciones);
    for (int i = 0; i < nEstaciones; i++) {
        capacidadEstaciones[i] = capacidadXEstacion;
    }

    pthread_t autos[nAutos];
    srand(time(NULL));

    for (int i = 0; i < nAutos; i++) {
        int* indiceAuto = malloc(sizeof(int));
        *indiceAuto = i + 1;
        pthread_create(&autos[i], NULL, autoRoutine, indiceAuto);
    }

    for (int i = 0; i < nAutos; i++) {
        pthread_join(autos[i], NULL);
    }

    printf("Todos los vehículos han completado su mantenimiento.\n");

    free(capacidadEstaciones);
    return EXIT_SUCCESS;
}

void* autoRoutine(void* arg) {
    int indiceAuto = *(int*)arg;
    free(arg);

    pthread_mutex_lock(&turnoMutex);
    while (indiceAuto != turnoAuto) {
        pthread_cond_wait(&turnoCond, &turnoMutex);
    }
    turnoAuto++;
    pthread_cond_broadcast(&turnoCond);
    pthread_mutex_unlock(&turnoMutex);

    int estacionAsignada = -1;
    pthread_mutex_lock(&estacionMutex);
    while (estacionAsignada < 0) {
        for (int i = 0; i < nEstaciones; i++) {
            if (capacidadEstaciones[i] > 0) {
                capacidadEstaciones[i]--;
                estacionAsignada = i + 1;
                printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n", indiceAuto, estacionAsignada);
                break;
            }
        }
        if (estacionAsignada < 0) {
            printf("Vehículo %d está esperando para ingresar a alguna estación de mantenimiento.\n", indiceAuto);
            pthread_cond_wait(&esperaCond, &estacionMutex);
        }
    }
    pthread_mutex_unlock(&estacionMutex);

    for (int i = 0; i < 4; i++) {
        //sleep(1);

        printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n", indiceAuto, tareas[i], estacionAsignada);
        printf("Vehículo %d ha completado el mantenimiento de la %s en la estación %d.\n", indiceAuto, tareas[i], estacionAsignada);
    }

    printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);

    pthread_mutex_lock(&estacionMutex);
    capacidadEstaciones[estacionAsignada - 1]++;
    pthread_cond_broadcast(&esperaCond);
    pthread_mutex_unlock(&estacionMutex);

    pthread_exit(NULL);
}

double timeDiff(struct timespec start, struct timespec end) {
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return sec + nsec / 1e9;
}
