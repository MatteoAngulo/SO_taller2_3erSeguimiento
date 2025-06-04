// CENTROS DE MANTENIMIENTO DE TESLAS CON BARRERAS, ESPERA ACTIVA Y ENTRADA ORDENADA

#define _XOPEN_SOURCE 600
#include <pthread.h> // Para crear y manejar hilos (pthread_create, pthread_join, mutex, cond, barrier, etc.)
#include <stdio.h> // Para printf, perror, fscanf
#include <stdlib.h> // Para malloc, free, srand, rand, exit
#include <time.h> // Para srand(time(NULL))
#include <unistd.h> // Para usleep, sleep

// ------- VARIABLES GLOBALES, TURNO Y BARRERA ----------

// Mutex para que los printf no se mezclen en consola
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex y condicional para controlar el orden de entrada de los autos
pthread_mutex_t turnoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  turnoCond  = PTHREAD_COND_INITIALIZER;

// Barrera que sincroniza a todos los autos cuando terminan su mantenimiento
pthread_barrier_t barrera;

// Cantidad de autos, cuántas estaciones y capacidad de cada estación
int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;
// Variable que indica el turno actual (comienza en 1)
int turnoAuto = 1;

// Array dinámico que lleva la cuenta de cuántas plazas libres hay en cada estación
int* capacidadEstaciones;

// Nombres de las 4 tareas de mantenimiento (solo para imprimir)
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
  // Reservo un array en heap para llevar la cuenta de cuántas plazas libres hay en cada estación
  capacidadEstaciones = (int*)malloc(sizeof(int) * nEstaciones);
  if (!capacidadEstaciones) {
    perror("No se pudo reservar memoria para capacidadEstaciones\n");
    return EXIT_FAILURE;
  }
  // Inicializo cada estación con la capacidad indicada
  for (int i = 0; i < nEstaciones; i++) {
    capacidadEstaciones[i] = capacidadXEstacion;
  }

  // 3) INICIALIZAR BARRERA
  // ---------------------------------------------------
  // La barrera esperará hasta que todos los nAutos lleguen al final de su rutina
  pthread_barrier_init(&barrera, NULL, nAutos);

  // 4) CREAR HILOS (AUTOS)
  // ---------------------------------------------------
  // Reservo dinámicamente el array de pthread_t según nAutos
  pthread_t *autos = (pthread_t*) malloc(sizeof(pthread_t) * nAutos);
  if (!autos) {
    perror("No se pudo reservar memoria para los hilos de autos\n");
    return EXIT_FAILURE;
  }
  srand(time(NULL)); // Semilla para rand (aunque en este código no se usa rand, queda por si se añade)

  for (int i = 0; i < nAutos; i++) {
    // Cada hilo necesita saber su "número de auto" (1, 2, 3, ...)
    int* indiceAuto = malloc(sizeof(int));
    if (!indiceAuto) {
      printf("No se pudo reservar memoria para el auto %d\n", i + 1);
      return EXIT_FAILURE;
    }
    *indiceAuto = i + 1;

    // Creo el hilo, que correrá autoRoutine(indiceAuto)
    pthread_create(&autos[i], NULL, autoRoutine, indiceAuto);
  }

  // 5) ESPERAR A QUE TERMINEN TODOS LOS AUTOS
  // ---------------------------------------------------
  for (int i = 0; i < nAutos; i++) {
    pthread_join(autos[i], NULL);
  }

  printf("Todos los vehículos han completado su mantenimiento y están listos para volver a la carretera\n");

  // 6) LIMPIAR RECURSOS
  // ---------------------------------------------------
  pthread_barrier_destroy(&barrera);
  free(capacidadEstaciones);
  free(autos);

  return EXIT_SUCCESS;
}

/* ---------------------------------------------------------
Cada hilo ejecuta esta función:
1) Espera su turno ordenado para comenzar (turnoAuto).
2) Trata de ingresar a una estación (espera activa si no hay plazas).
3) Realiza las 4 tareas de mantenimiento.
4) Libera la plaza y espera en la barrera junto a los demás.
------------------------------------------------------------*/
void* autoRoutine(void* arg) {
  // “indiceAuto” = número del auto (1, 2, 3, ..., nAutos)
  int indiceAuto = *(int*)arg;
  free(arg); // Ya no necesitamos este puntero en heap

  // 1) ESPERAR SU TURNO ORDENADO
  // ---------------------------------------------------
  pthread_mutex_lock(&turnoMutex);
  while (indiceAuto != turnoAuto) {
    // Si no es su turno, se bloquea en la condición
    pthread_cond_wait(&turnoCond, &turnoMutex);
  }
  // Una vez es su turno, avanza el contador para el siguiente auto
  pthread_mutex_lock(&mutex);
  turnoAuto++;
  pthread_mutex_unlock(&mutex);
  // Despierta a todos los hilos que están esperando su turno
  pthread_cond_broadcast(&turnoCond);
  pthread_mutex_unlock(&turnoMutex);

  // 2) TRATAR DE ENTRAR A ALGUNA ESTACIÓN
  // ---------------------------------------------------
  int estacionAsignada = -1;
  while (estacionAsignada < 0) {
    pthread_mutex_lock(&mutex);
    // Recorro todas las estaciones y busco una con espacio
    for (int i = 0; i < nEstaciones; ++i) {
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
      // Si no había lugar, imprimo que espero antes de salir del mutex
      printf("Vehículo %d está esperando para ingresar a una estación de mantenimiento.\n",
              indiceAuto);
    }
    pthread_mutex_unlock(&mutex);

    if (estacionAsignada < 0) {
      // Espera activa: duermo 100 milisegundos antes de volver a intentar
      usleep(100000);
    }
  }

  // 3) HACER LAS 4 TAREAS DE MANTENIMIENTO
  // ---------------------------------------------------
  for (int i = 0; i < 4; i++) {
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n",
            indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);

    // Simulo tiempo de trabajo con sleep(1) segundo
    sleep(1);

    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha completado el mantenimiento de la %s en la estación %d.\n",
            indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);
  }

  // 4) TERMINÓ TODO, LIBERAR PLAZA Y ESPERAR EN BARRERA
  // ---------------------------------------------------
  pthread_mutex_lock(&mutex);
  printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);
  // Libero la plaza en la estación para que otro auto la pueda usar
  capacidadEstaciones[estacionAsignada - 1]++;
  pthread_mutex_unlock(&mutex);

  // Espero en la barrera hasta que todos los autos terminen sus tareas
  pthread_barrier_wait(&barrera);

  // El hilo termina
  pthread_exit(NULL);
}
