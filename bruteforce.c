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

char search[] = "hello";

int attemptDecrypt(long key, char *ciph, int len) {
    char temp[len+1];
    memcpy(temp, ciph, len);
    temp[len] = 0;
    decrypt(key, temp, len);

    if (strstr((char *)temp, search) != NULL) {
        printf("Clave encontrada: %li\n", key);
        return 1;
    }

    return 0;
}

int retrieveText(const char *filename, char **text, int *length) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return 0;
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        size_t len = strlen(buffer);
        if (buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }
        *length = (int)len;
        *text = (char *)malloc(*length + 1);
        strcpy(*text, buffer);
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

    MPI_Init(NULL, NULL);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    clock_t start_time_C, end_time_C;
    double start_time, end_time;
    long encryptionKey = -1; // Valor inicial que indica que no se ha proporcionado clave

    int option;
    int key_provided = 0; // Variable para rastrear si -k fue proporcionado
    char *endptr;

    while ((option = getopt(argc, argv, "k:")) != -1) {
        switch (option) {
            case 'k':
                if (optarg != NULL) {
                    encryptionKey = strtol(optarg, &endptr, 10);
                    
                    // Verificar si el argumento es un número válido
                    if (*endptr != '\0' || encryptionKey <= 0) {
                        if (id == 0) {
                            fprintf(stderr, "Error: El argumento de -k debe ser un número entero positivo.\n");
                        }
                        MPI_Finalize();
                        exit(1);
                    }
                    key_provided = 1; // Marcamos que -k fue especificado y es válido
                } else {
                    if (id == 0) {
                        fprintf(stderr, "Error: La opción -k requiere un argumento. Uso: %s -k key\n", argv[0]);
                    }
                    MPI_Finalize();
                    exit(1);
                }
                break;
            default:
                if (id == 0) {
                    fprintf(stderr, "Uso: %s -k key\n", argv[0]);
                }
                MPI_Finalize();
                exit(1);
        }
    }

    // Verificar si se proporcionó la clave de encriptación
    if (encryptionKey == -1 || !key_provided) {
        if (id == 0) {
            fprintf(stderr, "Error: La clave de encriptación es obligatoria. Use la opción -k para especificarla.\n");
        }
        MPI_Finalize();
        exit(1);
    }

    // Verificar que la clave esté dentro del rango DES (0 <= clave < 2^56)
    if (encryptionKey <= 0 || encryptionKey >= upper) {
        if (id == 0) {
            fprintf(stderr, "Error: La clave de encriptación debe estar en el rango de 1 a %li.\n", upper - 1);
        }
        MPI_Finalize();
        exit(1);
    }

    start_time_C = clock();
    start_time = MPI_Wtime();

    int range_per_node = upper / N;
    mylower = range_per_node * id;
    myupper = range_per_node * (id + 1) - 1;
    if (id == N - 1) {
        myupper = upper;
    }

    char *text;
    int textLength;

    if (id == 0) {
        if (!retrieveText("input.txt", &text, &textLength)) {
            MPI_Finalize();
            return 1;
        }
        printf("Texto a cifrar: %s\n", text);
        encrypt(encryptionKey, text, textLength);
        printf("Texto cifrado en hexadecimal: ");
        printHex(text, textLength);
    }

    MPI_Bcast(&textLength, 1, MPI_INT, 0, comm);
    if (id != 0) {
        text = (char *)malloc(textLength);
    }
    MPI_Bcast(text, textLength, MPI_CHAR, 0, comm);

    long found = 0;

    MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    for (int i = mylower; i < myupper && (found == 0); ++i) {
        if (attemptDecrypt(i, text, textLength)) {
            found = i;
            for (int node = 0; node < N; node++) {
                MPI_Send(&found, 1, MPI_LONG, node, 0, MPI_COMM_WORLD);
            }
            break;
        }
    }

    if (id == 0) {
        MPI_Wait(&req, &st);
        decrypt(found, text, textLength);
        printf("Texto descifrado con clave: %s\n", text);

        end_time = MPI_Wtime();
        end_time_C = clock();

        printf("Tiempo de ejecución: %f segundos\n", end_time - start_time);
    }

    free(text);
    MPI_Finalize();
}
