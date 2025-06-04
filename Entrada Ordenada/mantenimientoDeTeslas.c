// CENTROS DE MANTENIMIENTO DE TESLAS CON SEMÁFOROS CON ENTRADA ORDENADA

#define _XOPEN_SOURCE 600
#include <pthread.h>  // Para crear y manejar hilos (pthread_create, pthread_join, mutex, cond, etc.)
#include <semaphore.h> // Para usar semáforos (sem_init, sem_wait, sem_post)
#include <stdio.h>   // Para printf, perror, fscanf
#include <stdlib.h> // Para malloc, free, srand, rand, exit
#include <time.h>  // Para srand(time(NULL))
//#include <unistd.h>  // Para el sleep() si se desea simular trabajo

/* -------- VARIABLES GLOBALES Y SEMÁFOROS ----------

// Mutex para asegurarnos de que los printf no se mezclen en pantalla */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex y condicional para controlar orden de entrada de los autos
pthread_mutex_t turnoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  turnoCond  = PTHREAD_COND_INITIALIZER;

// Array dinámico de semáforos: uno por cada estación de mantenimiento
sem_t* semasforosEstacion;
// Semáforo para poner a los autos a "esperar" hasta que haya lugar
sem_t semasEsperaAutos;

// Cantidad de autos, cuántas estaciones y capacidad de cada estación
int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;
// Variable que indica el turno actual (comienza en 1)
int turnoAuto = 1;

// Tareas que hará cada auto (solo los nombres, para imprimir)
char* tareas[] = {"BATERÍA", "MOTOR", "DIRECCIÓN", "SISTEMA DE NAVEGACIÓN"};

// Firma de la función que ejecuta cada hilo (cada auto)
void* autoRoutine(void* arg);

int main(int argc, char const* argv[]) {

  // 1) LEER ARGUMENTOS Y ARCHIVO
  // ---------------------------------------------------
  if (argc < 2) {
    perror("Faltan argumentos\n"); // Si no se proporciona el nombre de archivo
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

  // 2) INICIALIZAR SEMÁFOROS
  // ---------------------------------------------------
  // Reservamos un array de semáforos del tamaño de nEstaciones
  semasforosEstacion = (sem_t*)malloc(sizeof(sem_t) * nEstaciones);
  if (!semasforosEstacion) {
    perror("No se pudo reservar memoria para los semáforos de las estaciones\n");
    return EXIT_FAILURE;
  }

  // Semáforo que usarán los autos para quedarse en cola general
  sem_init(&semasEsperaAutos, 0, 0);

  // Inicializamos cada semáforo de estación con la capacidad definida
  for (int i = 0; i < nEstaciones; i++) {
    sem_init(&semasforosEstacion[i], 0, capacidadXEstacion);
  }

  // 3) CREAR HILOS (AUTOS)
  // ---------------------------------------------------
  // Reservamos dinámicamente el array de pthread_t según nAutos
  pthread_t *autos = (pthread_t*)malloc(sizeof(pthread_t) * nAutos);
  if (!autos) {
    perror("No se pudo reservar memoria para los hilos de autos\n");
    return EXIT_FAILURE;
  }

  srand(time(NULL)); // Semilla para rand (por si se usa más adelante)

  for (int i = 0; i < nAutos; i++) {
    // Cada hilo necesita un puntero a su número de auto (del 1 al nAutos)
    int* indiceAuto = malloc(sizeof(int));
    if (!indiceAuto) {
      printf("No se pudo guardar memoria para el índice del auto %d\n", i + 1);
      return EXIT_FAILURE;
    }
    *indiceAuto = i + 1;

    // Creamos el hilo: correrá autoRoutine(indiceAuto)
    pthread_create(&autos[i], NULL, autoRoutine, (void*)indiceAuto);
  }

  // 4) ESPERAR A QUE TERMINEN TODOS LOS AUTOS
  // ---------------------------------------------------
  for (int i = 0; i < nAutos; i++) {
    pthread_join(autos[i], NULL);
  }

  printf("Todos los vehículos han completado su mantenimiento y están listos para volver a la carretera\n");

  // 5) LIMPIAR RECURSOS
  // ---------------------------------------------------
  for (int i = 0; i < nEstaciones; i++) {
    sem_destroy(&semasforosEstacion[i]);
  }
  free(semasforosEstacion);
  free(autos);

  return EXIT_SUCCESS;
}

/* ---------------------------------------------------------
Cada hilo ejecuta esta función: 
1) Espera su turno ordenado para comenzar (turnoAuto).
2) Trata de ingresar a una estación (uso de sem_trywait/sem_wait).
3) Realiza las 4 tareas de mantenimiento.
4) Libera la estación y despierta a un auto en espera.
------------------------------------------------------------*/
void* autoRoutine(void* arg) {
  // “indiceAuto” = número del auto (1, 2, 3, ..., nAutos)
  int indiceAuto = *(int*)arg;
  free(arg); // Ya no necesitamos este puntero

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
  // Despierta a todos los hilos que están esperando el turno
  pthread_cond_broadcast(&turnoCond);
  pthread_mutex_unlock(&turnoMutex);

  // 2) TRATAR DE ENTRAR A ALGUNA ESTACIÓN
  // ---------------------------------------------------
  int estacionAsignada = -1;
  while (estacionAsignada < 0) {
    // Intentamos sin bloquearnos a entrar a cualquier estación disponible
    for (int i = 0; i < nEstaciones; ++i) {
      if (sem_trywait(&semasforosEstacion[i]) == 0) {
        // Entró a la estación i+1
        estacionAsignada = i + 1;
        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n",
                indiceAuto, estacionAsignada);
        pthread_mutex_unlock(&mutex);
        break;
      }
    }
    if (estacionAsignada < 0) {
      // Si no encontró lugar, imprime mensaje y se bloquea en sem_wait general
      pthread_mutex_lock(&mutex);
      printf("Vehículo %d está esperando para ingresar a alguna estación de mantenimiento.\n",
              indiceAuto);
      pthread_mutex_unlock(&mutex);
      sem_wait(&semasEsperaAutos);
    }
  }

  // 3) HACER LAS 4 TAREAS DE MANTENIMIENTO
  // ---------------------------------------------------
  for (int i = 0; i < 4; i++) {
    // Inicio de tarea
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n",
            indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);

    // Simula tiempo de trabajo con sleep (se puede descomentar si se desea)
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

  // Libera la plaza en la estación
  sem_post(&semasforosEstacion[estacionAsignada - 1]);
  // Despierta a un auto que esté esperando en la cola general
  sem_post(&semasEsperaAutos);

  pthread_exit(NULL);
}
