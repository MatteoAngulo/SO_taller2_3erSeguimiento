#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_barrier_t barrera;

int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;

int* capacidadEstaciones;
char* tareas[] = {"BATERÍA", "MOTOR", "DIRECCIÓN", "SISTEMA DE NAVEGACIÓN"};

void* autoRoutine(void* arg);

int main(int argc, char const* argv[]) {
  if (argc < 2) {
    perror("Faltan argumentos \n");
    return EXIT_FAILURE;
  }

  FILE* file = fopen(argv[1], "r");
  if (!file) {
    perror("Error al leer el archivo \n");
    return EXIT_FAILURE;
  }

  fscanf(file, "%d", &nAutos);
  fscanf(file, "%d", &nEstaciones);
  fscanf(file, "%d", &capacidadXEstacion);
  fclose(file);

  capacidadEstaciones = (int*)malloc(sizeof(int) * nEstaciones);
  for (int i = 0; i < nEstaciones; i++) {
    capacidadEstaciones[i] = capacidadXEstacion;
  }

  pthread_barrier_init(&barrera, NULL, nAutos);

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

  printf("Todos los vehículos han completado su mantenimiento y están listos para volver a la carretera\n");

  pthread_barrier_destroy(&barrera);
  free(capacidadEstaciones);

  return EXIT_SUCCESS;
}

void* autoRoutine(void* arg) {
  int indiceAuto = *(int*)arg;
  free(arg);

  int estacionAsignada = -1;

  while (estacionAsignada < 0) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < nEstaciones; ++i) {
      if (capacidadEstaciones[i] > 0) {
        capacidadEstaciones[i]--;
        estacionAsignada = i + 1;
        printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.\n", indiceAuto, estacionAsignada);
        break;
      }
    }
    if (estacionAsignada < 0) {
      printf("Vehículo %d está esperando para ingresar a una estación de mantenimiento.\n", indiceAuto);
    }
    pthread_mutex_unlock(&mutex);

    if (estacionAsignada < 0) usleep(100000); // Espera activa de 100ms
  }

  for (int i = 0; i < 4; i++) {
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación %d.\n", indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);

    sleep(1);  

    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha completado el mantenimiento de la %s en la estación %d.\n", indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);
  }

  pthread_mutex_lock(&mutex);
  printf("Vehículo %d ha completado TODO su mantenimiento.\n", indiceAuto);
  capacidadEstaciones[estacionAsignada - 1]++;
  pthread_mutex_unlock(&mutex);

  pthread_barrier_wait(&barrera);

  pthread_exit(NULL);
}
