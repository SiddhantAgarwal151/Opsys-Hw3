#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h> 


// Global variables (already declared in hw3-main.c)
extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char **words;

// Constants
#define MAX_WORD_LENGTH 6
#define MAX_GUESSES 6


/* The actual Wordle Logic would be implemented under threads*/
void *handle_client(void * arg){
    int client_fd = *((int *)arg);
    pthread_t threadID = pthread_self();
    printf("THREAD %lu: waiting for guess\n",threadID);
    short guessRemaining = MAX_GUESSES;

    //---------------------------Client Waiting Messages----------------------------------
    char* buffer = (char*)calloc(MAX_WORD_LENGTH,sizeof(char));
    while(1){
        //================================Receiving Data======================================
        printf("THREAD %lu: waiting for guess\n",threadID);
        int bytesRead = recv(client_fd, buffer, MAX_WORD_LENGTH, 0);
        if (bytesRead == -1){
            perror("Error receiving data");
            break;
        } 
        else if (bytesRead == 0){
            printf("Client disconnected\n");
            break;
        }
        buffer[bytesRead] = '\0';
        printf("THREAD %lu: rcvd guess: %s\n",threadID,buffer);

        //=================================Process Data=======================================
        // casting every words into lower case
        for (int i = 0; *(buffer+i); i++) {
            *(buffer+i) = tolower(*(buffer+i));
        }
        

    }
}


int wordle_server(int argc, char **argv){
    // Parse Command Inputs
    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: %s <listener-port> <seed> <dictionary-filename> <num-words>\n", *(argv + 0));
        return EXIT_FAILURE;
    }


    // Extract the listener port number, seed, dictionary filename, and number of words from argv.
    short listener_port = (short)atoi(*(argv + 1));
    int seed = atoi(*(argv + 2));
    char *dictionary_filename = *(argv + 3);
    int num_words = atoi(*(argv + 4));

    srand(seed);

    // Store valid words into dictionary
    char **dictionary = (char **)calloc(num_words + 1,sizeof(char *)); // +1 for NULL pointer
    if (!dictionary) {
        fprintf(stderr, "ERROR: Memory allocation failed for dictionary\n");
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(dictionary_filename, "r");
    if (!file) {
        fprintf(stderr, "ERROR: Unable to open dictionary file: %s\n", dictionary_filename);
        exit(EXIT_FAILURE);
    }
    printf("MAIN: opened %s (%d words)\n",dictionary_filename,num_words);
    printf("MAIN: seeded pseudo-random number generator with %d\n",seed);

    int i = 0;
    char *line = calloc(MAX_WORD_LENGTH+1,sizeof(char));

    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the end of the word.
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        // Allocate memory for the word and copy it into the dictionary.
        *(dictionary + i) = strdup(line);
        if (!*(dictionary + i)) {
            fprintf(stderr, "ERROR: Memory allocation failed for dictionary word\n");
            exit(EXIT_FAILURE);
        }
        i++;
    }
    free(line);
    fclose(file);
    *(dictionary + i) = NULL; // Set the last entry to NULL for easier iteration.


    //--------------------------Network Setup---------------------------------------------------
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to a specific address and port
    struct sockaddr_in tcp_server;
    tcp_server.sin_family = AF_INET;    // IPv4
    tcp_server.sin_addr.s_addr = htonl(INADDR_ANY); //Accpeting any Incoming IP
    tcp_server.sin_port = htons(listener_port);     // Setup port

    if (bind(listener, (struct sockaddr *)&tcp_server, sizeof(tcp_server)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(listener, 10) == -1) {
        perror("Error listening");
        exit(EXIT_FAILURE);
    }
    printf("MAIN: Wordle server listening on port {%d}\n",listener_port);

    //-----------------------------Receive Client Connection----------------------------------
    while(1){
        // Accept an incoming connection
        struct sockaddr_in remote_client;
        int addrlen = sizeof( remote_client );
        int newsd = accept( listener, (struct sockaddr *)&remote_client,(socklen_t *)&addrlen );
        if (newsd == -1){
            perror( "accept() failed" ); continue; 
        }
        printf("MAIN: rcvd incoming connection request\n");

    //---------------------------Application Layer Multithread----------------------------------
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, &newsd) != 0) {
            perror("Error creating thread");
            close(newsd);
            continue;
        }

        // Detach the child thread to make it independent of the main thread
        pthread_detach(thread);

    }






}