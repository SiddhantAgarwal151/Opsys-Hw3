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

void send_ready_message(int client_fd);
void close_client_connection(int client_fd);
void *handle_client(void *arg);
void send_reply(int client_fd, bool valid_guess, short guesses_remaining, const char *result);
void process_guess(int client_fd, const char *guess, const char *hidden_word);
bool is_valid_word(const char *guess, char **words, int num_words);

void send_ready_message(int client_fd) {
    const char *ready_msg = "ready";
    send_reply(client_fd, true, htons(MAX_GUESSES), ready_msg);
}

// Helper function to close the client connection and exit the thread.
void close_client_connection(int client_fd) {
    close(client_fd);
    pthread_exit(NULL);
}

// Implement the handle_client() function that each child thread will execute to handle a client.
void *handle_client(void *arg) {
    // Get the client_fd from the argument.
    int client_fd = *((int *)arg);

    // Send an initial "ready" message to the client.
    send_ready_message(client_fd);

    // Start the game loop to receive and process guesses until the game ends or the client disconnects.
    char *hidden_word = calloc(MAX_WORD_LENGTH + 1, sizeof(char)); // +1 for null terminator
    // Generate or choose a hidden word here and store it in the hidden_word array.

    char *guess_end = calloc(MAX_WORD_LENGTH + 1, sizeof(char)); // +1 for null terminator

    while (true) {
        // Receive the guess from the client.
        int bytes_received = recv(client_fd, guess_end, MAX_WORD_LENGTH, 0);
        if (bytes_received <= 0) {
            // Client disconnected or error occurred.
            fprintf(stderr, "THREAD %lu: Client disconnected; closing TCP connection...\n", pthread_self());
            close_client_connection(client_fd);
        }

        // Null-terminate the received guess string.
        *(guess_end + bytes_received) = '\0';

        // Process the client's guess.
        process_guess(client_fd, guess_end, hidden_word);
    }

    // Close the client connection and exit the thread.
    close_client_connection(client_fd);
    return NULL;
}

// Helper function to send the reply back to the client.
void send_reply(int client_fd, bool valid_guess, short guesses_remaining, const char *result) {
    char* reply_buffer = calloc(9, sizeof(char)); // 1 byte for valid_guess, 2 bytes for guesses_remaining, 5 bytes for result, and 1 byte for null terminator
    char *ptr = reply_buffer + 1; // Start after the first byte for valid_guess
    snprintf(ptr, 3, "%02d", guesses_remaining);
    ptr += 2;
    strncpy(ptr, result, MAX_WORD_LENGTH);
    *(reply_buffer + 0) = valid_guess ? 'Y' : 'N'; // Set the first byte for valid_guess
    *(reply_buffer + 8) = '\0';
    int bytes_sent = send(client_fd, reply_buffer, 8, 0);
    if (bytes_sent != 8) {
        fprintf(stderr, "THREAD %lu: Error sending reply to client\n", pthread_self());
        close_client_connection(client_fd);
    }
    free(reply_buffer);
}

// Implement the process_guess() function to process the client's guess.
void process_guess(int client_fd, const char *guess, const char *hidden_word) {
    short guesses_remaining = htons(MAX_GUESSES);
    char* result = calloc(MAX_WORD_LENGTH + 1, sizeof(char)); // +1 for null terminator

    if (strlen(guess) != MAX_WORD_LENGTH) {
        // Invalid guess, not 5-letter word
        send_reply(client_fd, false, guesses_remaining, "?????");
        return;
    }

    total_guesses++;

    // Check if the guess is a valid word from the dictionary
    if (!is_valid_word(guess, words, MAX_WORD_LENGTH)) {
        // Invalid guess, not present in the dictionary
        send_reply(client_fd, false, guesses_remaining, "?????");
        return;
    }

    // Process the guess and calculate the result
    char *result_ptr = result;
    const char *hidden_ptr = hidden_word;
    for (int i = 0; i < MAX_WORD_LENGTH; i++) {
        if (*guess == *hidden_ptr) {
            *result_ptr = toupper(*guess); // Matching letter in the correct position
        } else if (strchr(hidden_word, *guess)) {
            *result_ptr = tolower(*guess); // Letter in the word but in an incorrect position
        } else {
            *result_ptr = '-'; // Incorrect letter not in the word
        }
        guess++;
        hidden_ptr++;
        result_ptr++;
    }
    *result_ptr = '\0'; // Null terminator

    // Check if the guess matches the hidden word
    if (strcmp(result, hidden_word) == 0) {
        // Client guessed the word correctly
        total_wins++;
        send_reply(client_fd, true, guesses_remaining, result);
        fprintf(stdout, "THREAD %lu: Game over; word was %s!\n", pthread_self(), hidden_word);
        close_client_connection(client_fd);
    } else {
        // Valid guess, but not the correct word
        guesses_remaining--;
        send_reply(client_fd, true, guesses_remaining, result);
    }
}

// Helper function to check if a word is valid from the dictionary.
bool is_valid_word(const char *guess, char **words, int num_words) {
    for (int i = 0; i < num_words; i++) {
        if (strcmp(guess, *(words + i)) == 0) {
            return true;
        }
    }
    return false;
}


int wordle_server(int argc, char **argv) {
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
        *(dictionary + i) = strdup(line);
        if (!*(dictionary + i)) {
            fprintf(stderr, "ERROR: Memory allocation failed for dictionary word\n");
            exit(EXIT_FAILURE);
        }

        i++;
    }

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

        // Create a child thread to handle the client
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, &client_socket) != 0) {
            perror("Error creating thread");
            close(client_socket);
            continue;
        }

        // Detach the child thread to make it independent of the main thread
        pthread_detach(thread);
    }

    // The server should never reach this point.
    return EXIT_FAILURE;
}
