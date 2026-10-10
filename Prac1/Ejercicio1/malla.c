/*
 * Practica 1 - Ejercicio 1a: malla
 *
 * Uso: ./malla x y        (x = filas, y = columnas)
 *
 * malla crea y hijos (la primera fila). Cada uno de esos hijos crea el resto
 * de su columna hacia abajo, de uno en uno, hasta llegar a la fila x.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ESPERA 20   // segundos que aguanta vivo el arbol para poder hacer el pstree

/* Saca un mensaje por pantalla (descriptor 1).
   Lo hago con write y no con printf porque printf deja el texto en un buffer,
   y cuando haces fork el hijo se lleva una copia de ese buffer. Si la salida
   se manda a un fichero los mensajes salen repetidos. */
void imprimir(char *msg) {
    write(1, msg, strlen(msg));
}

/* Devuelve 1 si la cadena son todo digitos y 0 si no (por ejemplo "3abc" da 0) */
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

/* Manejador de SIGALRM. Esta vacio a proposito: lo unico que quiero
   es que el pause() de abajo vuelva cuando salte la alarma */
void despertar(int sig) {
}

/* Esto lo ejecuta cada proceso de la primera fila (p11, p12...).
   Va creando su columna hacia abajo y ya no vuelve al main. */
void bajarColumna(int col, int filas) {
    int f;
    pid_t pid;
    char msg[200];

    sprintf(msg, "Soy p1%d: mi pid es %d. Mi padre es %d\n", col, getpid(), getppid());
    imprimir(msg);

    for (f = 2; f <= filas; f++) {
        pid = fork();
        if (pid == -1) {
            perror("Error en el fork");
            exit(1);
        }

        if (pid > 0) {
            // soy el padre: ya tengo mi hijo, le espero y termino
            wait(NULL);
            exit(0);
        }

        // soy el hijo: digo quien soy y sigo en el bucle para crear al de abajo
        sprintf(msg, "Soy p%d%d: mi pid es %d. Mi padre es %d\n", f, col, getpid(), getppid());
        imprimir(msg);
    }

    // aqui solo llega el ultimo de la columna. Se queda parado ESPERA segundos
    // (alarma + pause) para que de tiempo a lanzar pstree -c desde otra terminal
    signal(SIGALRM, despertar);
    alarm(ESPERA);
    pause();
    exit(0);
}

int main(int argc, char *argv[]) {
    int filas, columnas;
    int c;
    pid_t pid;
    char msg[200];

    // comprobaciones de los argumentos
    if (argc != 3) {
        imprimir("Uso: ./malla <filas> <columnas>\n");
        exit(1);
    }
    if (!soloDigitos(argv[1]) || !soloDigitos(argv[2])) {
        imprimir("Error: las filas y las columnas tienen que ser numeros\n");
        exit(1);
    }

    filas = atoi(argv[1]);
    columnas = atoi(argv[2]);

    if (filas <= 0 || columnas <= 0) {
        imprimir("Error: las filas y las columnas tienen que ser mayores que 0\n");
        exit(1);
    }

    sprintf(msg, "Soy malla: mi pid es %d\n", getpid());
    imprimir(msg);

    // primera fila: todos son hijos de malla, por eso el fork lo hace siempre malla
    for (c = 1; c <= columnas; c++) {
        pid = fork();
        if (pid == -1) {
            perror("Error en el fork");
            exit(1);
        }
        if (pid == 0) {
            bajarColumna(c, filas);   // de aqui el hijo no vuelve
        }
    }

    // malla espera a los hijos de la primera fila (uno por columna)
    for (c = 1; c <= columnas; c++) {
        wait(NULL);
    }

    sprintf(msg, "Soy malla (%d) y muero\n", getpid());
    imprimir(msg);
    return 0;
}
