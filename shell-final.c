/************* IMPORTATION DES LIBRAIRIES NATIVES NECESSAIRES *************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>



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


//affiche le prompt du shell
//les couleurs sont utilisées pour le rendre plus lisible.
void display_prompt(){
    char *user = getenv("USER");
    char directory[MAX_INPUT_SIZE];
    getcwd(directory, sizeof(directory));

    printf("\n[" YELLOW "%s" CYAN "@" RESET RED "parrot" RESET "]-[" MAGENTA "%s" RESET "]", user, directory);
    printf("\n$ ");
}

//cette fonction gère les redirections. 
//elle recherche ces opérateurs (> et >>) dans la liste des arguments de la commande et
//effectue les redirections de fichier appropriées en utilisant les appels système open et dup2.
void handle_redirections(char **args) {

    //args : pointeur de pointeur car c'est une liste d'arguments, où chaque argument est représenté par une chaîne de caractères (donc un pointeur) 
    //et cette liste d'arguments est elle-même stockée dans un tableau (donc un pointeur de pointeur).

    //initialisation descripteur de fichier (stdin) 
    int input_fd = -1;
    //initialisation descripteur de fichier (stdout)
    int output_fd = -1;

    //parcours des args
    for (int i = 0; args[i] != NULL; i++) {

        //check si elem courrant est >
        if (strcmp(args[i], ">") == 0) {
            //vérifie si elem valide après >
            if (args[i + 1] != NULL) {
                //ouverture du fichier en écriture et écrase les données présents
                //maj du output_fd pour qu'il pointe vers ce fichier
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (output_fd == -1) {
                    //si y'a eu de maj c'est resté a -1 donc problème détecté
                    perror("Erreur : ouverture impossible du fichier de sortie");
                    exit(EXIT_FAILURE);
                }
                //suppression de >
                args[i] = NULL;
                //pour passer a elem suivant
                i++;
            } else {
                fprintf(stderr, "Erreur : nom de fichier manquant après '>'\n");
                exit(EXIT_FAILURE);
            }
        }

        if (strcmp(args[i], ">>") == 0) {
            if (args[i + 1] != NULL) {
                //ouverture du fichier en écriture et écrit après les données présents
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);
                if (output_fd == -1) {
                    perror("Erreur : ouverture impossible du fichier de sortie");
                    exit(EXIT_FAILURE);
                }
                args[i] = NULL;
                i++;
            } else {
                fprintf(stderr, "Erreur : nom de fichier manquant après '>'\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    //si le fichier d'entrée a été ouvert
    if (input_fd != -1) {
        //rediriger l'entrée standard (stdin) vers le descripteur de fichier input_fd.
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            //si erreur
            perror("Erreur : redirection de l'entrée standard impossible");
            exit(EXIT_FAILURE);
        }
        close(input_fd);
    }

    //si le fichier de sortie a été ouvert
    if (output_fd != -1) {
        //rediriger la sortie standard (stdout) vers le descripteur de fichier output_fd.
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            //si erreur
            perror("Erreur lors de la redirection de la sortie standard");
            exit(EXIT_FAILURE);
        }
        close(output_fd);
    }
}

//xécute une commande donnée en utilisant l'appel système execvp
void execute_command(char **args) {
    execvp(args[0], args);
    perror("Erreur lors de l'exécution de la commande");
    exit(EXIT_FAILURE);
}

//cette fonction gère les pipes (|) dans une commande. 
//elle divise la commande en deux parties distinctes, crée un tube (pipe), et exécute les deux parties de la commande en utilisant un processus enfant. 
//les données sont redirigées entre les processus à l'aide de dup2.
void handle_pipes(char **args) {
    //on parcours les args
    for (int i = 0; args[i] != NULL; i++) {
        //si elem courrant est |
        if (strcmp(args[i], "|") == 0) {
            //diviser le tableau args en deux parties distinctes 
            //first_command contient la première commande 
            //second_command contient la seconde commande.

            args[i] = NULL; //séparateur
            char **first_command = args;
            char **second_command = &args[i + 1];

            //tab de 2 descripteurs de fichier
            //ces descripteurs de fichier seront utilisés pour créer le tube.
            int pipe_fd[2];
            //créer un tube et les descripteurs de fichier sont stockés dans pipe_fd
            if (pipe(pipe_fd) == -1) {
                perror("Erreur : création impossible de la pipe");
                exit(EXIT_FAILURE);
            }

            //processus enfant va executer la seconde commande
            pid_t second_pid = fork();
            if (second_pid == 0) {
                //ferme le descripteur de fichier en écriture
                close(pipe_fd[1]);
                //on redirige l'entrée standard (STDIN_FILENO) pour lire depuis le descripteur de fichier en lecture pipe_fd[0]
                //signifie que la deuxième commande lira ses données d'entrée depuis le tube.
                if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
                    perror("Erreur lors de la redirection de l'entrée standard depuis le tube");
                    exit(EXIT_FAILURE);
                }
                //ferme le descripteur de fichier en lecture
                close(pipe_fd[0]);
                execute_command(second_command);
            } 
            //ferme le descripteur de fichier en lecture
            close(pipe_fd[0]);
            //on redirige la sortie standard (STDOUT_FILENO) vers le descripteur de fichier en écriture pipe_fd[1]
            //signifie que la première commande écrira sa sortie dans le tube.
            if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
                perror("Erreur lors de la redirection de la sortie standard vers le tube");
                exit(EXIT_FAILURE);
            }
            //ferme le descripteur de fichier en écriture
            close(pipe_fd[1]);
            execute_command(first_command);
        }
    }
}

//cette fonction gère l'exécution globale d'une commande. 
//elle crée un processus enfant pour exécuter la commande, gère les redirections et les pipes si nécessaire, et attend que le processus enfant se termine.
void execute_command_global(char **args) {
    int status;

    pid_t pid = fork();
    if (pid == 0) {
        //code du processus enfant
        handle_redirections(args);
        handle_pipes(args);
        execute_command(args);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("Erreur : création du processus fils impossible");
    } else {
        //code du processus parent
        //attendre la fin de l'exécution du fils avant de poursuivre
        waitpid(pid, &status, 0);
    }
}

//cette fonction est appelée en cas de réception du signal SIGINT 
//elle affiche un message et met fin au shell en définissant running sur 0.
void stop() {
    printf("\nSignal SIGINT reçu.\n");
    running = 0;
    exit(0);
}

//cette fonction change le répertoire courant du shell en utilisant l'appel système chdir
void cd(char* path) {
    chdir(path);
}



/************* CORPS PRINCIPAL DU PROGRAMME *************/


