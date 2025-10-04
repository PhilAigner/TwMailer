#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <thread>

using namespace std;

// Konfigurationsvariablen
#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define BACKLOG 10
#define ACK "ACK"

int server_socket; // Globale Variable für sauberes Beenden bei Signalen




// Funktion zum sicheren Senden aller Daten
int sendall(int socket, const char *buffer, size_t length) {
    size_t total_sent = 0;
    size_t bytes_left = length;
    int bytes_sent;

    while (total_sent < length) {
        bytes_sent = send(socket, buffer + total_sent, bytes_left, 0);
        if (bytes_sent == -1) {
            // Fehler beim Senden
            return -1;
        }
        total_sent += bytes_sent;
        bytes_left -= bytes_sent;
    }
    return total_sent;
}

// Signal-Handler für sauberes Beenden
void signal_handler(int signal_number) {
    cout << "\nBeende Server..." << endl;
    close(server_socket);
    exit(EXIT_SUCCESS);
}


void handle_mail() {
    cout << "Hier könnte die Mail-Funktionalität implementiert werden." << endl;
}


// Funktion zur Behandlung einer Client-Verbindung (für Thread)
void handle_client(int client_socket, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    cout << "Verbindung hergestellt mit " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;

    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received > 0) {
        cout << "Nachricht empfangen: " << buffer << endl;
        if (sendall(client_socket, ACK, strlen(ACK)) == -1) {
            cerr << "Fehler beim Senden der ACK-Antwort" << endl;
        } else {
            cout << "ACK-Antwort gesendet" << endl;

            handle_mail();
        }
    } else if (bytes_received == 0) {
        cout << "Client hat die Verbindung geschlossen" << endl;
    } else {
        cerr << "Fehler beim Empfangen der Daten" << endl;
    }
    close(client_socket);
}



int main() {
    struct sockaddr_in server_addr, client_addr;
    int client_socket;
    socklen_t client_addr_size;

    // Signal-Handler einrichten
    signal(SIGINT, signal_handler);

    // Socket erstellen
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        cerr << "Fehler beim Erstellen des Sockets" << endl;
        return EXIT_FAILURE;
    }

    // Socket-Optionen setzen (Socket-Wiederverwendung)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Fehler beim Setzen der Socket-Optionen" << endl;
        return EXIT_FAILURE;
    }

    // Server-Adresse konfigurieren
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // IP-Adresse konvertieren und setzen
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        cerr << "Ungültige Adresse / Adresse nicht unterstützt" << endl;
        return EXIT_FAILURE;
    }

    // Socket an Adresse binden
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Bind fehlgeschlagen" << endl;
        return EXIT_FAILURE;
    }

    // Auf Verbindungen warten
    if (listen(server_socket, BACKLOG) < 0) {
        cerr << "Listen fehlgeschlagen" << endl;
        return EXIT_FAILURE;
    }

    cout << "Server gestartet auf " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "Warte auf Verbindungen..." << endl;

    while (true) {
        client_addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket < 0) {
            cerr << "Fehler beim Akzeptieren der Verbindung" << endl;
            continue;
        }
        // Starte neuen Thread für den Client (Lambda für sichere Übergabe)
        thread t([client_socket, client_addr]() {
            handle_client(client_socket, client_addr);
        });
        t.detach(); // Thread im Hintergrund laufen lassen
    }

    // Server-Socket schließen
    close(server_socket);

    return 0;
}