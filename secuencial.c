#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/des.h>

#define BLOCK_SIZE 8  // DES usa bloques de 8 bytes

// Función para codificar (cifrar) el texto
void codificarTexto(char *llave, unsigned char *textoPlano, unsigned char *textoCodificado, int longitud) {
    DES_cblock bloqueLlave;
    DES_key_schedule planLlave;

    // Convertir la llave en un DES_cblock
    memcpy(bloqueLlave, llave, BLOCK_SIZE);
    DES_set_key((DES_cblock *)bloqueLlave, &planLlave);

    // Codificar cada bloque de 8 bytes
    for (int i = 0; i < longitud; i += BLOCK_SIZE) {
        DES_ecb_encrypt((DES_cblock *)(textoPlano + i), (DES_cblock *)(textoCodificado + i), &planLlave, DES_ENCRYPT);
    }
}

// Función para decodificar (descifrar) el texto
void decodificarTexto(char *llave, unsigned char *textoCodificado, unsigned char *textoDecodificado, int longitud) {
    DES_cblock bloqueLlave;
    DES_key_schedule planLlave;

    // Convertir la llave en un DES_cblock
    memcpy(bloqueLlave, llave, BLOCK_SIZE);
    DES_set_key((DES_cblock *)bloqueLlave, &planLlave);

    // Decodificar cada bloque de 8 bytes
    for (int i = 0; i < longitud; i += BLOCK_SIZE) {
        DES_ecb_encrypt((DES_cblock *)(textoCodificado + i), (DES_cblock *)(textoDecodificado + i), &planLlave, DES_DECRYPT);
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

// Función para guardar un archivo con el texto proporcionado
void guardarArchivo(const char *nombreArchivo, unsigned char *texto, int longitud) {
    FILE *archivo = fopen(nombreArchivo, "wb");
    if (!archivo) {
        perror("Error al abrir el archivo para escritura");
        return;
    }

    fwrite(texto, 1, longitud, archivo);
    fclose(archivo);
}

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[1], "-k") != 0) {
        printf("Uso: %s -k <llave> <archivo>\n", argv[0]);
        return 1;
    }

    char *llave = argv[2];
    char *archivoEntrada = argv[3];

    if (strlen(llave) != BLOCK_SIZE) {
        printf("La clave debe tener 8 caracteres.\n");
        return 1;
    }

    unsigned char *textoPlano;
    int longitudTexto = cargarArchivo(archivoEntrada, &textoPlano);
    if (longitudTexto < 0) {
        return 1;
    }

    // Ajustar el tamaño del texto a múltiplos de BLOCK_SIZE
    int longitudAjustada = ((longitudTexto + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    unsigned char *textoCodificado = (unsigned char *)malloc(longitudAjustada);
    unsigned char *textoDecodificado = (unsigned char *)malloc(longitudAjustada);

    // Codificar el texto
    codificarTexto(llave, textoPlano, textoCodificado, longitudAjustada);
    guardarArchivo("texto_cifrado.txt", textoCodificado, longitudAjustada);
    printf("Texto cifrado guardado en texto_cifrado.txt\n");

    // Decodificar el texto
    decodificarTexto(llave, textoCodificado, textoDecodificado, longitudAjustada);
    guardarArchivo("texto_descifrado.txt", textoDecodificado, longitudAjustada);
    printf("Texto descifrado guardado en texto_descifrado.txt\n");

    free(textoPlano);
    free(textoCodificado);
    free(textoDecodificado);

    return 0;
}
