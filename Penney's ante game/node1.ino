#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>

// Parametry sieci
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEA }; // MAC adres node
ZsutIPAddress ip(192, 168, 119, 13);                 // IP node
ZsutIPAddress serverIP(192, 168, 119, 10);           // IP serwera
unsigned int serverPort = 12345;                     // Port serwera
unsigned int localPort = 8888;                       // Port node

// Obiekt UDP
ZsutEthernetUDP Udp;

bool ackReceived = false;  // Flaga do przechowywania informacji, czy ACK zostało odebrane
bool gameStarted = false;  // Flaga do sprawdzania, czy gra się rozpoczęła
const char pattern[] = "RORO"; // Wzorzec binarny

// Struktura FIFO do przechowywania najnowszych danych
#define PATTERN_LENGTH 4
struct FIFO {
  char buffer[PATTERN_LENGTH];
  int count;

  void init() {
    count = 0;
    for (int i = 0; i < PATTERN_LENGTH; i++) {
      buffer[i] = '\0';
    }
  }

  void push(char bit) {
    if (count < PATTERN_LENGTH) {
      buffer[count] = bit;
      count++;
    } else {
      for (int i = 0; i < PATTERN_LENGTH - 1; i++) {
        buffer[i] = buffer[i + 1];
      }
      buffer[PATTERN_LENGTH - 1] = bit;
    }
  }

  bool matchesPattern(const char *pattern) {
    if (count < PATTERN_LENGTH) {
      return false;
    }
    return strncmp(buffer, pattern, PATTERN_LENGTH) == 0;
  }
};

FIFO bitBuffer;

// Struktura wiadomości ALP
struct ALPMessage {
  uint8_t message_type;    // Typ wiadomości (1 - FLIP, 2 - RESULT, 3 - ACK, 4 - START GAME, 5 - GAME OVER)
  uint8_t node_id;         // Identyfikator node
  uint16_t sequence_num;   // Numer sekwencji
  uint16_t payload_length; // Długość pola payload
  char payload[256];       // Dane wiadomości
};

// Funkcja do tworzenia wiadomości ALP
void createALPMessage(ALPMessage &message, uint8_t message_type, uint8_t node_id, uint16_t sequence_num, const char *payload) {
  message.message_type = message_type;
  message.node_id = node_id;
  message.sequence_num = sequence_num;
  message.payload_length = strlen(payload);
  strncpy(message.payload, payload, sizeof(message.payload) - 1);
  message.payload[sizeof(message.payload) - 1] = '\0';
}

// Funkcja do wysyłania wiadomości ALP
void sendALPMessage(ALPMessage &message) {
  Udp.beginPacket(serverIP, serverPort);
  Udp.write((uint8_t *)&message, sizeof(ALPMessage));
  Udp.endPacket();
}

// Funkcja do odbierania wiadomości ALP
bool receiveALPMessage(ALPMessage &message) {
  int packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    Udp.read((uint8_t *)&message, sizeof(ALPMessage));
    return true;
  }
  return false;
}

// Funkcja do wyświetlania szczegółów odebranej wiadomości ALP
void printALPMessageDetails(const ALPMessage &message) {
  Serial.println("Received ALP message:");
  Serial.print("  Message Type: ");
  Serial.println(message.message_type);
  Serial.print("  Node ID: ");
  Serial.println(message.node_id);
  Serial.print("  Sequence Number: ");
  Serial.println(message.sequence_num);
  Serial.print("  Payload: ");
  Serial.println(message.payload);
}

// Funkcja do wysłania ACK
void sendACK(const ALPMessage &receivedMessage) {
  ALPMessage ackMessage;
  createALPMessage(ackMessage, 3, receivedMessage.node_id, receivedMessage.sequence_num, "ACK");
  sendALPMessage(ackMessage);
  Serial.println("Sent ACK to server.");
}

void setup() {
  // Inicjalizacja serial monitor
  Serial.begin(115200);
  while (!Serial);

  // Inicjalizacja Ethernet i UDP
  ZsutEthernet.begin(mac, ip);
  Udp.begin(localPort);

  // Inicjalizacja FIFO
  bitBuffer.init();

  Serial.println("Node initialized. Ready to communicate with server.");
}

