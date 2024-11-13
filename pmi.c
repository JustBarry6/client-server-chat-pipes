#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define SHM_SIZE 1024
#define MAX_USERS 10
#define MAX_NAME_LENGTH 20

typedef struct {
    int connected;
    char name[MAX_NAME_LENGTH];
} UserData;

typedef struct {
    int num_users;
    UserData users[MAX_USERS];
} SharedData;

int enregistrer(SharedData *shared_data, char *nom);
int parler(SharedData *shared_data, char *nom);
void lister_connectes(SharedData *shared_data);
void deconnecter(SharedData *shared_data, char *nom);

int main(int argc, char **argv) {
    key_t key;
    int shmid;
    SharedData *shared_data;

    // Générer la clé pour le segment de mémoire partagée
    key = ftok(".", 'S');
    if (key == -1) {
        perror("Erreur lors de la génération de la clé");
        exit(1);
    }

    // Créer ou attacher le segment de mémoire partagée
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Erreur lors de la création ou de l'attachement du segment de mémoire partagée");
        exit(1);
    }

    // Attacher le segment de mémoire partagée à l'espace d'adressage du processus
    shared_data = (SharedData *) shmat(shmid, NULL, 0);
    if (shared_data == (SharedData *) -1) {
        perror("Erreur lors de l'attachement du segment de mémoire partagée");
        exit(1);
    }

    // Initialiser les utilisateurs connectés
    shared_data->num_users = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        shared_data->users[i].connected = 0;
        strcpy(shared_data->users[i].name, "");
    }

    // Interpréter les commandes jusqu'à la réception de la commande "q"
    char commande[3];
    while (1) {
        printf("Commande : ");
        scanf("%s", commande);

        if (strcmp(commande, "e") == 0) {
            char nom[MAX_NAME_LENGTH];
            scanf("%s", nom);
            enregistrer(shared_data, nom);
        } else if (strcmp(commande, "p") == 0) {
            char nom[MAX_NAME_LENGTH];
            scanf("%s", nom);
            parler(shared_data, nom);
        } else if (strcmp(commande, "l") == 0) {
            lister_connectes(shared_data);
        } else if (strcmp(commande, "d") == 0) {
            char nom[MAX_NAME_LENGTH];
            scanf("%s", nom);
            deconnecter(shared_data, nom);
        } else if (strcmp(commande, "q") == 0) {
            break;
        } else {
            printf("Commande invalide\n");
        }
    }

    // Détacher le segment de mémoire partagée
    if (shmdt(shared_data) == -1) {
        perror("Erreur lors du détachement du segment de mémoire partagée");
        exit(1);
    }

    // Supprimer le segment de mémoire partagée
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("Erreur lors de la suppression du segment de mémoire partagée");
        exit(1);
    }

    return 0;
}

int enregistrer(SharedData *shared_data, char *nom) {
    if (shared_data->num_users == MAX_USERS) {
        printf("Le nombre maximum d'utilisateurs est atteint\n");
        return -1;
    }

    for (int i = 0; i < shared_data->num_users; i++) {
        if (strcmp(shared_data->users[i].name, nom) == 0) {
            printf("Utilisateur déjà enregistré\n");
            return -1;
        }
    }

    strcpy(shared_data->users[shared_data->num_users].name, nom);
    shared_data->users[shared_data->num_users].connected = 1;
    shared_data->num_users++;

    printf("Utilisateur enregistré avec succès\n");
    return 0;
}

int parler(SharedData *shared_data, char *nom) {
    int user_index = -1;
    for (int i = 0; i < shared_data->num_users; i++) {
        if (strcmp(shared_data->users[i].name, nom) == 0) {
            user_index = i;
            break;
        }
    }

    if (user_index == -1 || !shared_data->users[user_index].connected) {
        printf("Utilisateur non trouvé ou déconnecté\n");
        return -1;
    }

    printf("Ouverture de la fenêtre de dialogue avec %s\n", nom);

    // Créer un tube pour la communication bidirectionnelle
    int tube[2];
    if (pipe(tube) == -1) {
        perror("Erreur lors de la création du tube de communication");
        exit(1);
    }

    // Code pour lancer une fenêtre de dialogue avec l'utilisateur nom
    pid_t pid = fork();

    if (pid == -1) {
        perror("Erreur lors de la création du processus de dialogue");
        exit(1);
    } else if (pid == 0) {
        // Code du processus de dialogue

        // Fermer l'extrémité d'écriture du tube
        close(tube[1]);

        // Rediriger l'entrée standard vers l'extrémité de lecture du tube
        dup2(tube[0], STDIN_FILENO);

        // Exécuter le code de communication
        execlp("xterm", "xterm", "-e", "./discussion", NULL);

        // En cas d'échec de l'exécution de la commande
        perror("Erreur lors de l'exécution de la commande de dialogue");
        exit(1);
    } else {
        // Code du processus principal

        // Fermer l'extrémité de lecture du tube
        close(tube[0]);

        // Envoyer le descripteur de fichier de l'extrémité d'écriture du tube au processus de dialogue
        if (write(tube[1], &tube[1], sizeof(int)) == -1) {
            perror("Erreur lors de l'envoi du descripteur de fichier du tube");
            exit(1);
        }

        // Attendre la fin du processus de dialogue
        wait(NULL);
    }

    return 0;
}

void lister_connectes(SharedData *shared_data) {
    printf("Utilisateurs connectés :\n");
    for (int i = 0; i < shared_data->num_users; i++) {
        if (shared_data->users[i].connected) {
            printf("- %s\n", shared_data->users[i].name);
        }
    }
}

void deconnecter(SharedData *shared_data, char *nom) {
    int user_index = -1;
    for (int i = 0; i < shared_data->num_users; i++) {
        if (strcmp(shared_data->users[i].name, nom) == 0) {
            user_index = i;
            break;
        }
    }

    if (user_index == -1 || !shared_data->users[user_index].connected) {
        printf("Utilisateur non trouvé ou déjà déconnecté\n");
        return;
    }

    shared_data->users[user_index].connected = 0;

    printf("Utilisateur déconnecté avec succès\n");
}

