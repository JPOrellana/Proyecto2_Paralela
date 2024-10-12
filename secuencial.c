#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/des.h>
#include <time.h>

#define BLOCK_SIZE 8  // DES usa bloques de 8 bytes

// Función para adaptar un número de 56 bits en una llave de 8 bytes para DES
void adaptarLlaveNumerica(unsigned long long numero, unsigned char *llaveAdaptada) {
    // El número es de 56 bits, lo que corresponde a 7 bytes. Vamos a mapearlo.
    for (int i = 0; i < 7; i++) {
        llaveAdaptada[6 - i] = (numero >> (i * 8)) & 0xFF;  // Extraemos byte por byte
    }
    llaveAdaptada[7] = 0;  // El último byte siempre será 0
}

// Función para codificar (cifrar) el texto
void codificarTexto(unsigned char *llaveAdaptada, unsigned char *textoPlano, unsigned char *textoCodificado, int longitud) {
    DES_cblock bloqueLlave;
    DES_key_schedule planLlave;

    // Convertir la llave en un DES_cblock
    memcpy(bloqueLlave, llaveAdaptada, 8);
    DES_set_key((DES_cblock *)bloqueLlave, &planLlave);

    // Codificar cada bloque de 8 bytes
    for (int i = 0; i < longitud; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(textoPlano + i), (DES_cblock *)(textoCodificado + i), &planLlave, DES_ENCRYPT);
    }
}

// Función para descifrar el texto
void descifrarTexto(unsigned char *llaveAdaptada, unsigned char *textoCodificado, unsigned char *textoDescifrado, int longitud) {
    DES_cblock bloqueLlave;
    DES_key_schedule planLlave;

    // Convertir la llave en un DES_cblock
    memcpy(bloqueLlave, llaveAdaptada, 8);
    DES_set_key((DES_cblock *)bloqueLlave, &planLlave);

    // Descifrar cada bloque de 8 bytes
    for (int i = 0; i < longitud; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(textoCodificado + i), (DES_cblock *)(textoDescifrado + i), &planLlave, DES_DECRYPT);
    }
}

// Función para cargar un archivo y devolver el texto contenido
int cargarArchivo(const char *nombreArchivo, unsigned char **texto) {
    FILE *archivo = fopen(nombreArchivo, "rb");
    if (!archivo) {
        perror("Error al abrir el archivo");
        return -1;
    }

    fseek(archivo, 0, SEEK_END);
    long tamArchivo = ftell(archivo);
    rewind(archivo);

    *texto = (unsigned char *)malloc(tamArchivo + 1);
    if (!*texto) {
        perror("Error al asignar memoria");
        fclose(archivo);
        return -1;
    }

    fread(*texto, 1, tamArchivo, archivo);
    fclose(archivo);

    return tamArchivo;
}

// Función para realizar fuerza bruta sobre las llaves
void fuerzaBrutaLlave(unsigned char *textoCodificado, unsigned char *textoOriginal, int longitud) {
    unsigned char textoDescifrado[longitud + 1];
    unsigned char llaveAdaptada[8];

    clock_t inicio = clock();  // Iniciar cronómetro

    // Probar todas las combinaciones de llaves de 56 bits
    for (unsigned long long i = 0; i < (1ULL << 56); i++) {
        // Convertir el número a una llave de 56 bits adaptada a 8 bytes
        adaptarLlaveNumerica(i, llaveAdaptada);

        // Descifrar el texto con la llave actual
        descifrarTexto(llaveAdaptada, textoCodificado, textoDescifrado, longitud);

        // Comparar el texto descifrado con el original
        if (memcmp(textoOriginal, textoDescifrado, longitud) == 0) {
            printf("¡Llave encontrada! Llave: %llu\n", i);
            printf("Texto descifrado: %s\n", textoDescifrado);
            break;
        }
    }

    clock_t fin = clock();  // Detener cronómetro
    double tiempo = (double)(fin - inicio) / CLOCKS_PER_SEC;
    printf("Tiempo de ejecución: %f segundos\n", tiempo);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <input.txt> <llave>\n", argv[0]);
        return 1;
    }

    char *archivoEntrada = argv[1];
    char *llaveOriginal = argv[2];
    int longitudLlave = strlen(llaveOriginal);

    unsigned char llaveAdaptada[8];
    adaptarLlaveNumerica(atoi(llaveOriginal), llaveAdaptada);  // Adaptar la longitud de la llave

    unsigned char *textoOriginal;
    int longitudOriginal = cargarArchivo(archivoEntrada, &textoOriginal);
    if (longitudOriginal < 0) return 1;

    // Ajustar el tamaño del texto a múltiplos de BLOCK_SIZE (8 bytes)
    int longitudAjustada = ((longitudOriginal + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    unsigned char *textoCodificado = (unsigned char *)malloc(longitudAjustada);

    // Codificar el texto
    codificarTexto(llaveAdaptada, textoOriginal, textoCodificado, longitudAjustada);
    printf("Texto cifrado guardado en memoria.\n");

    // Realizar la fuerza bruta para encontrar la llave correcta
    fuerzaBrutaLlave(textoCodificado, textoOriginal, longitudAjustada);

    free(textoOriginal);
    free(textoCodificado);

    return 0;
}