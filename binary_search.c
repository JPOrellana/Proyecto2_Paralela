#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>
#include <ctype.h>
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

int loadTextFromFile(const char *filename, char **text, int *length) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return 0;
    }

    fseek(file, 0, SEEK_END);
    *length = ftell(file);
    fseek(file, 0, SEEK_SET);

    *text = (char *)malloc(*length);
    if (*text == NULL) {
        perror("Error de asignación de memoria");
        fclose(file);
        return 0;
    }

    fread(*text, 1, *length, file);
    fclose(file);

    return 1;
}

int tryKey(long key, char *ciph, int len) {
    char temp[len + 1];
    memcpy(temp, ciph, len);
    temp[len] = '\0';
    decrypt(key, temp, len);
    
    if (strstr(temp, "hello") != NULL) {
        return 1; // Clave encontrada
    }
    return 0; // Clave no encontrada
}

int main(int argc, char *argv[]) {
    int N, id;
    long upper = (1L << 56); // Ajuste para soportar claves más largas.
    long mylower, myupper;
    MPI_Status status;
    char *text = NULL;
    int textLength = 0;
    long found = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &N);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);

    long encryptionKey = 1L;
    int option;
    while ((option = getopt(argc, argv, "k:")) != -1) {
        switch (option) {
            case 'k':
                encryptionKey = atol(optarg);
                break;
            default:
                if (id == 0) fprintf(stderr, "Uso: %s [-k key]\n", argv[0]);
                MPI_Finalize();
                return 1;
        }
    }

    if (id == 0) {
        if (!loadTextFromFile("input.txt", &text, &textLength)) {
            MPI_Finalize();
            return 1;
        }
        encrypt(encryptionKey, text, textLength);
        printf("Texto cifrado guardado en memoria.\n");
    }

    MPI_Bcast(&textLength, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (id != 0) {
        text = (char *)malloc(textLength);
    }

    MPI_Bcast(text, textLength, MPI_CHAR, 0, MPI_COMM_WORLD);

    double start_time = MPI_Wtime();

    long range_per_node = upper / N;
    mylower = range_per_node * id;
    myupper = range_per_node * (id + 1) - 1;
    if (id == N - 1) {
        myupper = upper - 1;
    }

    for (long key = mylower; key <= myupper && found == 0; ++key) {
        if (tryKey(key, text, textLength)) {
            found = key;
        }
        MPI_Allreduce(MPI_IN_PLACE, &found, 1, MPI_LONG, MPI_MAX, MPI_COMM_WORLD);
        if (found != 0) break;
    }

    double end_time = MPI_Wtime();
    double elapsed_time = end_time - start_time;

    if (id == 0 && found != 0) {
        decrypt(found, text, textLength);
        text[textLength] = '\0'; 
        printf("¡Llave encontrada! Llave: %ld\n", found);
        printf("Texto descifrado: %s\n", text);
        printf("Tiempo de ejecución: %f segundos\n", elapsed_time);
    }

    free(text);
    MPI_Finalize();
    return 0;
}