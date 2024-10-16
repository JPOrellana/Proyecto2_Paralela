# Proyecto - Cifrado DES
**Universidad del Valle de Guatemala**\
**Facultad de Ingeniería**\
**Departamento de Ciencias de la Computación**\
**Computación Paralela y Distribuida**

---

## Autores
- Diego Leiva
- Pablo Orellana

---
### Dependencias
- `GCC`
- `OpenSSL`
- `OpenMPI`

Para instalar las dependencias en su instancia de linux o WSL en windows ejecute:
**OpenSSL**
```bash
sudo apt update
sudo apt install openssl
```
**OpenMPI**
```bash
sudo apt-get install openmpi-bin openmpi-doc libopenmpi-dev 
```
**GCC**
```bash
sudo apt-get install build-essential
```

### Ejecucion del Proyecto en WSL
Para poder compilar y ejecutar los programas utilice los siguientes comandos:
**secuencial.c**
```bash
gcc -o secuencial secuencial.c -lssl -lcrypto
./secuencial input.txt <llave>
```
**bruteforce.c**
```bash
mpicc -o bruteforce bruteforce.c -lssl -lcrypto
mpirun -np 4 ./bruteforce -k <llave>
```

**jump_search.c**
```bash
mpicc -o jump_search jump_search.c -lssl -lcrypto
mpirun -np 4 ./jump_search -k <llave>
```

**binary_search.c**
```bash
mpicc -o binary_search binary_search.c -lssl -lcrypto
mpirun -np 4 ./binary_search -k <llave>
```

### Nota:
si tiene problemas con errores de librerias deprecadas compile los programas con esta bandera adicional:
```bash
mpicc -o <output> <programa.c> -lssl -lcrypto -Wno-deprecated-declarations
```
