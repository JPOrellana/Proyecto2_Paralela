#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <openssl/des.h>
#include <time.h>

#define BLOCK_SIZE 8  // Tamaño del bloque de DES
#define MAX_KEY 100000000L  // Limite superior para las claves

// Función para cifrar el texto con una clave
void cifrarTexto(long llave, char *texto, int len) {
    DES_cblock bloqueLlave;
    DES_key_schedule planLlave;

    // Configura la llave
    memset(bloqueLlave, 0, sizeof(DES_cblock));
    memcpy(bloqueLlave, &llave, sizeof(long));
    DES_set_key_unchecked(&bloqueLlave, &planLlave);

    // Cifrar cada bloque
    for (int i = 0; i < len; i += BLOCK_SIZE) {
        DES_ecb_encrypt((DES_cblock *)(texto + i), (DES_cblock *)(texto + i), &planLlave, DES_ENCRYPT);
    }
}

// Función para descifrar el texto con una clave
void descifrarTexto(long llave, char *textoCifrado, int len) {
    DES_cblock bloqueLlave;
    DES_key_schedule planLlave;

    // Configura la llave
    memset(bloqueLlave, 0, sizeof(DES_cblock));
    memcpy(bloqueLlave, &llave, sizeof(long));
    DES_set_key_unchecked(&bloqueLlave, &planLlave);

    // Descifrar cada bloque
    for (int i = 0; i < len; i += BLOCK_SIZE) {
        DES_ecb_encrypt((DES_cblock *)(textoCifrado + i), (DES_cblock *)(textoCifrado + i), &planLlave, DES_DECRYPT);
    }
}

// Función para cargar el texto desde un archivo
int cargarTexto(const char *nombreArchivo, char **texto) {
    FILE *archivo = fopen(nombreArchivo, "r");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return -1;
    }

    fseek(archivo, 0, SEEK_END);
    long tamArchivo = ftell(archivo);
    rewind(archivo);

    *texto = (char *)malloc(tamArchivo + 1);
    if (*texto == NULL) {
        perror("Error al asignar memoria");
        fclose(archivo);
        return -1;
    }

    fread(*texto, 1, tamArchivo, archivo);
    fclose(archivo);
    return tamArchivo;
}

// Función para realizar fuerza bruta en búsqueda de la llave correcta
long fuerzaBrutaLlave(long inicio, long fin, char *textoCifrado, char *textoOriginal, int len) {
    char textoDescifrado[len + 1];
    for (long llave = inicio; llave <= fin; llave++) {
        memcpy(textoDescifrado, textoCifrado, len);
        descifrarTexto(llave, textoDescifrado, len);

        // Comparar el texto descifrado con el original
        if (memcmp(textoOriginal, textoDescifrado, len) == 0) {
            return llave;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    int id, N;
    long llaveInicial, llaveFinal;
    long llaveCorrecta = -1;
    long llaveGlobal = -1;  // Variable para recibir el resultado final
    MPI_Status st;
    double tiempoInicio, tiempoFin;

    // Inicializa MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &N);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);

    // Verificación de argumentos
    if (argc != 3) {
        if (id == 0) {
            printf("Uso: %s <input.txt> <llave_inicial>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    char *archivoEntrada = argv[1];
    long llaveProporcionada = atol(argv[2]);

    // Cada proceso debe cargar el texto original
    char *textoOriginal, *textoCifrado;
    int longitudTexto = cargarTexto(archivoEntrada, &textoOriginal);
    if (longitudTexto < 0) {
        MPI_Finalize();
        return 1;
    }

    // Ajustar el tamaño del texto a múltiplos de BLOCK_SIZE
    int longitudAjustada = ((longitudTexto + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    textoCifrado = (char *)malloc(longitudAjustada);

    // Solo el proceso 0 cifra el texto
    if (id == 0) {
        memcpy(textoCifrado, textoOriginal, longitudTexto);
        cifrarTexto(llaveProporcionada, textoCifrado, longitudAjustada);
        printf("Texto cifrado guardado en memoria.\n");
    }

    // Difundir el texto cifrado a todos los procesos
    MPI_Bcast(textoCifrado, longitudAjustada, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Calcular el rango de llaves para cada proceso
    long rangoPorProceso = MAX_KEY / N;
    llaveInicial = id * rangoPorProceso;
    llaveFinal = (id + 1) * rangoPorProceso - 1;

    if (id == N - 1) {
        llaveFinal = MAX_KEY;  // Asegurar que el último proceso cubra todo el rango restante
    }

    // Iniciar medición de tiempo
    tiempoInicio = MPI_Wtime();

    // Cada proceso realiza fuerza bruta en su rango de llaves
    llaveCorrecta = fuerzaBrutaLlave(llaveInicial, llaveFinal, textoCifrado, textoOriginal, longitudAjustada);

    // Recoger la llave correcta de todos los procesos
    MPI_Reduce(&llaveCorrecta, &llaveGlobal, 1, MPI_LONG, MPI_MAX, 0, MPI_COMM_WORLD);

    // Solo el proceso 0 imprime la salida
    if (llaveGlobal != -1 && id == 0) {
        printf("¡Llave encontrada! Llave: %ld\n", llaveGlobal);
        descifrarTexto(llaveGlobal, textoCifrado, longitudAjustada);
        printf("Texto descifrado: %s\n", textoCifrado);
    }

    // Medir tiempo final
    tiempoFin = MPI_Wtime();
    if (id == 0) {
        printf("Tiempo de ejecución: %f segundos\n", tiempoFin - tiempoInicio);
    }

    // Liberar memoria
    free(textoOriginal);
    free(textoCifrado);

    MPI_Finalize();
    return 0;
}
