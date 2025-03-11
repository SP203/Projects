#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>

#define PORT 12345
#define MAX_NODES 2

// Struktura wiadomosci ALP
typedef struct {
    uint8_t message_type;    // Typ wiadomosci
    uint8_t node_id;         // Identyfikator noda
    uint16_t sequence_num;   // Numer sekwencji
    uint16_t payload_length; // Dlugosc payload
    char payload[256];       // Dane wiadomosci
} ALPMessage;

// Struktura do przechowywania informacji o nodach
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    int node_id;
    int ready;               // 1 - jesli odebrano "Node Ready"
    int start_ack_received;  // 1 - jesli odebrano "ACK START GAME"
    int game_over_ack;       // 1 - jesli odebrano "ACK GAME OVER"
} NodeInfo;

NodeInfo nodes[MAX_NODES];
int node_count = 0;
bool game_active = false;
int bit_sequence_counter = 0;

//Struktóra przechowujaca wygrywajace patterny
typedef struct {
    int node_id;             // ID noda, który wykryl pattern
    char pattern[256];       // Wykryty pattern
    int rolls_count;         // Liczba rzutów
} GameResult;

GameResult results[MAX_NODES]; // Tablica wyników
int results_count = 0;         // Liczba zapisanych wyników
int games = 5;                 // Liczba gier



// Funkcja tworzaca wiadomosc ALP
void createALPMessage(ALPMessage *message, uint8_t message_type, uint8_t node_id, uint16_t sequence_num, const char *payload) {
    message->message_type = message_type;
    message->node_id = node_id;
    message->sequence_num = sequence_num;
    message->payload_length = strlen(payload);
    strncpy(message->payload, payload, sizeof(message->payload) - 1);
    message->payload[sizeof(message->payload) - 1] = '\0';
}


// Funkcja wysylajaca wiadomosc START GAME do wszystkich nodów
void broadcast_start_message(int server_socket) {
    for (int i = 0; i < node_count; i++) {
        struct sockaddr_in client_addr;
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(nodes[i].port);
        inet_pton(AF_INET, nodes[i].ip, &client_addr.sin_addr);

        ALPMessage message;
        createALPMessage(&message, 4, nodes[i].node_id, 0, "START GAME");

        sendto(server_socket, &message, sizeof(message), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        printf("START GAME sent to Node %d (%s:%d)\n", nodes[i].node_id, nodes[i].ip, nodes[i].port);
    }
}

// Funkcja sprawdzajaca, czy wszystkie nody potwierdzily GAME OVER
bool all_nodes_ack_game_over() {
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].game_over_ack) {
            return false;
        }
    }
    return true;
}

// Funkcja sprawdzajaca, czy wszystkie nody potwierdzily START GAME
bool all_nodes_ack_start() {
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].start_ack_received) {
            return false;
        }
    }
    return true;
}

// Funkcja wyswietlajaca wyniki
void print_game_results() {
    int temp = results_count;
    temp = temp -1;
    printf("\nGame Results:\n");
        printf("Pattern: %s, Rolls: %d\n",
              results[temp].pattern, results[temp].rolls_count);
}

// Funkcja resetujaca gre
void reset_game_state() {
    game_active = false;
    bit_sequence_counter = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].start_ack_received = 0;
        nodes[i].game_over_ack = 0;
        //nodes[i].ready = 0; 
    }
    printf("Game state has been reset.\n");
}

// Funkcja oczekujaca na ACK GAME OVER
void wait_for_game_over_acks(int server_socket) {
    printf("Waiting for ACK GAME OVER from all nodes...\n");

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ALPMessage receivedMessage;

    while (!all_nodes_ack_game_over()) {
        int received_bytes = recvfrom(server_socket, &receivedMessage, sizeof(receivedMessage), 0, (struct sockaddr *)&client_addr, &addr_len);

        if (received_bytes > 0) {
            if (receivedMessage.message_type == 3 && strcmp(receivedMessage.payload, "ACK OVER") == 0) {
                for (int i = 0; i < node_count; i++) {
                    if (strcmp(nodes[i].ip, inet_ntoa(client_addr.sin_addr)) == 0 && ntohs(client_addr.sin_port) == nodes[i].port) {
                        nodes[i].game_over_ack = 1;
                        printf("ACK GAME OVER received from Node %d (%s:%d)\n", nodes[i].node_id, nodes[i].ip, nodes[i].port);
                        break;
                    }
                }
            }
        }
    }

    printf("All nodes acknowledged GAME OVER.\n");
}