//commence par définir un gestionnaire de signal pour SIGINT (Ctrl+C) pour gérer proprement l'arrêt du shell
//lit en boucle les commandes de l'utilisateur, les analyse, et les exécute
//le shell affiche le prompt, lit l'entrée de l'utilisateur pour exécuter des commandes internes (cd) ou des commandes externes
//continue jusqu'à ce que running soit défini sur 0 
int main(int argc, char *argv[]) {
    signal(SIGINT, stop);

    //utilisé pour stocker l'entree user
    char input[1024];
    //tableau de pointeurs qui sera utilisé pour stocker les arguments extraits de input.
    char *args[100];
    //utilisé pour parcourir le tab d'args
    char *arg;
    //utilisé pour compter le nbr d'arg dans le ligne
    int arg_count;

    //si il y a des arguments et que le premier est -c
    if (argc > 1 && strcmp(argv[1], "-c") == 0) {
        ///et qu'il y a quelque chose après -c
        if (argc > 2) {
            //command pointe vers le 3e element du tableau de pointeur argv qui est l'input user
            char *command = argv[2];
            //tableau de pointeurs qui sera utilisé pour stocker les arguments extraits de command.
            char *args[100]; 
            int arg_count = 0;

            //parsing de la command a l'aide du delimiteur " ", strtok renvoit un pointeur
            char *arg = strtok(command, " ");

            //préparation du tableau d'argument
            //on traite chaque element parsé
            while (arg != NULL) {
                //on l'ajoute au tableau
                args[arg_count] = arg;
                //on avance
                arg_count++;
                //on prends l'element suivant
                arg = strtok(NULL, " ");
            }
            args[arg_count] = NULL;

            //exécution la commande
            execute_command_global(args);

        } else {
            fprintf(stderr, "Erreur : l'option -c nécessite une commande à exécuter.\n");
            return 1;
        }
    } else {
        //si l'option -c n'est pas utilisée
        while (running) {
            display_prompt();
            //on lit l'entrée standard et on stock dans input
            fgets(input, sizeof(input), stdin);
            //on supprime le dernier caractere
            input[strlen(input) - 1] = '\0';

            arg_count = 0;
            //parsing de la command a l'aide du delimiteur " "
            arg = strtok(input, " ");
            while (arg != NULL) {
                //on l'ajoute au tableau
                args[arg_count] = arg;
                //on avance
                arg_count++;
                //on prends l'element suivant
                arg = strtok(NULL, " ");
            }
            args[arg_count] = NULL;

            //si args
            if (arg_count > 0) {
                //si c'est exit on se tire
                if (strcmp(args[0], "exit") == 0) {
                    exit(0);
                } else if (strcmp(args[0], "cd") == 0) {
                    //si c'est cd uniquement
                    if (arg_count < 2) {
                        cd(getenv("HOME"));
                    }
                    //sinon
                    else{
                        cd(args[1]);
                    }
                } else {
                    //sinon c'est une commande basique
                    execute_command_global(args);
                }
            }
        }
    }

    return 0;
}