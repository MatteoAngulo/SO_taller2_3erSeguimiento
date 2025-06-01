/*
Problema de concurrencia "El Autolavado Durmiente" basado en "The Sleeping
Barber Problem"

Contexto:

- Un solo autolavado automático.

- Una única estación de lavado (puede lavar un solo auto a la vez).

- Una fila de espera con un número limitado de espacios (por ejemplo, carriles o
bahías de espera).

Reglas:

- Si no hay autos esperando, el autolavado se apaga para ahorrar energía.

- Cuando llega un auto:

  - Si el autolavado está apagado, el auto lo activa (lo "despierta") y pasa
directamente a ser lavado.

  - Si el autolavado está ocupado:

    - Si hay espacio en la fila de espera, el auto se forma.

    - Si no hay espacio, el auto se va (no espera).

El objetivo del problema es sincronizar correctamente el funcionamiento del
autolavado y la llegada de autos, asegurando que no se pierdan autos
innecesariamente y que el autolavado funcione solo cuando sea necesario.
*/

//con semásforos

#define _XOPEN_SOURCE 600
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>


int espaciosLibres = 0, constEspaciosLibres = 0, nAutos= 0, turno = 1;


void* autolavadoRoutine(void* arg);

void* autosRoutine(void* arg);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t semaAutolavado;
sem_t semaAutos;


int main(int argc, char const *argv[]) {
  if(argc < 3){
    perror("faltan argumentos -> ./archivo.out [espacios Libres] [numero de autos] \n");
    return EXIT_FAILURE;
  }
  espaciosLibres = atoi(argv[1]);
  constEspaciosLibres = atoi(argv[1]);
  nAutos = atoi(argv[2]);

  sem_init(&semaAutolavado, 0, 0);
  sem_init(&semaAutos,      0, 0);

  
  pthread_t autolavado;
  pthread_t autos[nAutos];



  pthread_create(&autolavado, NULL, autolavadoRoutine, NULL);

  for (int i = 0; i < nAutos; i++){
    int* id = (int*) malloc(sizeof(int));
    *id = i+1;
    pthread_create(&autos[i], NULL, autosRoutine, id);
  }
  
  for (int i = 0; i < nAutos; i++){
    pthread_join(autos[i],NULL);
  }
  
  
  return 0;
}


void* autolavadoRoutine(void* arg){
    while(true){
      sem_wait(&semaAutolavado);
      pthread_mutex_lock(&mutex);
      espaciosLibres++;
      printf("Autolavado recibiendo a auto... quedan %d espacios libres \n",espaciosLibres);
      pthread_mutex_unlock(&mutex);
      sem_post(&semaAutos);
      sleep(3);
      printf("Auto lavado... \n");
    }
  
    pthread_exit(0);
}

void* autosRoutine(void* arg){
  int turnoAuto = *(int*)arg;
  sleep(rand() % 3);
  pthread_mutex_lock(&mutex);
  if(espaciosLibres > 0){
    espaciosLibres--;
    printf("Auto %d ingresando por espacio libre del autolavado... y hay %d espacios libres \n", turnoAuto, espaciosLibres);
    pthread_mutex_unlock(&mutex);
    sem_post(&semaAutolavado);
    sem_wait(&semaAutos);
    printf("Auto %d está siendo lavado... \n", turnoAuto);
    free(arg);

  }else{
    printf("Auto %d se va porque no hay espacio.. \n",turnoAuto);
    pthread_mutex_unlock(&mutex);
    free(arg);
    pthread_exit(0);
  }

  
  pthread_exit(0);
}