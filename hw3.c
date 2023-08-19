#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <signal.h> // Add this header for signal handling
#include <fcntl.h>
#include <errno.h>

// Constants
#define MAX_WORD_LENGTH 6
#define MAX_GUESSES 6
#define INVALID_GUESS 0
#define WRONG_GUESS 1
#define CORRECT_GUESS 2

// // Bundle the variables into a certain format
// struct ServerReply{
//     char validGuess;
//     short guessesRemaining;
//     char* result;
// };

typedef struct{
    int client_sd;
    char** dictionary;
    int numWords;
}threadArgs;
// Global variables (already declared in hw3-main.c)
extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char **words;
// Global flag to control the server loop
volatile int signal_received = 0;

bool isWordInDictionary(const char *guess, char **dictionary, int dictSize) {
    for (int i = 0; i < dictSize; i++) {
        if (strncmp(guess, *(dictionary + i),MAX_WORD_LENGTH-1) == 0) {
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
        if (*(guess + i) == *(hiddenWordCopy + i)) {
            *(result + i) = *(hiddenWordCopy + i) - 32;  // Convert to uppercase
            *(guessCopy + i) = '-';
            *(hiddenWordCopy + i) = '-';
        }
    }

    // If the guess is valid, return
    if(strncmp(guessCopy,hiddenWordCopy,MAX_WORD_LENGTH-1)==0){
        printf("yes\n");
        free(guessCopy);
        free(hiddenWordCopy);
        return CORRECT_GUESS;
    }

    // Step 2: Handle lowercase letters (letters in hidden word but not in correct position)
    for (int i = 0; i < 5; i++) {
        if (*(result + i) != '?' || *(guessCopy + i) == '-') {
            continue;  // Skip if already matched or already processed
        }
        for (int j = 0; j < 5; j++) {
            if (*(guess +i) == *(hiddenWordCopy + j)) {
                *(result + i) = *(hiddenWordCopy + j);
                *(guessCopy + i) = '-';
                *(hiddenWordCopy + j) = '-';
                break;
            }
        }
    }

    // Step 3: Handle '-' (incorrect letters not in hidden word)
    for (int i = 0; i < 5; i++) {
        if (*(result + i) == '?' && *(guessCopy + i) != '-') {
            *(result + i) = '-';
        }
    }

    // Step 4: Handle duplicates
    for (int i = 0; i < 5; i++) {
        if (*(guessCopy + i) != '-') {
            for (int j = 0; j < 5; j++) {
                if (*(guessCopy + i) == *(hiddenWordCopy + j)) {
                    if (*(result + j) == '?') {
                        *(result + j) = *(hiddenWordCopy + j);
                    }
                }
            }
        }
    }
    free(guessCopy);
    free(hiddenWordCopy);
    return WRONG_GUESS;
}


/* The actual Wordle Logic would be implemented under threads*/
void *handle_client(void * arg){
    threadArgs* args = (threadArgs*) arg;
    int client_sd = args->client_sd;
    char** dictionary = args->dictionary;
    int num_words = args->numWords;
    int randomIndex = rand() % num_words;
    char* hiddenWord = calloc(MAX_WORD_LENGTH,sizeof(char));
    strcpy(hiddenWord, *(dictionary + randomIndex));
    //hiddenWord = "sonic";
    pthread_t threadID = pthread_self();
    short guessRemaining = MAX_GUESSES;

    printf("%s\n",hiddenWord);
    //---------------------------Client Waiting Messages----------------------------------
    while(guessRemaining>=0){
        //================================Receiving Data======================================
        printf("THREAD %lu: waiting for guess\n",threadID);
        char* guess = (char*)calloc(MAX_WORD_LENGTH,sizeof(char));
        char* result = (char*)calloc(MAX_WORD_LENGTH,sizeof(char));
        char* response = calloc(8, sizeof(char)); // Adjust the buffer size as needed
        int bytesRead = recv(client_sd, guess, MAX_WORD_LENGTH, 0);
        if (bytesRead == -1){
            perror("Error receiving data");
            break;
        } 
        else if (bytesRead == 0){
            printf("Client disconnected\n");
            break;
        }
        *(guess + bytesRead) = '\0';
        printf("THREAD %lu: rcvd guess: %s\n",threadID,guess);
        
        //=================================Process Data=======================================
        // casting every words into lower case
        for (int i = 0; *(guess+i); i++) {
            *(guess+i) = tolower(*(guess+i));
        }
        int status = generateResult(hiddenWord,guess,dictionary,num_words,result);
        //================================Process the guess result============================
        
        if(status == CORRECT_GUESS){
            guessRemaining--;
            // struct ServerReply reply;
            // reply.validGuess = 'Y';
            // reply.guessesRemaining = htons(guessRemaining);
            // reply.result = result;
            *(response + 0) = 'Y'; 
            strncpy(response + 3, result, 5); // Copy 5 byte result word  
            *(short *)(response + 1) = htons(guessRemaining); // 2 byte remaining guesses
            ssize_t bytes_sent = send(client_sd, response, sizeof(response), 0);
            if (bytes_sent == -1) {
                perror("Error sending server reply");
            }
            printf("THREAD %lu: sending reply: %s (%d guesses left)\n",threadID,result,guessRemaining);
            printf("THREAD %lu: game over; word was %s!\n",threadID,result);
            break;
        }
        else if(status == WRONG_GUESS){
            guessRemaining--;
            *(response + 0) = 'Y'; 
            strncpy(response + 3, result, 5); // Copy 5 byte result word  
            *(short *)(response + 1) = htons(guessRemaining); // 2 byte remaining guesses
            ssize_t bytes_sent = send(client_sd, response, sizeof(response), 0);
            if (bytes_sent == -1) {
                perror("Error sending server reply");
            }
            printf("THREAD %lu: sending reply: %s (%d guesses left)\n",threadID,result,guessRemaining);
            if (guessRemaining == 0) {
                printf("THREAD %ld: game over; word was %s!\n", pthread_self(), hiddenWord);
            }
        }
        else if(status == INVALID_GUESS){
            *(response + 0) = 'N'; 
            strncpy(response + 3, result, 5); // Copy 5 byte result word  
            *(short *)(response + 1) = htons(guessRemaining); // 2 byte remaining guesses
            ssize_t bytes_sent = send(client_sd, response, sizeof(response), 0);
            if (bytes_sent == -1) {
                perror("Error sending server reply");
            }
            printf("THREAD %lu: sending reply: %s (%d guesses left)\n",threadID,result,guessRemaining);
        }
        // if (result.lower() == hiddenWord) {
        //     printf("THREAD %ld: game over; word was %s!\n", pthread_self(), hiddenWord);
        // }
        free(guess);
        free(response);
        free(result);
    }
    
    // Free memory  
    
    free(hiddenWord);
    close(client_sd);
}


// void *handle_client(void *arg, char **dictionary, int num_words) {
//     int client_socket = *((int *)arg);
//     // Select a random word from the dictionary using the seeded random number generator
//     int random_index = rand() % num_words;
//     char *target_word = *(dictionary + random_index);
//     int guesses = 0; // Initialize guesses for this thread
//     bool game_over = false;

//     while (!game_over) {

//     // Receive a guess from the client
//     char *guess = calloc (MAX_WORD_LENGTH + 1, sizeof(char));
//     ssize_t bytes_received = recv(client_socket, guess, sizeof(guess) - 1, 0);
    
//     if (bytes_received <= 0) {
//         // Handle client disconnect or error
//         close(client_socket);
//         printf("SERVER: Client disconnected\n");
//         continue;
//     }
//     *(guess + bytes_received) = '\0'; // Null-terminate the received data

//     // ... Accept an incoming connection and receive a guess from the client ...
//     // Process the guess and send response
//     char *response = calloc(8, sizeof(char)); // Adjust the buffer size as needed
//     // Implement game logic and generate response here
//     // ... Implement game logic and generate response here ...

//     bool is_valid_guess = valid_guess(guess, dictionary, num_words);
//     int correct_guess_count = strcmp(guess, target_word) == 0 ? MAX_WORD_LENGTH : partial_match(guess, target_word);
   
//     // Calculate the remaining guesses based on the total guesses and the guesses made by the client
    
    
//     // Generate the response based on the guess and the target word
//     char *response_word = calloc(MAX_WORD_LENGTH + 1, sizeof(char));
//     for (int i = 0; i < MAX_WORD_LENGTH; i++) {
//         if (*(guess + i) == *(target_word + i)) {
//             *(response_word + i) = toupper(*(guess + i)); // Uppercase for correct position
//         } else if (strchr(target_word, *(guess + i))) {
//             *(response_word + i) = tolower(*(guess + i)); // Lowercase for correct letter in wrong position
//         } else {
//             *(response_word + i) = '-'; // Dash for incorrect letter
//         }
//     }
//     *(response_word + MAX_WORD_LENGTH) = '\0';

//     *(response + 0) = is_valid_guess ? 'Y' : 'N'; // Valid guess indicator
//     if (is_valid_guess) {
//         guesses++;
//     }
//     *(short *)(response + 1) = htons(MAX_GUESSES - guesses); // 2 byte remaining guesses
//     strncpy(response + 3, response_word, 5); // Copy 5 byte result word  


//     send(client_socket, response, 8, 0); // Send 8 bytes

//     if (is_valid_guess && correct_guess_count == MAX_WORD_LENGTH) {
//             printf("THREAD %ld: game over; word was %s!\n", pthread_self(), target_word);
//             game_over = true; // Set the game over flag
//         } else if (MAX_GUESSES - guesses == 0) {
//             printf("THREAD %ld: game over; out of guesses\n", pthread_self());
//             game_over = true; // Set the game over flag
//         }

//         // Close the client socket after handling the request
        
//     // Free memory  
//     free(guess);
//     free(response);
//     free(response_word);
//     if (game_over) {
//         printf("hug\n");
//         close(client_socket);
//             printf("THREAD %ld: Client connection closed\n", pthread_self());
//             break;
//         }
//     }
//     free(arg); // Free the memory allocated for the client socket
//     return NULL;
// }

// Signal handler function for SIGUSR1
void sigusr1_handler(int signo) {
    if (signo == SIGUSR1) {
        signal_received = 1; // Set the signal status
        printf("MAIN: SIGUSR1 received; Wordle server shutting down...\n");
    }
}


// bool valid_guess(const char *guess, const char **dictionary, int num_words) {
//     for (int i = 0; i < num_words; i++) {
//         if (strcmp(guess, *(dictionary + i)) == 0) {
//             return true; // Guess is valid
//         }
//     }
//     return false; // Guess is not valid
// }

// int partial_match(const char *guess, const char *target) {
//     int match_count = 0;
//     int *guess_freq = calloc(26, sizeof(int));
//     int *target_freq = calloc(26, sizeof(int));

//     // Count the frequency of characters in the guess
//     for (int i = 0; *(guess + i); i++) {
//         if (isalpha(*(guess + i))) {
//             *(guess_freq + tolower(*(guess + i)) - 'a') += 1;
//         }
//     }

//     // Count the frequency of characters in the target word
//     for (int i = 0; *(target + i); i++) {
//         if (isalpha(*(target + i))) {
//             *(target_freq + tolower(*(target + i)) - 'a') += 1;
//         }
//     }

//     // Calculate the minimum frequency of matching characters
//     for (int i = 0; i < 26; i++) {
//         match_count += (*(guess_freq + i) < *(target_freq + i)) ? *(guess_freq + i) : *(target_freq + i);
//     }
//     free(guess_freq);
//     free(target_freq);

//     return match_count;
// }


int wordle_server(int argc, char **argv) {
    // Parse command-line arguments and validate inputs.
    signal(SIGUSR1, sigusr1_handler);

    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: %s <listener-port> <seed> <dictionary-filename> <num-words>\n", *(argv + 0));
        return EXIT_FAILURE;
    }
    // Extract the listener port number, seed, dictionary filename, and number of words from argv.
    int listener_port = atoi(*(argv + 1));
    int seed = atoi(*(argv + 2));
    char *dictionary_filename = *(argv + 3);
    int num_words = atoi(*(argv + 4));

    srand(seed);

    char **dictionary = (char **)calloc((num_words + 1), sizeof(char *)); // +1 for NULL pointer
    if (!dictionary) {
        fprintf(stderr, "ERROR: Memory allocation failed for dictionary\n");
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(dictionary_filename, "r");
    if (!file) {
        fprintf(stderr, "ERROR: Unable to open dictionary file: %s\n", dictionary_filename);
        exit(EXIT_FAILURE);
    }

    //char *line = line + MAX_WORD_LENGTH + 1; // +1 for null terminator
    int i = 0;
    char *line = calloc(MAX_WORD_LENGTH + 2, sizeof(char)); // Allocate enough space for the word, newline, and null terminator


    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the end of the word.
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        // Allocate memory for the word and copy it into the dictionary.
        //*(dictionary + i) = calloc(strlen(line) + 1, sizeof(char));
        *(dictionary + i) = strdup(line);
        if (!*(dictionary + i)) {
            fprintf(stderr, "ERROR: Memory allocation failed for dictionary word\n");
            exit(EXIT_FAILURE);
        }

        i++;
        // Free each word  

    }
    free(line); //causes issue


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

    // Set listener socket to non-blocking
    //int flags = fcntl(listener, F_GETFL, 0);
    //fcntl(listener, F_SETFL, flags | O_NONBLOCK);
     //-----------------------------Receive Client Connection----------------------------------
    while(!signal_received){
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
        threadArgs args;
        args.client_sd = newsd;
        args.dictionary = dictionary;
        args.numWords = num_words;
        if (pthread_create(&thread, NULL, handle_client, &args) != 0) {
            perror("Error creating thread");
            close(newsd);
            continue;
        }

        // Detach the child thread to make it independent of the main thread
        pthread_detach(thread);
    }
    
    close(listener);
    

 // The server should never reach this point.
    for(int i = 0; i < num_words; i++) {
    free(*(dictionary + i));
    }
    // Free dictionary and line buffer
    free(dictionary);
    return EXIT_SUCCESS;
}