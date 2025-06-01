#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
// Con semásforo


sem_t estacionLista;
sem_t autoListo;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int nAutos = 0, nEstaciones = 0, capacidadXEstacion = 0, estacionesOcupadas = 0, indiceAuto = 1, nAutosEnEstacion;

void* estacionRoutine(void* arg);
void* autoRoutine(void* arg);

int main(int argc, char const* argv[]) {
  if (argc < 2) {
    perror("Faltan argumentos \n");
    return EXIT_FAILURE;
  }
  FILE* file = fopen(argv[1], "r");
  fscanf(file, "%d", &nAutos);
  fscanf(file, "%d", &nEstaciones);
  fscanf(file, "%d", &capacidadXEstacion);
  fclose(file);

  printf("numero de estaciones %d \n",nEstaciones);

  sem_init(&estacionLista, 0, 0);
  sem_init(&autoListo, 0, 0);


  pthread_t estaciones[nEstaciones];
  pthread_t autos[nAutos];

  for (int i = 0; i < nEstaciones; i++) {
    // int* indice;
    // *indice = i + 1;
    pthread_create(&estaciones[i], NULL, estacionRoutine, NULL);
  }

  for (int i = 0; i < nAutos; i++) {
    // int* indice;
    // *indice = i + 1;
    pthread_create(&autos[i], NULL, autoRoutine,NULL);
  }


  for (int i = 0; i < nEstaciones; i++) {
    pthread_join(estaciones[i],NULL);
  }
  
  for (int i = 0; i < nAutos; i++) {
    pthread_join(autos[i],NULL);
  }

  sem_destroy(&estacionLista);
  sem_destroy(&autoListo);
  return 0;
}


void* estacionRoutine(void* arg) {
  // int indice = *(int*)arg;
  while(1){
    sem_wait(&autoListo);
    pthread_mutex_lock(&mutex);
    nAutosEnEstacion++;
    printf("Vehiculo %d ha completado el mantenimiento en la estacion %d \n", indiceAuto, estacionesOcupadas+1);
    if(nAutosEnEstacion == capacidadXEstacion){
      estacionesOcupadas++;
    }
    pthread_mutex_unlock(&mutex);
    sem_post(&estacionLista);    

  }

}


void* autoRoutine(void* arg) {
  // int indice = *(int*)arg;
  while(indiceAuto != nAutos){
    pthread_mutex_lock(&mutex);
    if(estacionesOcupadas < nEstaciones){
      printf("Vehiculo %d ha ingresado a la estación de mantimiento %d \n", indiceAuto, estacionesOcupadas+1);
      indiceAuto++;
      pthread_mutex_unlock(&mutex);
      sem_post(&autoListo);
      sem_wait(&estacionLista);
    }else{
      printf("vehiculo %d está esperando para ingresar a alguna estación de mantenimiento \n", indiceAuto);
      // pthread_mutex_unlock(&mutex);
    }
  }
  

}