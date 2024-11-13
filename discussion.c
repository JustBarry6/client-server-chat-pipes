#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define MAX_USERS 10
#define MAX_NAME_LENGTH 20
#define BUFFER_SIZE 256

typedef struct {
    char name[MAX_NAME_LENGTH];
    int tube_lecture;
    int connected;
} User;

typedef struct {
    User users[MAX_USERS];
    int num_users;
} SharedData;

typedef struct {
    int tube_lecture;
    SharedData *shared_data;
} ThreadArgs;

void *lecture(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int tube_lecture = args->tube_lecture;
    SharedData *shared_data = args->shared_data;
    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes_read = read(tube_lecture, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            printf("Message reçu : %s\n", buffer);
        }

        if (strcmp(buffer, "/quitter") == 0) {
            break;
        }

        memset(buffer, 0, BUFFER_SIZE);
    }

    // Déconnexion de l'utilisateur lorsqu'il quitte la fenêtre de dialogue
    for (int i = 0; i < shared_data->num_users; i++) {
        if (shared_data->users[i].connected && shared_data->users[i].tube_lecture == tube_lecture) {
            shared_data->users[i].connected = 0;
            break;
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    // Création du segment de mémoire partagée
    key_t key = ftok(".", 's');
    int shmid = shmget(key, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Erreur lors de la création du segment de mémoire partagée");
        exit(1);
    }

    // Attachement du segment de mémoire partagée
    SharedData *shared_data = (SharedData *)shmat(shmid, NULL, 0);
    if (shared_data == (SharedData *)-1) {
        perror("Erreur lors de l'attachement du segment de mémoire partagée");
        exit(1);
    }

    // Initialisation des données partagées
    shared_data->num_users = 0;

    int tubes[MAX_USERS][2];
    for (int i = 0; i < MAX_USERS; i++) {
        if (pipe(tubes[i]) == -1) {
            perror("Erreur lors de la création du tube");
            exit(1);
        }

        shared_data->users[i].tube_lecture = tubes[i][0];
    }

    pthread_t threads[MAX_USERS];

    for (int i = 0; i < MAX_USERS; i++) {
        ThreadArgs args;
        args.tube_lecture = tubes[i][0];
        args.shared_data = shared_data;

        if (pthread_create(&threads[i], NULL, lecture, &args) != 0) {
            perror("Erreur lors de la création du thread");
            exit(1);
        }
    }

    char message[BUFFER_SIZE];

    while (1) {
        printf("Entrez un message (ou '/quitter' pour quitter) : ");
        fgets(message, BUFFER_SIZE, stdin);

        // Supprimer le saut de ligne à la fin du message
        message[strcspn(message, "\n")] = '\0';

        if (strcmp(message, "/quitter") == 0) {
            break;
        }

        // Envoi du message à tous les utilisateurs connectés
        for (int i = 0; i < shared_data->num_users; i++) {
            if (shared_data->users[i].connected) {
                if (write(shared_data->users[i].tube_lecture, message, strlen(message)) == -1) {
                    perror("Erreur lors de l'écriture dans le tube");
                    exit(1);
                }
            }
        }

        memset(message, 0, BUFFER_SIZE);
    }

    // Attente de la terminaison de tous les threads
    for (int i = 0; i < MAX_USERS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Détachement du segment de mémoire partagée
    if (shmdt(shared_data) == -1) {
        perror("Erreur lors du détachement du segment de mémoire partagée");
        exit(1);
    }

    // Suppression du segment de mémoire partagée
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("Erreur lors de la suppression du segment de mémoire partagée");
        exit(1);
    }

    return 0;
}

