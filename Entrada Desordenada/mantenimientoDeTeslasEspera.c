// CENTROS DE MANTENIMIENTO DE TESLAS CON ESPERA ACTIVA SIN SEGURO DE ENTRADA ORDENADA

#define _XOPEN_SOURCE 600
#include <pthread.h>   // Para crear y manejar hilos (pthread_create, pthread_join, mutex, etc.)
#include <stdio.h>     // Para printf, perror, fscanf
#include <stdlib.h>    // Para malloc, free, srand, rand, exit
#include <time.h>      // Para srand(time(NULL)), sleep
#include <unistd.h>    // Para usleep, sleep

/* -------- VARIABLES GLOBALES ----------

// Mutex para asegurarnos de que los printf no se mezclen */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Array dinámico que mantiene cuántas plazas libres hay en cada estación
int* capacidadEstaciones;

// Cantidad de autos, cuántas estaciones y capacidad de cada estación
int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;

// Tareas que hará cada auto (solo los nombres, para imprimir)
char* tareas[] = {"BATERÍA", "MOTOR", "DIRECCIÓN", "SISTEMA DE NAVEGACIÓN"};

// Firma de la función que ejecuta cada hilo (cada auto)
void* autoRoutine(void* arg);

int main(int argc, char const* argv[]) {
    // 1) LEER ARGUMENTOS Y ARCHIVO
    // ---------------------------------------------------
    if (argc < 2) {
        perror("Faltan argumentos\n"); // Si no se da el nombre de archivo
        return EXIT_FAILURE;
    }
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Error al leer el archivo\n");
        return EXIT_FAILURE;
    }
    // El archivo debe contener 3 números: nAutos, nEstaciones y capacidadXEstacion
    fscanf(file, "%d", &nAutos);
    fscanf(file, "%d", &nEstaciones);
    fscanf(file, "%d", &capacidadXEstacion);
    fclose(file);

    // 2) INICIALIZAR CAPACIDAD DE ESTACIONES
    // ---------------------------------------------------
    // Reservo un array en heap para llevar la cuenta de plazas libres en cada estación
    capacidadEstaciones = malloc(sizeof(int) * nEstaciones);
    if (!capacidadEstaciones) {
        perror("No se pudo reservar memoria para capacidadEstaciones\n");
        return EXIT_FAILURE;
    }
    // Inicializo cada estación con la capacidad indicada
    for (int i = 0; i < nEstaciones; i++) {
        capacidadEstaciones[i] = capacidadXEstacion;
    }

    // 3) CREAR HILOS (AUTOS)
    // ---------------------------------------------------
    // Reservo dinámicamente el array de pthread_t según nAutos
    pthread_t *autos = (pthread_t*) malloc(sizeof(pthread_t) * nAutos);
    if (!autos) {
        perror("No se pudo reservar memoria para los hilos de autos\n");
        return EXIT_FAILURE;
    }
    srand(time(NULL)); // Semilla para rand (por si se usa luego)

    for (int i = 0; i < nAutos; i++) {
        // Cada hilo necesita saber su "número de auto" (1, 2, 3, ...)
        int* indiceAuto = malloc(sizeof(int));
        if (!indiceAuto) {
            printf("No se pudo guardar memoria para el índice del auto %d\n", i + 1);
            return EXIT_FAILURE;
        }
        *indiceAuto = i + 1;

        // Creo el hilo, que correrá autoRoutine(indiceAuto)
        pthread_create(&autos[i], NULL, autoRoutine, indiceAuto);
    }

    // 4) ESPERAR A QUE TERMINEN TODOS LOS AUTOS
    // ---------------------------------------------------
    for (int i = 0; i < nAutos; i++) {
        pthread_join(autos[i], NULL);
    }

    printf("Todos los vehículos han completado su mantenimiento.\n");

    // 5) LIMPIAR RECURSOS
    // ---------------------------------------------------
    free(capacidadEstaciones);
    free(autos);

    return EXIT_SUCCESS;
}

/* ---------------------------------------------------------
Cada hilo ejecuta esta función: simula a un auto que
quiere ingresar a una estación (espera activa), hace 4 tareas
y luego libera la plaza.
------------------------------------------------------------*/
void* autoRoutine(void* arg) {
    // “indiceAuto” = número del auto (1, 2, 3, ..., nAutos)
    int indiceAuto = *(int*)arg;
    free(arg); // Ya no necesitamos este puntero en el heap

    int estacionAsignada = -1;

    // 1) TRATAR DE ENTRAR A ALGUNA ESTACIÓN
    // ---------------------------------------------------
    // Rutina de espera activa: si no hay plaza en ninguna estación,
    // imprime mensaje y hace usleep(1000) antes de reintentar.
    while (estacionAsignada < 0) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < nEstaciones; i++) {
            if (capacidadEstaciones[i] > 0) {
                // Ocupo una plaza en la estación i
                capacidadEstaciones[i]--;
                estacionAsignada = i + 1;
                printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n",
                        indiceAuto, estacionAsignada);
                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        if (estacionAsignada < 0) {
            // No había lugar en ninguna estación: hago espera activa
            printf("Vehículo %d esperando estación disponible...\n", indiceAuto);
            usleep(1000); // pausar 1 ms antes de reintentar
        }
    }

    // 2) HACER LAS 4 TAREAS DE MANTENIMIENTO
    // ---------------------------------------------------
    for (int i = 0; i < 4; i++) {
        // Espero 1 segundo para simular trabajo
        sleep(1);

        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n",
                indiceAuto, tareas[i], estacionAsignada);
        printf("Vehículo %d ha completado el mantenimiento de la %s en la estación %d.\n",
                indiceAuto, tareas[i], estacionAsignada);
        pthread_mutex_unlock(&mutex);
    }

    // 3) TERMINÓ TODO, LIBERAR PLAZA
    // ---------------------------------------------------
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);
    pthread_mutex_unlock(&mutex);

    // Libero la plaza en la estación para que otro auto pueda usarla
    pthread_mutex_lock(&mutex);
    capacidadEstaciones[estacionAsignada - 1]++;
    pthread_mutex_unlock(&mutex);

    // El hilo finaliza
    pthread_exit(NULL);
}
