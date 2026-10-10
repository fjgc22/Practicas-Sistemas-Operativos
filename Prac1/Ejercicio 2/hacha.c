/*
 * Practica 1 - Ejercicio 2: hacha
 *
 * Uso: ./hacha archivo tamano
 *
 * Parte el archivo en trozos de "tamano" bytes: archivo.h00, archivo.h01...
 * El padre lee el archivo y le manda cada trozo a un hijo por una tuberia.
 * El hijo es el que crea el archivo del trozo y escribe en el.
 *
 * Los hijos los lanzo de uno en uno: hasta que no acaba uno no creo el
 * siguiente. Asi el padre sabe si cada trozo ha salido bien.
 *
 * Aqui no se puede usar printf, todo va con write.
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define TAM_BLOQUE 4096    // leo y escribo en bloques de este tamano
#define TAM_NOMBRE 1024    // maximo para el nombre de un trozo

/* Saca el mensaje por la salida de errores (descriptor 2) y termina el programa */
void fallo(char *msg) {
    write(2, msg, strlen(msg));
    exit(1);
}

/* Devuelve 1 si la cadena son todo digitos y 0 si no (por ejemplo "50000abc" da 0) */
int soloDigitos(char *cad) {
    int k;

    if (cad[0] == '\0')
        return 0;

    for (k = 0; cad[k] != '\0'; k++) {
        if (cad[k] < '0' || cad[k] > '9')
            return 0;
    }
    return 1;
}

/* Deja en "salida" el nombre del trozo n: nombre del archivo + ".h" + el numero.
   El numero va con dos cifras como minimo (h00, h01 ... h99) y si hay mas de
   cien trozos pues con tres (h100, h101 ...).
   Lo hago a mano porque sprintf no se puede usar en este ejercicio. */
void nombreTrozo(char *salida, char *archivo, int n) {
    char digitos[12];
    int nd = 0;
    int pos;

    // voy sacando las cifras con el resto de dividir entre 10 (salen al reves)
    do {
        digitos[nd] = '0' + n % 10;
        n = n / 10;
        nd++;
    } while (n > 0);

    // si solo ha salido una cifra le anado un 0 (que al darle la vuelta queda delante)
    if (nd == 1) {
        digitos[1] = '0';
        nd = 2;
    }

    strcpy(salida, archivo);
    pos = strlen(salida);
    salida[pos] = '.';
    salida[pos + 1] = 'h';
    pos = pos + 2;

    // copio las cifras empezando por la ultima para que queden en su orden
    while (nd > 0) {
        nd--;
        salida[pos] = digitos[nd];
        pos++;
    }
    salida[pos] = '\0';
}

/* Codigo del hijo: crea el archivo del trozo n y mete en el todo lo que
   le llega por la tuberia. Siempre acaba con exit, no vuelve al main. */
void hijo(int tubo[2], int origen, char *archivo, int n) {
    int destino, leidos;
    char bloque[TAM_BLOQUE];
    char nombre[TAM_NOMBRE];

    // el hijo solo lee de la tuberia, cierro el lado de escribir.
    // el archivo original tampoco lo uso
    close(tubo[1]);
    close(origen);

    nombreTrozo(nombre, archivo, n);

    destino = creat(nombre, 0644);
    if (destino < 0) {
        fallo("Error al crear el archivo del trozo\n");
    }

    // cuando el padre cierra su lado de la tuberia, read devuelve 0 y salgo del bucle
    while ((leidos = read(tubo[0], bloque, TAM_BLOQUE)) > 0) {
        if (write(destino, bloque, leidos) != leidos) {
            fallo("Error al escribir el trozo\n");
        }
    }

    close(destino);
    close(tubo[0]);
    exit(0);
}

/* Codigo del padre: lee "cuantos" bytes del archivo original y los manda por
   la tuberia, bloque a bloque. Devuelve 1 si ha ido bien y 0 si ha fallado algo. */
int mandarTrozo(int origen, int salida, long cuantos) {
    int leidos, pedir;
    char bloque[TAM_BLOQUE];

    while (cuantos > 0) {
        // pido un bloque entero, o lo que quede si es menos
        pedir = TAM_BLOQUE;
        if (cuantos < TAM_BLOQUE)
            pedir = cuantos;

        leidos = read(origen, bloque, pedir);
        if (leidos <= 0)
            return 0;   // error al leer, o el archivo se ha acabado antes de lo que tocaba

        if (write(salida, bloque, leidos) != leidos)
            return 0;   // el hijo ya no esta leyendo

        cuantos = cuantos - leidos;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int origen;
    int tubo[2];
    int numTrozos, t, status, bien;
    long tamTrozo, tamTotal, cuantos;
    pid_t pid;

    // comprobaciones de los argumentos
    if (argc != 3) {
        fallo("Uso: ./hacha <archivo> <tamano>\n");
    }
    if (!soloDigitos(argv[2]) || atoi(argv[2]) <= 0) {
        fallo("Error: el tamano tiene que ser un numero mayor que 0\n");
    }
    tamTrozo = atoi(argv[2]);

    // tiene que caber: nombre + ".h" + hasta 10 cifras + el '\0'
    if (strlen(argv[1]) + 13 > TAM_NOMBRE) {
        fallo("Error: el nombre del archivo es demasiado largo\n");
    }

    origen = open(argv[1], O_RDONLY);
    if (origen < 0) {
        fallo("Error al abrir el archivo\n");
    }

    // para saber cuanto ocupa: lseek al final me devuelve la posicion, que es
    // el tamano. Luego vuelvo al principio para empezar a leer
    tamTotal = lseek(origen, 0, SEEK_END);
    if (tamTotal < 0 || lseek(origen, 0, SEEK_SET) < 0) {
        fallo("Error: no se puede saber el tamano del archivo\n");
    }
    if (tamTotal == 0) {
        fallo("Error: el archivo esta vacio\n");
    }

    // numero de trozos, redondeando hacia arriba
    numTrozos = (tamTotal + tamTrozo - 1) / tamTrozo;

    // Si un hijo falla y cierra la tuberia, cuando el padre escribe en ella le
    // llega SIGPIPE y se muere sin decir nada. Ignorandola, write devuelve
    // error y asi puedo sacar yo el mensaje
    signal(SIGPIPE, SIG_IGN);

    for (t = 0; t < numTrozos; t++) {

        // una tuberia nueva para cada hijo
        if (pipe(tubo) < 0) {
            fallo("Error al crear la tuberia\n");
        }

        pid = fork();
        if (pid == -1) {
            fallo("Error en el fork\n");
        }
        if (pid == 0) {
            hijo(tubo, origen, argv[1], t);   // de aqui no vuelve
        }

        // el padre solo escribe en la tuberia, cierro el lado de leer
        close(tubo[0]);

        // todos los trozos llevan tamTrozo bytes menos el ultimo, que lleva lo que sobre
        cuantos = tamTrozo;
        if (t == numTrozos - 1) {
            cuantos = tamTotal - (long)t * tamTrozo;
        }

        bien = mandarTrozo(origen, tubo[1], cuantos);

        // al cerrar el lado de escribir, al hijo le llega el fin de archivo (su read da 0)
        close(tubo[1]);

        // espero a este hijo antes de crear el siguiente y miro como ha acabado.
        // si no ha terminado con exit(0) es que el trozo no se ha creado bien
        wait(&status);
        if (status != 0 || !bien) {
            close(origen);
            fallo("Error: no se ha podido crear uno de los trozos\n");
        }
    }

    close(origen);
    return 0;
}
