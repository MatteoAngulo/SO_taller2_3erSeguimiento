// CENTROS DE MANTENIMIENTO DE TESLAS CON VARIABLES CONDICIONALES Y ENTRADA ORDENADA

#define _XOPEN_SOURCE 600
#include <pthread.h>  // Para crear y manejar hilos (pthread_create, pthread_join, mutex, cond, etc.)
#include <stdio.h> // Para printf, perror, fscanf
#include <stdlib.h> // Para malloc, free, srand, rand, exit
#include <time.h>  // Para srand(time(NULL))
#include <unistd.h>  // Para sleep

/* -------- VARIABLES GLOBALES Y CONDICIONALES ----------

// Mutex para que los printf no se mezclen en consola */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex y condicional para controlar el turno de entrada de cada auto
pthread_mutex_t turnoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  turnoCond  = PTHREAD_COND_INITIALIZER;

// Condicional y mutex para que los autos esperen cuando no haya espacio en ninguna estación
pthread_cond_t esperaCond   = PTHREAD_COND_INITIALIZER;
pthread_mutex_t estacionMutex = PTHREAD_MUTEX_INITIALIZER;

// Array dinámico que lleva la cuenta de cuántas plazas libres hay en cada estación
int* capacidadEstaciones;

// Cantidad de autos, cuántas estaciones y capacidad de cada estación
int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;
// Variable que indica el turno actual; arrancamos en 1
int turnoAuto = 1;

// Nombres de las 4 tareas de mantenimiento (solo para imprimir)
char* tareas[] = {"BATERÍA", "MOTOR", "DIRECCIÓN", "SISTEMA DE NAVEGACIÓN"};

// Firma de la función que ejecuta cada hilo (cada auto)
void* autoRoutine(void* arg);

int main(int argc, char const* argv[]) {
    // 1) LEER ARGUMENTOS Y ARCHIVO
    // ---------------------------------------------------
    if (argc < 2) {
        perror("Faltan argumentos\n"); // Si no se da el nombre del archivo
        return EXIT_FAILURE;
    }
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Error al leer el archivo\n");
        return EXIT_FAILURE;
    }
    // El archivo debe contener: nAutos, nEstaciones y capacidadXEstacion
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
    srand(time(NULL)); // Semilla para rand (en caso de usarla luego)

    for (int i = 0; i < nAutos; i++) {
        // Cada hilo necesita saber su "número de auto" (1, 2, 3, ...)
        int* indiceAuto = malloc(sizeof(int));
        if (!indiceAuto) {
            printf("No se pudo reservar memoria para el índice del auto %d\n", i + 1);
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
Cada hilo ejecuta esta función:
1) Espera su turno ordenado para poder iniciar la búsqueda de estación (turnoAuto).
2) Intenta ingresar a una estación; si no hay espacio, espera en esperaCond.
3) Realiza las 4 tareas de mantenimiento (batería, motor, etc.).
4) Libera la plaza y despierta a otros autos en espera antes de terminar.
------------------------------------------------------------*/
void* autoRoutine(void* arg) {
    // “indiceAuto” = número del auto (1, 2, 3, ..., nAutos)
    int indiceAuto = *(int*)arg;
    free(arg); // Ya no necesitamos este puntero en heap

    // 1) ESPERAR SU TURNO ORDENADO
    // ---------------------------------------------------
    pthread_mutex_lock(&turnoMutex);
    while (indiceAuto != turnoAuto) {
        // Si no es su turno, se bloquea en la condicional turnoCond
        pthread_cond_wait(&turnoCond, &turnoMutex);
    }
    // Cuando le toca, avanzo el turno para el siguiente auto
    turnoAuto++;
    // Despierto a todos los hilos que están en esperaCond del turno
    pthread_cond_broadcast(&turnoCond);
    pthread_mutex_unlock(&turnoMutex);

    // 2) TRATAR DE ENTRAR A ALGUNA ESTACIÓN
    // ---------------------------------------------------
    int estacionAsignada = -1;
    pthread_mutex_lock(&estacionMutex);
    while (estacionAsignada < 0) {
        // Recorro todas las estaciones en busca de espacio
        for (int i = 0; i < nEstaciones; i++) {
            if (capacidadEstaciones[i] > 0) {
                // Ocupo una plaza
                capacidadEstaciones[i]--;
                estacionAsignada = i + 1;
                printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n",
                        indiceAuto, estacionAsignada);
                break;
            }
        }
        if (estacionAsignada < 0) {
            // Si no encontré lugar, me bloqueo en esperaCond
            printf("Vehículo %d está esperando para ingresar a alguna estación de mantenimiento.\n",
                    indiceAuto);
            pthread_cond_wait(&esperaCond, &estacionMutex);
            // Aquí el hilo se despierta cuando otro auto libera plaza y hace broadcast de esperaCond
        }
    }
    pthread_mutex_unlock(&estacionMutex);

    // 3) HACER LAS 4 TAREAS DE MANTENIMIENTO
    // ---------------------------------------------------
    for (int i = 0; i < 4; i++) {
        // Inicio de tarea
        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n",
                indiceAuto, tareas[i], estacionAsignada);
        pthread_mutex_unlock(&mutex);

        // Simulo tiempo de trabajo (si se desea, descomentar el sleep)
        // sleep(1);

        // Fin de tarea
        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha completado el mantenimiento de la %s en la estación %d.\n",
                indiceAuto, tareas[i], estacionAsignada);
        pthread_mutex_unlock(&mutex);
    }

    // 4) TERMINÓ TODO, LIBERAR PLAZA Y DESPERTAR A OTROS
    // ---------------------------------------------------
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);
    pthread_mutex_unlock(&mutex);

    // Libero la plaza en la estación para que otro auto la pueda usar
    pthread_mutex_lock(&estacionMutex);
    capacidadEstaciones[estacionAsignada - 1]++;
    // Aviso a todos los hilos que están esperando en esperaCond
    pthread_cond_broadcast(&esperaCond);
    pthread_mutex_unlock(&estacionMutex);

    // El hilo termina
    pthread_exit(NULL);
}