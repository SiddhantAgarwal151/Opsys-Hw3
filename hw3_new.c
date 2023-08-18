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
#define INVALID_GUESS 0
#define WRONG_GUESS 1
#define CORRECT_GUESS 2

// Bundle the variables into a certain format
struct ServerReply{
    char validGuess;
    short guessesRemaining;
    char* result;
};

bool isWordInDictionary(const char *guess, char **dictionary, int dictSize) {
    for (int i = 0; i < dictSize; i++) {
        if (strcmp(guess, dictionary[i]) == 0) {
            return true;
        }
    }
    return false;
}

int generateResult(const char *hiddenWord, const char *guess, char **dictionary, int dictSize, char *result) {
    if (!isWordInDictionary(guess, dictionary, dictSize)) {
        strcpy(result, "?????");
        return INVALID_GUESS;
    }
    char* guessCopy = (char*)calloc(MAX_WORD_LENGTH,sizeof(char));
    char* hiddenWordCopy = (char*)calloc(MAX_WORD_LENGTH,sizeof(char));
    strcpy(guessCopy,guess);
    strcpy(hiddenWordCopy,hiddenWord);

    // Initialize result to "?????"
    strcpy(result, "?????");

    // Step 1: Handle exact matches (uppercase letters)
    for (int i = 0; i < 5; i++) {
        if (guess[i] == hiddenWordCopy[i]) {
            result[i] = hiddenWordCopy[i] - 32;  // Convert to uppercase
            guessCopy[i] = '-';
            hiddenWordCopy[i] = '-';
        }
    }

    // If the guess is valid, return
    if(strcmp(guessCopy,hiddenWordCopy)==0){
        return CORRECT_GUESS;
    }

    // Step 2: Handle lowercase letters (letters in hidden word but not in correct position)
    for (int i = 0; i < 5; i++) {
        if (result[i] != '?' || guessCopy[i] == '-') {
            continue;  // Skip if already matched or already processed
        }
        for (int j = 0; j < 5; j++) {
            if (guess[i] == hiddenWordCopy[j]) {
                result[i] = hiddenWordCopy[j];
                guessCopy[i] = '-';
                hiddenWordCopy[j] = '-';
                break;
            }
        }
    }

    // Step 3: Handle '-' (incorrect letters not in hidden word)
    for (int i = 0; i < 5; i++) {
        if (result[i] == '?' && guessCopy[i] != '-') {
            result[i] = '-';
        }
    }

    // Step 4: Handle duplicates
    for (int i = 0; i < 5; i++) {
        if (guessCopy[i] != '-') {
            for (int j = 0; j < 5; j++) {
                if (guessCopy[i] == hiddenWordCopy[j]) {
                    if (result[j] == '?') {
                        result[j] = hiddenWordCopy[j];
                    }
                }
            }
        }
    }
    return WRONG_GUESS;
}


/* The actual Wordle Logic would be implemented under threads*/
void *handle_client(void * arg, char **dictionary, int num_words){
    int client_sd = *((int *)arg);
    int randomIndex = rand() % num_words;
    char *hiddenWord = *(dictionary + randomIndex);
    pthread_t threadID = pthread_self();
    printf("THREAD %lu: waiting for guess\n",threadID);
    short guessRemaining = MAX_GUESSES;

    //---------------------------Client Waiting Messages----------------------------------
    char* guess = (char*)calloc(MAX_WORD_LENGTH,sizeof(char));
    while(guessRemaining>=0){
        //================================Receiving Data======================================
        printf("THREAD %lu: waiting for guess\n",threadID);
        int bytesRead = recv(client_sd, guess, MAX_WORD_LENGTH, 0);
        if (bytesRead == -1){
            perror("Error receiving data");
            break;
        } 
        else if (bytesRead == 0){
            printf("Client disconnected\n");
            break;
        }
        guess[bytesRead] = '\0';
        printf("THREAD %lu: rcvd guess: %s\n",threadID,guess);
        
        //=================================Process Data=======================================
        // casting every words into lower case
        for (int i = 0; *(guess+i); i++) {
            *(guess+i) = tolower(*(guess+i));
        }
        char* result = (char*)calloc(MAX_WORD_LENGTH,sizeof(char));
        int status = generateResult(hiddenWord,guess,dictionary,num_words,result);

        //================================Process the guess result============================
        
        if(status == CORRECT_GUESS){
            guessRemaining--;
            struct ServerReply reply;
            reply.validGuess = 'Y';
            reply.guessesRemaining = htons(guessRemaining);
            reply.result = result;
            ssize_t bytes_sent = send(client_sd, &reply, sizeof(reply), 0);
            if (bytes_sent == -1) {
                perror("Error sending server reply");
            }
            printf("THREAD %lu: sending reply: %s (%d guesses left)\n",threadID,result,guessRemaining);
            printf("THREAD %lu: game over; word was %s!\n",threadID,result);
            break;
        }
        else if(status == WRONG_GUESS){
            guessRemaining--;
            struct ServerReply reply;
            reply.validGuess = 'Y';
            reply.guessesRemaining = htons(guessRemaining);
            reply.result = result;
            ssize_t bytes_sent = send(client_sd, &reply, sizeof(reply), 0);
            if (bytes_sent == -1) {
                perror("Error sending server reply");
            }
            printf("THREAD %lu: sending reply: %s (%d guesses left)\n",threadID,result,guessRemaining);
        }
        else if(status == INVALID_GUESS){
            struct ServerReply reply;
            reply.validGuess = 'N';
            reply.guessesRemaining = htons(guessRemaining);
            reply.result = result;
            ssize_t bytes_sent = send(client_sd, &reply, sizeof(reply), 0);
            if (bytes_sent == -1) {
                perror("Error sending server reply");
            }
            printf("THREAD %lu: sending reply: %s (%d guesses left)\n",threadID,result,guessRemaining);
        }
    }
    close(client_sd);
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
    char *line = calloc(MAX_WORD_LENGTH,sizeof(char));

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