#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h> 

// Constants
#define MAX_WORD_LENGTH 6
#define MAX_GUESSES 6

// Global variables (already declared in hw3-main.c)
extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char **words;

bool valid_guess(const char *guess, const char **dictionary, int num_words) {
    for (int i = 0; i < num_words; i++) {
        if (strcmp(guess, *(dictionary + i)) == 0) {
            return true; // Guess is valid
        }
    }
    return false; // Guess is not valid
}

int partial_match(const char *guess, const char *target) {
    int match_count = 0;
    int *guess_freq = calloc(26, sizeof(int));
    int *target_freq = calloc(26, sizeof(int));

    // Count the frequency of characters in the guess
    for (int i = 0; *(guess + i); i++) {
        if (isalpha(*(guess + i))) {
            *(guess_freq + tolower(*(guess + i)) - 'a') += 1;
        }
    }

    // Count the frequency of characters in the target word
    for (int i = 0; *(target + i); i++) {
        if (isalpha(*(target + i))) {
            *(target_freq + tolower(*(target + i)) - 'a') += 1;
        }
    }

    // Calculate the minimum frequency of matching characters
    for (int i = 0; i < 26; i++) {
        match_count += (*(guess_freq + i) < *(target_freq + i)) ? *(guess_freq + i) : *(target_freq + i);
    }
    free(guess_freq);
    free(target_freq);

    return match_count;
}


int wordle_server(int argc, char **argv) {
    char *target_word = "mouth";
    // Parse command-line arguments and validate inputs.
    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: %s <listener-port> <seed> <dictionary-filename> <num-words>\n", *(argv + 0));
        return EXIT_FAILURE;
    }
    // Extract the listener port number, seed, dictionary filename, and number of words from argv.
    int listener_port = atoi(*(argv + 1));
    int seed = atoi(*(argv + 2));
    char *dictionary_filename = *(argv + 3);
    int num_words = atoi(*(argv + 4));

    char **dictionary = (char **)malloc((num_words + 1) * sizeof(char *)); // +1 for NULL pointer
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
    char *line = calloc(MAX_WORD_LENGTH+1,sizeof(char));

    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the end of the word.
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        // Allocate memory for the word and copy it into the dictionary.
        *(dictionary + i) = malloc(strlen(line) + 1);
        strcpy((*(dictionary + i)), line);
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

    // Create a TCP socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to a specific address and port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(listener_port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) == -1) {
        perror("Error listening");
        exit(EXIT_FAILURE);
    }

    printf("MAIN: Wordle server listening on port %d\n", listener_port);
    // Main server loop: accept incoming connections and create a child thread to handle each client.

    while (true) {
    // Accept an incoming connection
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        perror("Error accepting client connection");
        continue;
    }

    printf("SERVER: Accepted client connection\n");

    // Receive a guess from the client
    char *guess = calloc (MAX_WORD_LENGTH + 1, sizeof(char));
    ssize_t bytes_received = recv(client_socket, guess, sizeof(guess) - 1, 0);
    total_guesses++;
    if (bytes_received <= 0) {
        // Handle client disconnect or error
        close(client_socket);
        printf("SERVER: Client disconnected\n");
        continue;
    }
    *(guess + bytes_received) = '\0'; // Null-terminate the received data

    // ... Accept an incoming connection and receive a guess from the client ...
    printf("huh\n");
    // Process the guess and send response
    char *response = calloc(8, sizeof(char)); // Adjust the buffer size as needed
    // Implement game logic and generate response here
    // ... Implement game logic and generate response here ...

    bool is_valid_guess = valid_guess(guess, dictionary, num_words);
    int correct_guess_count = strcmp(guess, target_word) == 0 ? MAX_WORD_LENGTH : partial_match(guess, target_word);
   
    // Calculate the remaining guesses based on the total guesses and the guesses made by the client
    
    
    // Generate the response based on the guess and the target word
    char *response_word = calloc(MAX_WORD_LENGTH + 1, sizeof(char));
    for (int i = 0; i < MAX_WORD_LENGTH; i++) {
        if (*(guess + i) == *(target_word + i)) {
            *(response_word + i) = toupper(*(guess + i)); // Uppercase for correct position
        } else if (strchr(target_word, *(guess + i))) {
            *(response_word + i) = tolower(*(guess + i)); // Lowercase for correct letter in wrong position
        } else {
            *(response_word + i) = '-'; // Dash for incorrect letter
        }
    }
    *(response_word + MAX_WORD_LENGTH) = '\0';

    *(response + 0) = is_valid_guess ? 'Y' : 'N'; // Valid guess indicator

    *(short *)(response + 1) = htons(MAX_GUESSES - total_guesses); // 2 byte remaining guesses
    strncpy(response + 3, response_word, 5); // Copy 5 byte result word  


    send(client_socket, response, 8, 0); // Send 8 bytes
    // Free memory  
    free(guess);
    free(response);
    free(response_word);

    // Send the response message to the client
    //printf("%s", response);
    // Close the client socket after handling the request
    close(client_socket);
    printf("SERVER: Client connection closed\n");
    }

 // The server should never reach this point.
    for(int i = 0; i < num_words; i++) {
    free(*(dictionary + i));
    }
    // Free dictionary and line buffer
    free(dictionary);
    free(line);
    return EXIT_FAILURE;
}