void loop() {
  if (!ackReceived) { // Jeśli ACK nie zostało jeszcze odebrane
    ALPMessage message;
    createALPMessage(message, 3, 1, 0, "Node Ready");

    // Wysyłanie wiadomości
    sendALPMessage(message);
    Serial.println("Sent Node Ready message to server. Waiting for ACK...");

    // Czekanie na ACK
    ALPMessage receivedMessage;
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // Timeout 5 sekund
      if (receiveALPMessage(receivedMessage)) {
        printALPMessageDetails(receivedMessage);
        if (receivedMessage.message_type == 3 && strcmp(receivedMessage.payload, "ACK") == 0) {
          Serial.println("ACK received. Waiting for game start...");
          ackReceived = true;
          break;
        }
      }
    }

    if (!ackReceived) {
      Serial.println("No ACK received. Retrying...");
      delay(1000); // Opóźnienie przed retransmisją
    }
  } else if (!gameStarted) {
    ALPMessage receivedMessage;
        if (receiveALPMessage(receivedMessage)) {
            printALPMessageDetails(receivedMessage);
            if (receivedMessage.message_type == 4 && strcmp(receivedMessage.payload, "START GAME") == 0) {
                Serial.println("START GAME received!");

                // Wysłanie potwierdzenia START GAME do serwera
                ALPMessage ackMessage;
                createALPMessage(ackMessage, 3, receivedMessage.node_id, receivedMessage.sequence_num, "ACK START GAME");
                sendALPMessage(ackMessage);
                Serial.println("Sent ACK for START GAME to server.");

                gameStarted = true;
            }
        }
  } else { // Tryb gry
    ALPMessage receivedMessage;
    if (receiveALPMessage(receivedMessage)) {
        if (receivedMessage.message_type == 1) { // Odebrano bit
            char bit = receivedMessage.payload[0];
            bitBuffer.push(bit);

            // Wysłanie potwierdzenia odbioru bitu do serwera
            ALPMessage ackMessage;
            char ackPayload[16];
            sprintf(ackPayload, "ACK BIT %c", bit); // Tworzymy wiadomość z potwierdzeniem bitu
            createALPMessage(ackMessage, 3, receivedMessage.node_id, receivedMessage.sequence_num, ackPayload);
            sendALPMessage(ackMessage);
            Serial.println("Sent ACK for received bit to server.");

            if (bitBuffer.matchesPattern(pattern)) {
                Serial.println("Pattern detected!");
                // Debugowanie FIFO
                Serial.print("FIFO state during pattern detection: ");
                for (int i = 0; i < PATTERN_LENGTH; i++) {
                    Serial.print(bitBuffer.buffer[i]);
                }
                Serial.println();

                // Wysłanie wiadomości PATTERN DETECTED do serwera
                ALPMessage detectedMessage;
                createALPMessage(detectedMessage, 5, receivedMessage.node_id, receivedMessage.sequence_num, pattern);
                sendALPMessage(detectedMessage);
                Serial.println("Sent PATTERN DETECTED to server.");
                //gameStarted = false; // Zatrzymanie gry
            }
        }
         // Obsługa wiadomości GAME OVER
        else if (receivedMessage.message_type == 5 && strcmp(receivedMessage.payload, "GAME OVER") == 0) {
            Serial.println("Game over received. Resetting game state.");
            printALPMessageDetails(receivedMessage);

            //odesłanie ack game over do serwera
            ALPMessage ackMessage;
            char ackPayload[16];
            sprintf(ackPayload, "ACK OVER"); // Tworzymy wiadomość z potwierdzeniem bitu
            createALPMessage(ackMessage, 3, receivedMessage.node_id, receivedMessage.sequence_num, ackPayload);
            sendALPMessage(ackMessage);
            Serial.println("Sent ACK for GAME OVER to server.");
            gameStarted = false; // Zatrzymanie gry
            bitBuffer.init();    // Reset FIFO
        }
    }
}


  delay(100);
}
