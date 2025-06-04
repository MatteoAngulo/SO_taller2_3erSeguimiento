
// CENTROS DE MANTENIMIENTO DE TESLAS CON SEMÁSFOROS SIN SEGURO DE ENTRADA ORDENADA

#define _XOPEN_SOURCE 600
#include <pthread.h> // Para crear y manejar hilos (pthread_create, pthread_join, mutex, etc.)
#include <semaphore.h> // Para usar semáforos (sem_init, sem_wait, sem_post)
#include <stdio.h> // Para printf, perror, fscanf
#include <stdlib.h> // Para malloc, free, srand, rand, exit
#include <time.h> // Para srand(time(NULL))
//#include <unistd.h>  //Para el sleep()

/* -------- VARIABLES GLOBALES Y SEMÁFOROS ----------

Mutex para asegurarnos de que los printf no se mezclen*/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Array dinámico de semáforos: uno por cada estación de mantenimiento
sem_t* semasforosEstacion;
// Semáforo para poner a los autos a "esperar" hasta que haya lugar
sem_t semasEsperaAutos;

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
    perror("Faltan argumentos \n"); // en caso de que no se de el nombre de archivo
    return EXIT_FAILURE;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    perror("Error al leer el archivo \n");
    return EXIT_FAILURE;
  }
  // El archivo debe contener 3 números: nAutos, nEstaciones y capacidadXEstacion
  fscanf(file, "%d", &nAutos);
  fscanf(file, "%d", &nEstaciones);
  fscanf(file, "%d", &capacidadXEstacion);
  fclose(file);

  // 2) INICIALIZAR SEMÁFOROS
  // ---------------------------------------------------
  // Reservo un array de semáforos del tamaño de estaciones
  semasforosEstacion = (sem_t*)malloc(sizeof(sem_t) * nEstaciones);
  if (!semasforosEstacion) {
    perror("No se pudo reservar memoria para los semáforos de las estaciones\n");
    return EXIT_FAILURE;
  }

  // Semáforo adicional que usarán los autos para "quedarse en cola"
  sem_init(&semasEsperaAutos, 0, 0);

  // Para cada estación, se inicializa un semáforo con valor = capacidad de esa estación
  // Así solo la capacidad por estación dirá cuantos autos podrán entrar a esa estación simultáneamente.
  for (int i = 0; i < nEstaciones; i++) {
    sem_init(&semasforosEstacion[i], 0, capacidadXEstacion);
  }

  // 3) CREAR HILOS (AUTOS)
  // ---------------------------------------------------
  // Reservo dinámicamente el array de pthread_t según nAutos,
  // para no inflar el stack con un arreglo gigante.
  pthread_t *autos = (pthread_t *) malloc(sizeof(pthread_t) * nAutos);
  if (!autos) {
    perror("No se pudo reservar memoria para los hilos de autos\n");
    return EXIT_FAILURE;
  }

  srand(time(NULL)); // Semilla para rand (si en el futuro quieres randomizar algo)

  for (int i = 0; i < nAutos; i++) {
    // Cada hilo necesita saber su "número de auto" (del 1 al nAutos).
    // Reservamos un int en el heap para pasárselo al hilo:
    int* indiceAuto = malloc(sizeof(int));
    if (!indiceAuto) {
      printf("No se pudo guardar memoria para el índice del auto %d\n", i + 1);
      return EXIT_FAILURE;
    }
    *indiceAuto = i + 1;

    // Creamos el hilo, que correrá autoRoutine(indiceAuto)
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
  //liberamos memoria
  free(semasforosEstacion);
  free(autos);

  return EXIT_SUCCESS;
}

/* ---------------------------------------------------------
Cada hilo ejecuta esta función: simula a un auto que
quiere ingresar a una estación, hace 4 tareas y luego sale.
------------------------------------------------------------*/
void* autoRoutine(void* arg) {
  // “indiceAuto” = número del auto (1, 2, 3, ..., nAutos)
  int indiceAuto = *(int*)arg;
  free(arg); // Ya no lo necesitamos, lo liberamos

  int estacionAsignada = -1;

  // 
  // 1) TRATAR DE ENTRAR A ALGUNA ESTACIÓN
  // ---------------------------------------------------
  // (Rutina del auto)
  // Mientras no tenga estación, recorro todas y pruebo sem_trywait
  // para ver si me dejan entrar sin bloquearme inmediatamente.
  while (estacionAsignada < 0) {
    for (int i = 0; i < nEstaciones; ++i) {
      if (sem_trywait(&semasforosEstacion[i]) == 0) {
        // Pude restar 1 del semáforo: entré a la estación i+1
        estacionAsignada = i + 1;
        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n", 
                indiceAuto, estacionAsignada);
        pthread_mutex_unlock(&mutex);
        break;
      }
    }

    if (estacionAsignada < 0) {
      // No había lugar en ninguna estación, así que me pongo a esperar
      pthread_mutex_lock(&mutex);
      printf("Vehículo %d está esperando para ingresar a alguna estación de mantenimiento.\n", 
              indiceAuto);
      pthread_mutex_unlock(&mutex);
      sem_wait(&semasEsperaAutos); 
      // Quedo bloqueado hasta que alguien haga sem_post(&semasEsperaAutos),
      // que ocurre cuando un auto sale de su mantenimiento.
    }
  }

  // 2) HACER LAS 4 TAREAS DE MANTENIMIENTO
  // ---------------------------------------------------
  for (int i = 0; i < 4; i++) {
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación de mantenimiento %d.\n",
            indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);

    // sleep(rand()%3 + 1); // si quisieras simular tiempo de trabajo.

    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha completado el mantenimiento de la %s en la estación de mantenimiento %d.\n",
      indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);
  }

  // 3) TERMINÓ TODO, SALE DE LA ESTACIÓN
  // ---------------------------------------------------
  pthread_mutex_lock(&mutex);
  printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);
  pthread_mutex_unlock(&mutex);

  // Libero la plaza en la estación
  sem_post(&semasforosEstacion[estacionAsignada - 1]);
  // Despierto a un auto que esté esperando en la cola general
  sem_post(&semasEsperaAutos);

  pthread_exit(NULL);
}

