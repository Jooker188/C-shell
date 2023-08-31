/************* IMPORTATION DES LIBRAIRIES NATIVES NECESSAIRES *************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


/************* DÉFINITION DES CONSTANTES *************/

#define MAX_INPUT_SIZE 1024
#define YELLOW "\033[1;33m"
#define CYAN "\033[1;36m"
#define MAGENTA "\033[1;35m"
#define RED "\033[1;31m"
#define RESET "\033[0m"



/************* DÉFINITION DES VARIABLES GLOBALES *************/

int running = 1;



/************* DÉFINITION DES FONCTIONS *************/

void display_prompt(){
    //on utilise un pointeur car getenv renvoit un pointeur
    char *user = getenv("USER");
    //allocation de la taille de directory
    char directory[MAX_INPUT_SIZE];
    //affectation de la valeur cwd à directory
    getcwd(directory, sizeof(directory));

    //affichage du prompt stylisé
    printf("\n[" YELLOW "%s" CYAN "@" RESET RED "parrot" RESET "]-[" MAGENTA "%s" RESET "]", user, directory);
    printf("\n$ ");
}

char* get_user_input(){
    //allocation de la taille de input
    char input[MAX_INPUT_SIZE];
    //lire l'entrée utilisateur depuis le clavier (stdin) et stocker dans input
    fgets(input, sizeof(input), stdin);
    //supprime le caractère de saut de ligne à la fin
    input[strcspn(input, "\n")] = '\0';

    //si on tape exit, ça termine le processus
    if (strcmp(input, "exit") == 0) {
        exit(0);
    }

    //allocation dynamique de la mémoire de la chaîne et sa copie
    char* input_copy = strdup(input);

    return input_copy;
}

void execute_command(char* command){
    //création d'un processus enfant
    pid_t pid = fork();

    if (pid < 0) {
        perror("Erreur lors du fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { //enfant
        //tableau de pointeurs car execvp attends ça
        // "/bin/bash" : chemin vers executable bash
        // "-c" : spécifie qu'on execute bash avec une commande
        // "command" : représente la commande
        // "NULL" : si pas présent, bug d'adressage execvp
        char *args[] = {"/bin/bash", "-c", command, NULL};

        //execution de bash (args[0]) avec le tableau de pointeurs
        execvp(args[0], args);

        perror("Erreur execvp");
        exit(EXIT_FAILURE);
    } else { //parent
        int status;
        //on attends la fin de l'execution du fils avant de poursuivre
        waitpid(pid, &status, 0);
    }
}

void cd(char* path) {
    chdir(path);
}

void stop() {
    printf("\nSignal SIGINT reçu.\n");
    running = 0;
    exit(0);
}

/************* CORPS PRINCIPAL DU PROGRAMME *************/

int main(int argc, char *argv[]) {

    //si jamais le prog recoit un signal SIGINT, on rentre dans stop() puis on met running sur 0
    signal(SIGINT, stop);
    
    //si y'a des arguments
    if (argc > 1){
        //on les parcours
        for (int i = 1; i < argc; i++) {
            //si l'argument courant est "-c"
            if (strcmp(argv[i], "-c") == 0){
                //on execute la commande avec ce shell bash
                execute_command(argv[i+1]);
                exit(0);
            }
            //sinon on affiche la syntaxe
            else{
                fprintf(stderr, "Usage: %s [-c command]\n", argv[0]);
                exit(0);
            }
        }
    }

    while (running) {
        //affiche du prompt
        display_prompt();
        
        //récupération de l'entrée utilisateur
        char* input = get_user_input();

        if (strcmp(input, "cd") == 0) {
            //répertoire personnel de l'utilisateur
            cd(getenv("HOME"));
        } else if (strncmp(input, "cd ", 3) == 0) {
            //traitement des commandes "cd" avec un chemin spécifié
            //récupération du chemin apres "cd " (3 carac)
            char *path = input + 3;
            cd(path);
        } else {
            //éxécuter les autres commandes avec bash
            execute_command(input);
        }

        //on libère la mémoire allouée par strdup
        free(input);
    }
    
    return 0;
}
