// CENTROS DE MANTENIMIENTO DE TESLAS CON BARRERAS Y ESPERA ACTIVA SIN SEGURO DE ENTRADA ORDENADA

#define _XOPEN_SOURCE 600
#include <pthread.h> // Para crear y manejar hilos (pthread_create, pthread_join, mutex, barrier, etc.)
#include <stdio.h> // Para printf, perror, fscanf
#include <stdlib.h> // Para malloc, free, srand, rand, exit
#include <time.h> // Para srand(time(NULL)), sleep
#include <unistd.h> // Para usleep, sleep

// ------- VARIABLES GLOBALES Y BARRERA ----------


// Mutex para que los printf no se mezclen en consola
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Barrera que sincroniza a todos los autos al final
pthread_barrier_t barrera;

// Cantidad de autos, cuántas estaciones y capacidad de cada estación
int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;

// Array dinámico que lleva la cuenta de cuántas plazas quedan en cada estación
int* capacidadEstaciones;

// Nombres de las 4 tareas de mantenimiento (solo para imprimir)
char* tareas[] = {"BATERÍA", "MOTOR", "DIRECCIÓN", "SISTEMA DE NAVEGACIÓN"};

// Firma de la función que ejecuta cada hilo (cada auto)
void* autoRoutine(void* arg);

int main(int argc, char const* argv[]) {
  // 1) LEER ARGUMENTOS Y ARCHIVO
  // ---------------------------------------------------
  if (argc < 2) {
    perror("Faltan argumentos \n"); // Si no se da el archivo con los números
    return EXIT_FAILURE;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    perror("Error al leer el archivo \n");
    return EXIT_FAILURE;
  }
  // El archivo debe contener: nAutos, nEstaciones y capacidadXEstacion (en ese orden)
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
      printf("No se pudo guardar memoria para el índice del auto %d\n", i + 1);
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
Cada hilo ejecuta esta función: simula a un auto que
intenta ingresar a una estación (con espera activa), 
hace 4 tareas y luego espera en barrera.
------------------------------------------------------------*/

void* autoRoutine(void* arg) {
  // “indiceAuto” = número del auto (1, 2, 3, ..., nAutos)
  int indiceAuto = *(int*)arg;
  free(arg); // Ya no necesitamos este puntero en heap

  int estacionAsignada = -1;


  // 1) TRATAR DE ENTRAR A ALGUNA ESTACIÓN
  // ---------------------------------------------------
  // Rutina de espera activa: si ninguna estación tiene plaza, hago usleep(100ms) y pruebo otra vez.
  while (estacionAsignada < 0) {
    pthread_mutex_lock(&mutex);
    // Recorro todas las estaciones y busco una con espacio
    for (int i = 0; i < nEstaciones; ++i) {
      if (capacidadEstaciones[i] > 0) {
        // Si la encuentro, “ocupo” una plaza y me asigno a esa estación
        capacidadEstaciones[i]--;
        estacionAsignada = i + 1;
        printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n",
                indiceAuto, estacionAsignada);
        break;
      }
    }
    if (estacionAsignada < 0) {
      // Si no había lugar, imprimo que espero y luego bloqueo el mutex antes de salir
      printf("Vehículo %d está esperando para ingresar a una estación de mantenimiento.\n",
              indiceAuto);
    }
    pthread_mutex_unlock(&mutex);

    if (estacionAsignada < 0) {
      // Espera activa: duermo 100 milisegundos antes de volver a intentar
      usleep(100000);
    }
  }


  // 2) HACER LAS 4 TAREAS DE MANTENIMIENTO
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

  // 3) TERMINÓ TODO, LIBERAR PLaza y ESPERAR EN BARRERA
  // ---------------------------------------------------
  pthread_mutex_lock(&mutex);
  printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);
  // Libero la plaza en la estación para que otro auto la pueda usar
  capacidadEstaciones[estacionAsignada - 1]++;
  pthread_mutex_unlock(&mutex);

  // Espero en la barrera hasta que todos los autos terminen sus tareas
  pthread_barrier_wait(&barrera);

  // El hilo sale y termina (pthread_join en main lo recogerá)
  pthread_exit(NULL);
}
