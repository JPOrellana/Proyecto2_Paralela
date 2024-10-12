#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <openssl/des.h>
#include <time.h>

void decrypt(long key, char *ciph, int len) {
    DES_cblock key_block;
    DES_key_schedule schedule;
    memset(key_block, 0, sizeof(DES_cblock));
    memcpy(key_block, &key, sizeof(long));
    DES_set_key_unchecked(&key_block, &schedule);
    for (int i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &schedule, DES_DECRYPT);
    }
}

void encrypt(long key, char *ciph, int len) {
    DES_cblock key_block;
    DES_key_schedule schedule;
    memset(key_block, 0, sizeof(DES_cblock));
    memcpy(key_block, &key, sizeof(long));
    DES_set_key_unchecked(&key_block, &schedule);
    for (int i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &schedule, DES_ENCRYPT);
    }
}

char *text;  // Variable global para el texto de entrada
char *encrypted_text;  // Variable global para el texto encriptado
char search_pattern[] = "hello";  // Patrón de búsqueda fijo

int tryKey(long key, char *ciph, int len) {
    char temp[len + 1];
    memcpy(temp, ciph, len);
    temp[len] = 0;
    decrypt(key, temp, len);
    return strstr((char *)temp, search_pattern) != NULL;
}

// Cargar el texto de entrada y eliminar saltos de línea
int loadTextFromFile(const char *filename, int *length) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return 0;
    }

    // Leer línea completa en un buffer
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        size_t len = strlen(buffer);
        // Remover salto de línea al final si existe
        if (buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }
        *length = (int)len;
        text = (char *)malloc(*length + 1);
        strcpy(text, buffer);
    }
    fclose(file);
    return 1;
}

void printHex(char *data, int length) {
    for (int i = 0; i < length; i++) {
        printf("%02x", (unsigned char)data[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    int N, id;
    long upper = (1L << 56);
    long mylower, myupper;
    MPI_Status st;
    MPI_Request req;
    MPI_Comm comm = MPI_COMM_WORLD;

    // Verificar que los argumentos son los correctos
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo> <clave>\n", argv[0]);
        MPI_Finalize();
        exit(1);
    }

    const char *filename = argv[1];
    long encryptionKey = atol(argv[2]);
    if (encryptionKey <= 0) {
        fprintf(stderr, "Error: La clave debe ser un número entero positivo\n");
        MPI_Finalize();
        exit(1);
    }

    MPI_Init(NULL, NULL);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    int textLength;

    // Prueba 1: Cifrar y descifrar con una clave conocida
    if (id == 0) {
        // Cargar el texto de entrada
        if (!loadTextFromFile(filename, &textLength)) {
            MPI_Finalize();
            return 1;
        }
        printf("Texto original: %s\n", text);

        // Cifrar el texto con la clave dada y almacenar en `encrypted_text`
        encrypted_text = (char *)malloc(textLength);
        memcpy(encrypted_text, text, textLength);
        encrypt(encryptionKey, encrypted_text, textLength);

        printf("Texto cifrado en hexadecimal: ");
        printHex(encrypted_text, textLength);  // Mostrar el texto cifrado en hexadecimal

        // Desencriptar el texto usando la misma clave para verificar
        decrypt(encryptionKey, encrypted_text, textLength);
        printf("Texto desencriptado: %s\n", encrypted_text);
    }
    MPI_Barrier(comm);  // Sincronizar después de la primera prueba

    // Prueba 2: Búsqueda de fuerza bruta
    if (id != 0) {
        encrypted_text = (char *)malloc(textLength);  // Reservar espacio para recibir el texto cifrado
    }

    // Difundir el texto cifrado a todos los procesos
    MPI_Bcast(&textLength, 1, MPI_INT, 0, comm);
    MPI_Bcast(encrypted_text, textLength, MPI_CHAR, 0, comm);

    long found = 0;
    MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    // Distribuir rango de búsqueda
    int range_per_node = upper / N;
    mylower = range_per_node * id;
    myupper = range_per_node * (id + 1) - 1;
    if (id == N - 1) {
        myupper = upper;
    }

    // Iniciar búsqueda por fuerza bruta
    for (long i = mylower; i < myupper && (found == 0); ++i) {
        if (tryKey(i, encrypted_text, textLength)) {
            found = i;
            printf("Clave encontrada en proceso %d: %ld\n", id, found);
            for (int node = 0; node < N; node++) {
                MPI_Send(&found, 1, MPI_LONG, node, 0, MPI_COMM_WORLD);
            }
            break;
        }
    }

    // Mostrar resultados en el proceso principal
    if (id == 0) {
        MPI_Wait(&req, &st);
        decrypt(found, encrypted_text, textLength);
        printf("Clave encontrada: %ld\n", found);
        printf("Texto desencriptado con clave encontrada: %s\n", encrypted_text);
    }

    free(text);
    free(encrypted_text);
    MPI_Finalize();
}
