// CENTROS DE MANTENIMIENTO DE TESLAS CON VARIABLES CONDICIONALES SIN SEGURO DE ENTRADA ORDENADA

#define _XOPEN_SOURCE 600
#include <pthread.h> // Para crear y manejar hilos (pthread_create, pthread_join, mutex, cond, etc.)
#include <stdio.h> // Para printf, perror, fscanf
#include <stdlib.h> // Para malloc, free, srand, rand, exit
#include <time.h> // Para srand(time(NULL))
#include <unistd.h> // Para sleep

/* -------- VARIABLES GLOBALES Y CONDICIONALES ----------

Mutex para asegurarnos de que los printf no se mezclen */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Condicional para que los autos esperen cuando no haya espacio
pthread_cond_t esperaCond = PTHREAD_COND_INITIALIZER;
// Mutex que protege el acceso a capacidadEstaciones
pthread_mutex_t estacionMutex = PTHREAD_MUTEX_INITIALIZER;

// Array dinámico que lleva la cuenta de cuántas plazas quedan en cada estación
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
        perror("Faltan argumentos\n"); // Si no se proporciona el archivo con los números
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
    srand(time(NULL)); // Semilla para rand (en caso de usarlo después)

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
quiere ingresar a una estación (usando condicional para esperar),
hace 4 tareas y luego libera la plaza y despierta a otros.
------------------------------------------------------------*/
void* autoRoutine(void* arg) {
    // “indiceAuto” = número del auto (1, 2, 3, ..., nAutos)
    int indiceAuto = *(int*)arg;
    free(arg); // Ya no necesitamos este puntero en el heap

    int estacionAsignada = -1;

    // 1) TRATAR DE ENTRAR A ALGUNA ESTACIÓN
    // ---------------------------------------------------
    // Rutina de espera con variable condicional:
    // Si no hay espacio en ninguna estación, espera hasta que le avisen
    pthread_mutex_lock(&estacionMutex);
    while (estacionAsignada < 0) {
        // Recorro todas las estaciones y busco una con plaza libre
        for (int i = 0; i < nEstaciones; i++) {
            if (capacidadEstaciones[i] > 0) {
                // Si la encuentro, "ocupo" una plaza y me asigno a esa estación
                capacidadEstaciones[i]--;
                estacionAsignada = i + 1;
                printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n",
                        indiceAuto, estacionAsignada);
                break;
            }
        }
        if (estacionAsignada < 0) {
            // Si no había lugar, imprimo que espero y me bloqueo en la condicional
            printf("Vehículo %d está esperando para ingresar a alguna estación de mantenimiento.\n",
                    indiceAuto);
            pthread_cond_wait(&esperaCond, &estacionMutex);
            // Aquí el hilo se bloquea hasta que alguien haga pthread_cond_broadcast
            // cuando se libere una plaza en cualquier estación
        }
    }
    pthread_mutex_unlock(&estacionMutex);

    // 2) HACER LAS 4 TAREAS DE MANTENIMIENTO
    // ---------------------------------------------------
    for (int i = 0; i < 4; i++) {
        // Inicio de tarea
        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n",
                indiceAuto, tareas[i], estacionAsignada);
        pthread_mutex_unlock(&mutex);

        // Simulo tiempo de trabajo (si quieres, descomenta el sleep)
        // sleep(1);

        // Fin de tarea
        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha completado el mantenimiento de la %s en la estación %d.\n",
                indiceAuto, tareas[i], estacionAsignada);
        pthread_mutex_unlock(&mutex);
    }

    // 3) TERMINÓ TODO, LIBERAR PLAZA Y DESPERTAR A OTROS
    // ---------------------------------------------------
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);
    pthread_mutex_unlock(&mutex);

    // Libero la plaza en la estación para que otro auto la pueda usar
    pthread_mutex_lock(&estacionMutex);
    capacidadEstaciones[estacionAsignada - 1]++;
    // Aviso a todos los que estén esperando que puede haber espacio ahora
    pthread_cond_broadcast(&esperaCond);
    pthread_mutex_unlock(&estacionMutex);

    // El hilo termina
    pthread_exit(NULL);
}