// Funkcja wysylajaca losowy bit (O lub R) do nodów
void send_random_bit_to_nodes(int server_socket) {
    while (game_active) {  // Petla gry dopóki flaga game_active jest true
        char bit[2];
        sprintf(bit, "%c", (rand() % 2 == 0) ? 'O' : 'R'); // Generowanie bitu 'O' lub 'R'

        // Wysylanie bitu do kazdego noda
        for (int i = 0; i < node_count; i++) {
            struct sockaddr_in client_addr;
            client_addr.sin_family = AF_INET;
            client_addr.sin_port = htons(nodes[i].port);
            inet_pton(AF_INET, nodes[i].ip, &client_addr.sin_addr);

            ALPMessage message;
            createALPMessage(&message, 1, nodes[i].node_id, bit_sequence_counter, bit);

            int sent_bytes = sendto(server_socket, &message, sizeof(message), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            if (sent_bytes < 0) {
                perror("Error sending bit");
            } else {
                printf("Sent bit: %s to Node %d (%s:%d)\n", bit, nodes[i].node_id, nodes[i].ip, nodes[i].port);
            }
        }

        bit_sequence_counter++;

        // Nasluchiwanie odpowiedzi od nodów
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        ALPMessage receivedMessage;

        // Ustawienie limitu czasu nasluchiwania na odpowiedzi (w przypadku braku wiadomosci)
        struct timeval timeout = {1, 0}; // 1 sekunda
        setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        while (1) {
            int received_bytes = recvfrom(server_socket, &receivedMessage, sizeof(receivedMessage), 0, (struct sockaddr *)&client_addr, &addr_len);

            if (received_bytes > 0) {
                // Obsluga wiadomosci ACK BIT
                if (receivedMessage.message_type == 3 && strstr(receivedMessage.payload, "ACK BIT") == receivedMessage.payload) {
                    printf("Bit acknowledgment received from Node %d: %s\n",
                           receivedMessage.node_id, receivedMessage.payload);
                }
                // Obsluga wiadomosci PATTERN DETECTED
                else if (receivedMessage.message_type == 5) {
                    printf("PATTERN DETECTED received from Node %d. Pattern: %s, Sequence: %d\n",
                    receivedMessage.node_id, receivedMessage.payload, ntohs(receivedMessage.sequence_num));
                    
                    
                    // Debugowanie przed dodaniem wyniku
                    printf("Before adding result - results_count: %d\n", results_count);

                    // Zapisanie wyniku do struktury
                    
                        results[results_count].node_id = receivedMessage.node_id;
                        strncpy(results[results_count].pattern, receivedMessage.payload, sizeof(results[results_count].pattern) - 1);
                        results[results_count].pattern[sizeof(results[results_count].pattern) - 1] = '\0';
                        results[results_count].rolls_count = bit_sequence_counter; // Liczba rzutów do wykrycia patternu
                        results_count++;
                        
                        printf("After adding result - results_count: %d\n", results_count);

                    
                    // Zatrzymanie gry
                    game_active = false;
                    
                    // Wyslanie GAME OVER do nodów
                    for (int i = 0; i < node_count; i++) {
                        struct sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(nodes[i].port);
                        inet_pton(AF_INET, nodes[i].ip, &addr.sin_addr);
                    
                        ALPMessage end_message;
                        createALPMessage(&end_message, 5, nodes[i].node_id, 0, "GAME OVER");
                        sendto(server_socket, &end_message, sizeof(end_message), 0, (struct sockaddr *)&addr, sizeof(addr));
                    }
                    
                    wait_for_game_over_acks(server_socket);
                    
                    printf("Game over. Stopping bit transmission.\n");
                    print_game_results();
                    reset_game_state();


                    return; // Wyjscie z funkcji po zakonczeniu gry
                }
            } else {
                // Jesli timeout wystapil, wyjdz z petli nasluchiwania
                break;
            }
        }

        // Krótkie opóznienie przed wyslaniem kolejnego bitu
        //if (game_active) {
            //usleep(5000); // 500ms delay
        //}
    }

    printf("Stopped sending bits as game is no longer active.\n");
}

// Funkcja wyswietlajaca wszystkie wyniki po zakonczeniu wszystkich gier
void print_all_game_results() {
    printf("\n=== FINAL GAME RESULTS ===\n");
    for (int i = 0; i < 5; i++) {
        printf(" Pattern: %s | Rolls: %d\n",
            results[i].pattern, results[i].rolls_count);
    }
}

typedef struct {
    char pattern[256];      // Pattern (np. "RORR")
    int total_rolls;        // Suma liczby rzutów dla danego patternu
    int occurrences;        // Liczba wystąpień danego patternu
    float probability;      // Prawdopodobieństwo wygranej patternu
    float average_rolls;    // Średnia liczba rzutów
} PatternStats;

void calculate_pattern_stats(GameResult *results, int results_count, int total_games) {
    PatternStats stats[node_count];
    int pattern_count = 0;

    // Inicjalizujemy statystyki
    for (int i = 0; i < node_count; i++) {
        stats[i].total_rolls = 0;
        stats[i].occurrences = 0;
        stats[i].probability = 0.0f;
        stats[i].average_rolls = 0.0f;
        memset(stats[i].pattern, 0, sizeof(stats[i].pattern));
    }

    // Zliczamy wyniki dla każdego patternu
    for (int i = 0; i < results_count; i++) {
        const char *current_pattern = results[i].pattern;
        int found = 0;

        // Sprawdzamy, czy pattern już istnieje w statystykach
        for (int j = 0; j < pattern_count; j++) {
            if (strcmp(stats[j].pattern, current_pattern) == 0) {
                stats[j].total_rolls += results[i].rolls_count;
                stats[j].occurrences++;
                found = 1;
                break;
            }
        }

        // Jeśli pattern nie istnieje, dodajemy go do statystyk
        if (!found && pattern_count < node_count) {
            strncpy(stats[pattern_count].pattern, current_pattern, sizeof(stats[pattern_count].pattern) - 1);
            stats[pattern_count].total_rolls = results[i].rolls_count;
            stats[pattern_count].occurrences = 1;
            pattern_count++;
        }
    }

    // Obliczamy prawdopodobieństwo i średnią liczbę rzutów dla każdego patternu
    for (int i = 0; i < pattern_count; i++) {
        if (stats[i].occurrences > 0) {
            stats[i].probability = (float)stats[i].occurrences / total_games;
            stats[i].average_rolls = (float)stats[i].total_rolls / stats[i].occurrences;
        }
    }

    // Wyświetlamy wyniki
    printf("\nPattern Statistics:\n");
    for (int i = 0; i < pattern_count; i++) {
        printf("Pattern: %s\n", stats[i].pattern);
        printf("  Total Wins: %d\n", stats[i].occurrences);
        printf("  Probability of Winning: %.2f%%\n", stats[i].probability * 100);
        printf("  Average Rolls to Win: %.2f\n", stats[i].average_rolls);
    }
}

int main() {
    srand(time(NULL)); // Inicjalizacja generatora liczb losowych
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Rejestracja nodów
    while (node_count < MAX_NODES) {
        ALPMessage message;
        recvfrom(server_socket, &message, sizeof(message), 0, (struct sockaddr *)&client_addr, &addr_len);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        if (message.message_type == 3 && strcmp(message.payload, "Node Ready") == 0) {
            if (node_count < MAX_NODES) {
                strcpy(nodes[node_count].ip, client_ip);
                nodes[node_count].port = client_port;
                nodes[node_count].node_id = node_count + 1;
                nodes[node_count].ready = 1;
                nodes[node_count].start_ack_received = 0;
                node_count++;
                printf("Node Ready received from %s:%d (Node ID: %d)\n", client_ip, client_port, nodes[node_count - 1].node_id);

                // Odeslanie ACK
                ALPMessage ack_message;
                createALPMessage(&ack_message, 3, nodes[node_count - 1].node_id, 0, "ACK");
                sendto(server_socket, &ack_message, sizeof(ack_message), 0, (struct sockaddr *)&client_addr, addr_len);
            }
        }
    }

    printf("All nodes registered. Starting the game loop...\n");

    // Petla gier
    for (int round = 1; round <= games; round++) {
        printf("\n--- GAME ROUND %d ---\n", round);

        // Wysylanie wiadomosci START GAME
        broadcast_start_message(server_socket);

        // Petla oczekiwania na potwierdzenie START GAME
        while (1) {
            ALPMessage message;
            recvfrom(server_socket, &message, sizeof(message), 0, (struct sockaddr *)&client_addr, &addr_len);
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);

            if (message.message_type == 3 && strcmp(message.payload, "ACK START GAME") == 0) {
                for (int i = 0; i < node_count; i++) {
                    if (strcmp(nodes[i].ip, client_ip) == 0 && nodes[i].port == client_port) {
                        nodes[i].start_ack_received = 1;
                        printf("ACK START GAME received from Node %d (%s:%d)\n", nodes[i].node_id, nodes[i].ip, nodes[i].port);
                    }
                }

                if (all_nodes_ack_start() && !game_active) {
                    printf("All nodes acknowledged START GAME. Starting to send bits...\n");
                    game_active = true;
                    send_random_bit_to_nodes(server_socket);
                    break; // Przerwij petle po rozpoczeciu gry
                }
            }
        }

        // Reset stanu po kazdej grze
        reset_game_state();
    }

    printf("All game rounds completed.\n");
    print_all_game_results();
    calculate_pattern_stats(results, results_count, games); // 5 to liczba rozegranych gier
    close(server_socket);
    return 0;
}
