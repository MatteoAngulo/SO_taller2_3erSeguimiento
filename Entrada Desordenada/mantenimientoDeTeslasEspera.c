#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int* capacidadEstaciones;
int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;

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

    pthread_t *autos = (pthread_t *) malloc(sizeof(pthread_t) * nAutos);
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

    int estacionAsignada = -1;
    while (estacionAsignada < 0) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < nEstaciones; i++) {
            if (capacidadEstaciones[i] > 0) {
                capacidadEstaciones[i]--;
                estacionAsignada = i + 1;
                printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n", indiceAuto, estacionAsignada);
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
        if (estacionAsignada < 0) {
            printf("Vehículo %d esperando estación disponible...\n", indiceAuto);
            usleep(1000); // espera activa ligera
        }
    }

    for (int i = 0; i < 4; i++) {
        sleep(1);
        printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n", indiceAuto, tareas[i], estacionAsignada);
        printf("Vehículo %d ha completado el mantenimiento de la %s en la estación %d.\n", indiceAuto, tareas[i], estacionAsignada);
    }

    printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);

    pthread_mutex_lock(&mutex);
    capacidadEstaciones[estacionAsignada - 1]++;
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}

