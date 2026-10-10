/*
 * Practica 1 - Ejercicio 1b: ejec
 *
 * Uso: ./ejec segundos
 *
 * Arbol:  ejec -> A -> B -> X, Y, Z
 *
 * Como van las senales:
 *   1. a Z le salta la alarma y manda SIGUSR1 a A
 *   2. A hace el pstree y cuando acaba manda SIGUSR2 a ejec
 *   3. ejec manda SIGUSR2 a A, A se la pasa a B y B va matando a Z, Y y X
 *   4. cada padre espera a su hijo antes de morir
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Todo esto es global porque a un manejador de senal solo le llega el numero
   de la senal, y dentro necesito los pids para poder hacer los kill */
int espera;                       // segundos que me pasan por argumento
pid_t pEjec, pA, pB, pX, pY, pZ;
char quienSoy;                    // solo lo usan X, Y y Z: guardan aqui su letra

/* Saca un mensaje por pantalla (descriptor 1) con write.
   No uso printf porque deja el texto en un buffer que los hijos se copian
   al hacer fork (si se redirige la salida a un fichero salen lineas repetidas).
   Ademas write se puede usar tranquilamente dentro de un manejador. */
void imprimir(char *msg) {
    write(1, msg, strlen(msg));
}

/* Devuelve 1 si la cadena son todo digitos y 0 si no (por ejemplo "15abc" da 0) */
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

/* ---------- manejadores ---------- */

/* Z (SIGALRM): ha saltado la alarma, aviso a A */
void avisarA(int sig) {
    kill(pA, SIGUSR1);
}

/* A (SIGUSR1): toca hacer el pstree */
void lanzarPstree(int sig) {
    pid_t hijo;
    char pidTexto[20];

    // el exec cambia el programa del proceso que lo llama. Si lo hiciera A
    // directamente dejaria de ser A, asi que creo un hijo y que lo haga el
    hijo = fork();
    if (hijo == -1) {
        perror("Error en el fork");
        return;
    }
    if (hijo == 0) {
        sprintf(pidTexto, "%d", pEjec);
        execlp("pstree", "pstree", "-c", pidTexto, NULL);
        perror("Error al ejecutar pstree");   // solo se llega aqui si falla el exec
        exit(1);
    }

    // espero a que acabe el pstree (B sigue vivo, no me puede devolver otro pid)
    while (wait(NULL) != hijo);

    // le digo a ejec que ya esta y que puede empezar a cerrar
    kill(pEjec, SIGUSR2);
}

/* ejec (SIGUSR2): A ya ha hecho el pstree, empiezo a cerrar el arbol */
void iniciarCierre(int sig) {
    kill(pA, SIGUSR2);
}

/* A (SIGUSR2): le paso la orden a B */
void cierreA(int sig) {
    kill(pB, SIGUSR2);
}

/* B (SIGUSR2): mato a mis hijos de uno en uno, en orden Z, Y, X.
   Despues de cada kill hago wait. Como los otros dos siguen en pause,
   el wait solo puede devolver el que acabo de matar. */
void cierreB(int sig) {
    kill(pZ, SIGUSR2);
    wait(NULL);
    kill(pY, SIGUSR2);
    wait(NULL);
    kill(pX, SIGUSR2);
    wait(NULL);
}

/* X, Y y Z (SIGUSR2): digo que muero y termino */
void cierreHoja(int sig) {
    char msg[100];

    sprintf(msg, "Soy %c (%d) y muero\n", quienSoy, getpid());
    imprimir(msg);
    exit(0);
}

/* ---------- creacion del arbol ---------- */

/* La llama B tres veces para crear X, Y y Z.
   A B le devuelve el pid del hijo. El hijo se queda dentro y no vuelve nunca. */
pid_t nuevaHoja(char nombre) {
    pid_t pid;
    char msg[200];

    pid = fork();
    if (pid == -1) {
        perror("Error en el fork");
        exit(1);
    }
    if (pid > 0)
        return pid;   // B sigue con la siguiente hoja

    // a partir de aqui soy X, Y o Z
    quienSoy = nombre;
    signal(SIGUSR2, cierreHoja);
    if (nombre == 'Z')
        signal(SIGALRM, avisarA);

    sprintf(msg, "Soy el proceso %c: mi pid es %d. Mi padre es %d. Mi abuelo es %d. Mi bisabuelo es %d\n",
            nombre, getpid(), getppid(), pA, pEjec);
    imprimir(msg);

    // Z no puede usar sleep, asi que se pone una alarma
    if (nombre == 'Z')
        alarm(espera);

    // me quedo parado hasta que me manden morir
    while (1)
        pause();
}

/* La llama A. A vuelve enseguida y B se queda dentro hasta que muere. */
void crearB() {
    char msg[200];

    pB = fork();
    if (pB == -1) {
        perror("Error en el fork");
        exit(1);
    }
    if (pB > 0)
        return;   // esto es A, que vuelve al main

    // a partir de aqui soy B
    pB = getpid();
    signal(SIGUSR2, cierreB);
    sprintf(msg, "Soy el proceso B: mi pid es %d. Mi padre es %d. Mi abuelo es %d\n",
            pB, getppid(), pEjec);
    imprimir(msg);

    pX = nuevaHoja('X');
    pY = nuevaHoja('Y');
    pZ = nuevaHoja('Z');

    // B se queda aqui esperando. A sus tres hijos los recoge el manejador cierreB,
    // asi que cuando wait devuelve -1 es que ya no le queda ninguno
    while (wait(NULL) != -1);

    sprintf(msg, "Soy B (%d) y muero\n", pB);
    imprimir(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    char msg[200];

    // comprobaciones del argumento
    if (argc != 2) {
        imprimir("Uso: ./ejec <segundos>\n");
        exit(1);
    }
    if (!soloDigitos(argv[1]) || atoi(argv[1]) <= 0) {
        imprimir("Error: los segundos tienen que ser un numero mayor que 0\n");
        exit(1);
    }
    espera = atoi(argv[1]);

    pEjec = getpid();
    sprintf(msg, "Soy el proceso ejec: mi pid es %d\n", pEjec);
    imprimir(msg);

    // pongo el manejador antes del fork para que la senal no pueda llegar sin el
    signal(SIGUSR2, iniciarCierre);

    pA = fork();
    if (pA == -1) {
        perror("Error en el fork");
        exit(1);
    }

    if (pA == 0) {
        // soy A
        pA = getpid();
        signal(SIGUSR1, lanzarPstree);
        signal(SIGUSR2, cierreA);
        sprintf(msg, "Soy el proceso A: mi pid es %d. Mi padre es %d\n", pA, getppid());
        imprimir(msg);

        crearB();

        // espero a B. Lo meto en un while porque si llega una senal mientras
        // estoy en el wait, el wait vuelve sin que B haya muerto todavia
        while (wait(NULL) != pB);

        sprintf(msg, "Soy A (%d) y muero\n", pA);
        imprimir(msg);
        exit(0);
    }

    // soy ejec: espero a A, con el while por lo mismo que antes
    while (wait(NULL) != pA);

    sprintf(msg, "Soy ejec (%d) y muero\n", pEjec);
    imprimir(msg);
    return 0;
}
