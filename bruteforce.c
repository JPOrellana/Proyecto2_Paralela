#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>

void decrypt(long key, char *ciph, int len){
  // Preparar la clave y realizar descifrado
  DES_key_schedule schedule;
  DES_cblock des_key;
  memcpy(des_key, &key, sizeof(DES_cblock));  // Copiar la clave
  DES_set_key_checked(&des_key, &schedule);   // Configurar clave con paridad
  for(int i = 0; i < len; i += 8) {           // Iterar bloques de 8 bytes
    DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &schedule, DES_DECRYPT);
  }
}

void encrypt(long key, char *ciph, int len){
  // Preparar la clave y realizar cifrado
  DES_key_schedule schedule;
  DES_cblock des_key;
  memcpy(des_key, &key, sizeof(DES_cblock));
  DES_set_key_checked(&des_key, &schedule);
  for(int i = 0; i < len; i += 8) {
    DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &schedule, DES_ENCRYPT);
  }
}

char search[] = " the ";
int tryKey(long key, char *ciph, int len){
  char temp[len+1];
  memcpy(temp, ciph, len);
  temp[len]=0;
  decrypt(key, temp, len);
  return strstr((char *)temp, search) != NULL;
}

unsigned char cipher[] = {108, 245, 65, 63, 125, 200, 150, 66, 17, 170, 207, 170, 34, 31, 70, 215, 0};

int main(int argc, char *argv[]){
  int N, id;
  
  // Reducción del rango de búsqueda para pruebas rápidas
  long upper = (1L << 24); // Rango reducido para pruebas
  long mylower, myupper;
  MPI_Status st;
  MPI_Request req;
  int flag;
  int ciphlen = strlen(cipher);
  MPI_Comm comm = MPI_COMM_WORLD;

  MPI_Init(NULL, NULL);
  MPI_Comm_size(comm, &N);
  MPI_Comm_rank(comm, &id);

  // Definir el rango de búsqueda para cada proceso
  int range_per_node = upper / N;
  mylower = range_per_node * id;
  myupper = range_per_node * (id + 1) - 1;
  if (id == N - 1) {
    myupper = upper; // Compensar residuo en el último proceso
  }

  // Mensajes de diagnóstico
  printf("Proceso %d: Rango de búsqueda desde %ld hasta %ld\n", id, mylower, myupper);
  
  long found = 0;
  MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);
  printf("Proceso %d: Esperando clave encontrada en otros procesos\n", id);

  for (long i = mylower; i < myupper && (found == 0); ++i) {
    if (tryKey(i, (char *)cipher, ciphlen)) {
      found = i;
      printf("Proceso %d: Clave encontrada! %ld\n", id, found);  // Mensaje de clave encontrada
      for (int node = 0; node < N; node++) {
        MPI_Send(&found, 1, MPI_LONG, node, 0, MPI_COMM_WORLD);
        printf("Proceso %d: Enviando clave %ld al proceso %d\n", id, found, node);
      }
      break;
    }
  }

  // Comprobar si el proceso encontró la clave antes de recibirla
  printf("Proceso %d: Comprobando clave recibida después de búsqueda\n", id);
  if (found == 0) {
    MPI_Wait(&req, &st);
    printf("Proceso %d: Clave recibida %ld\n", id, found);
  }

  if (id == 0 && found != 0) {
    printf("Proceso %d: Iniciando descifrado con clave %ld\n", id, found);
    decrypt(found, (char *)cipher, ciphlen);
    printf("Clave: %li, Mensaje descifrado: %s\n", found, cipher);
  } else if (id == 0) {
    printf("Proceso %d: No se encontró la clave\n", id);
  }

  MPI_Finalize();
  return 0;
}
