/*
 * Practica 1 - Ejercicio 3: hijos
 *
 * Uso: ./hijos x y
 *
 * Arbol:  hijos (superpadre) -> p1 -> p2 -> ... -> px
 *         y de px cuelgan y hijos (los subhijos: px1, px2 ... pxy)
 *
 * El superpadre tiene que escribir los pids de los subhijos, pero esos procesos
 * estan muy por debajo de el y no tiene forma de saberlos. Por eso todos los
 * pids se apuntan en memoria compartida. Guardo x + y enteros:
 *
 *    lista[0]  ...  lista[x-1]       pids de la cadena (p1 ... px)
 *    lista[x]  ...  lista[x+y-1]     pids de los subhijos (px1 ... pxy)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int shmid;            // identificador de la memoria compartida
int *lista = NULL;    // puntero a la memoria compartida (la tabla de pids)

/* Devuelve 1 si la cadena son todo digitos y 0 si no */
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

/* Crea la memoria compartida con sitio para n enteros y la engancha a "lista".
   Devuelve 0 si va bien y 1 si falla. (Es como el creaComp del guion de practicas) */
int creaComp(int n) {
    int k;

    if ((shmid = shmget(IPC_PRIVATE, n * sizeof(int), IPC_CREAT | 0666)) == -1) {
        perror("Error al crear memoria compartida");
        return 1;
    }

    // vinculo el segmento al proceso: shmat me da el puntero para usarlo
    lista = (int *) shmat(shmid, 0, 0);
    if (lista == (int *) -1) {
        perror("Error al vincular la memoria compartida");
        shmctl(shmid, IPC_RMID, 0);
        lista = NULL;
        return 1;
    }

    // lo dejo todo a 0 para empezar
    for (k = 0; k < n; k++)
        lista[k] = 0;

    return 0;
}

/* Desvincula la memoria compartida y la borra del sistema.
   Solo la llama el superpadre, cuando ya ha terminado todo */
void borraComp(void) {
    if (lista != NULL) {
        if (shmdt((char *) lista) < 0) {
            perror("Error al desligar la memoria compartida");
            exit(3);
        }
        if (shmctl(shmid, IPC_RMID, 0) < 0) {
            perror("Error al borrar la memoria compartida");
            exit(4);
        }
        lista = NULL;
    }
}

/* Esto lo ejecuta px (el ultimo de la cadena): crea los y subhijos,
   les espera y termina. No vuelve al main. */
void crearSubhijos(int x, int y) {
    int s, k;
    pid_t pid;

    for (s = 0; s < y; s++) {
        pid = fork();
        if (pid == -1) {
            perror("Error en el fork");
            exit(1);
        }

        if (pid == 0) {
            // soy un subhijo: apunto mi pid para que lo pueda leer el superpadre...
            lista[x + s] = getpid();

            // ...y escribo los pids de la cadena, que ya estan apuntados
            printf("Soy el subhijo %d, mis padres son: ", getpid());
            for (k = 0; k < x; k++) {
                printf("%d", lista[k]);
                if (k < x - 1)
                    printf(", ");
            }
            printf("\n");
            exit(0);
        }
    }

    // px espera a todos sus subhijos antes de morir
    for (s = 0; s < y; s++) {
        wait(NULL);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    int x, y;
    int n, k, status;
    pid_t pid;

    // comprobaciones de los argumentos
    if (argc != 3) {
        printf("Uso: ./hijos <x> <y>\n");
        exit(1);
    }
    if (!soloDigitos(argv[1]) || !soloDigitos(argv[2]) || atoi(argv[1]) <= 0 || atoi(argv[2]) <= 0) {
        printf("Error: x e y tienen que ser numeros mayores que 0\n");
        exit(1);
    }
    x = atoi(argv[1]);
    y = atoi(argv[2]);

    // la memoria hay que crearla antes de hacer ningun fork, asi la heredan todos
    if (creaComp(x + y) != 0) {
        exit(1);
    }

    // la cadena p1 -> p2 -> ... -> px
    // el que hace el fork (el padre) se sale del bucle, y el hijo apunta su pid
    // y da otra vuelta para crear al siguiente
    for (n = 0; n < x; n++) {
        pid = fork();
        if (pid == -1) {
            perror("Error en el fork");
            if (n == 0)
                borraComp();   // si el que falla es el superpadre, que borre la memoria
            exit(1);
        }
        if (pid > 0)
            break;

        lista[n] = getpid();
    }

    // el unico que acaba el bucle entero (n == x) es px
    if (n == x) {
        crearSubhijos(x, y);
    }

    // aqui llegan el superpadre (n == 0) y los de la cadena menos px.
    // cada uno espera a su unico hijo. Asi nadie muere antes que sus hijos y,
    // cuando al superpadre le vuelve el wait, todos los subhijos ya han
    // apuntado su pid en la memoria
    wait(&status);
    if (n > 0) {
        exit(status != 0);   // si ha habido error lo paso hacia arriba
    }

    // a partir de aqui solo queda el superpadre
    if (status != 0) {
        printf("Error: no se ha podido crear el arbol completo\n");
        borraComp();
        exit(1);
    }

    printf("Soy el superpadre (%d) : mis hijos finales son: ", getpid());
    for (k = 0; k < y; k++) {
        printf("%d", lista[x + k]);
        if (k < y - 1)
            printf(", ");
    }
    printf("\n");

    borraComp();
    return 0;
}
