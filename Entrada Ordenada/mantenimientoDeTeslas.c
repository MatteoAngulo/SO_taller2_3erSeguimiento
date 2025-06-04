#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// Con semásforo


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t turnoMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turnoCond   = PTHREAD_COND_INITIALIZER;

sem_t* semasforosEstacion;
sem_t semasEsperaAutos;

int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0;
int turnoAuto = 1;

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

  semasforosEstacion = (sem_t*)malloc(sizeof(sem_t) * nEstaciones);
  if (!semasforosEstacion) {
    perror(
        "No se pudo reservar memoria para los semasforos de las estaciones \n");
    return EXIT_FAILURE;
  }

  sem_init(&semasEsperaAutos,0,0);

  for (int i = 0; i < nEstaciones; i++) {
    sem_init(&semasforosEstacion[i], 0, capacidadXEstacion);
  }

  pthread_t *autos = (pthread_t *) malloc(sizeof(pthread_t) * nAutos);
  int* indiceAuto;

  srand(time(NULL));

  for (int i = 0; i < nAutos; i++) {
    indiceAuto = malloc(sizeof(int));
    *indiceAuto = i + 1;
    pthread_create(&autos[i], NULL, autoRoutine, (void*)indiceAuto);
  }


  for (int i = 0; i < nAutos; i++) {
    pthread_join(autos[i], NULL);
  }
  
  printf("Todos los vehículos han completado su mantenimiento y están listos para volver a la carretera \n");


  for (int i = 0; i < nEstaciones; i++) {
    sem_destroy(&semasforosEstacion[i]);
  }

  free(semasforosEstacion);
  return EXIT_SUCCESS;
}


void* autoRoutine(void* arg) {
  int indiceAuto = *(int*)arg;
  free(arg);

  pthread_mutex_lock(&turnoMutex);
  while(indiceAuto != turnoAuto){
    pthread_cond_wait(&turnoCond, &turnoMutex);
  }
  pthread_mutex_lock(&mutex);
  turnoAuto++;
  pthread_mutex_unlock(&mutex);
  pthread_cond_broadcast(&turnoCond);
  pthread_mutex_unlock(&turnoMutex);


  int estacionAsignada = -1;

  while (estacionAsignada < 0) {
    for (int i = 0; i < nEstaciones; ++i) {
      if (sem_trywait(&semasforosEstacion[i]) == 0) {
        estacionAsignada = i+1;
        pthread_mutex_lock(&mutex);
        printf("Vehículo %d ha ingresado a la estación de mantenimiento %d.  \n", indiceAuto, estacionAsignada);
        pthread_mutex_unlock(&mutex);
        break;
      }
    }

    if (estacionAsignada < 0) {
      pthread_mutex_lock(&mutex);
      printf("Vehículo %d está esperando para ingresar a alguna estación de mantenimiento.\n",indiceAuto);
      pthread_mutex_unlock(&mutex);
      sem_wait(&semasEsperaAutos);
    }
  }

  for (int i = 0; i < 4; i++) {
    //sleep(1);

    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha iniciado el mantenimiento de la %s en la estación de mantenimiento %d. \n",indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);
    // sleep(3);//para más realismo
    pthread_mutex_lock(&mutex);
    printf("Vehículo %d ha completado el mantenimiento de la %s en la estación de mantenimiento de su %d. \n",indiceAuto, tareas[i], estacionAsignada);
    pthread_mutex_unlock(&mutex);
    
  }
  pthread_mutex_lock(&mutex);
  printf("Vehículo %d ha completado TODO su mantenimiento \n",indiceAuto);
  pthread_mutex_unlock(&mutex);
  sem_post(&semasforosEstacion[estacionAsignada-1]);
  sem_post(&semasEsperaAutos);

  pthread_exit(NULL);
}
