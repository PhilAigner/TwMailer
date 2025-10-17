#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <thread>

#include "serverfunctions.cpp"

// Konfigurationsvariablen
#define SERVER_PORT 8080
#define MAIL_SPOOL_DIR "./mailspool"
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define BACKLOG 10
#define ACK "ACK"
#define ERR "ERR"
#define connectedmsg "connected"

int server_socket; // Globale Variable für sauberes Beenden bei Signalen

using namespace std;

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

bool ack_handler(int client_socket, bool rtrn) {
    if (rtrn) {
        if (sendall(client_socket, ACK, strlen(ACK)) == -1) {
            cerr << "Fehler beim Senden der ACK-Antwort" << endl;
            return false;
        } else {
            cout << "ACK-Antwort gesendet" << endl;
            return true;
        }
    } else {
        if (sendall(client_socket, ERR, strlen(ERR)) == -1) {
            cerr << "Fehler beim Senden der ERR-Antwort" << endl;
            return false;
        } else {
            cout << "ERR-Antwort gesendet" << endl;
            return false;
        }
    }
}


bool function_send(char* buffer) {
    // Extrahiere den Nachrichtentext nach "SEND|"
    const char* msg_start = buffer + 5; // 5 ist die Länge von "SEND|"
    string message(msg_start);

    cout << "SEND function called with message: " << message << endl;
    
    // Beispiel: Speichere die Nachricht für einen festen Benutzer "testuser"
    string username = "testuser"; // In einer echten Anwendung sollte dies dynamisch sein

    bool rtrn = save_mail(username, message);
    return rtrn;
}

bool function_read(char* buffer) {
    // Extrahiere den Nachrichtentext nach "READ"
    const char* msg_start = buffer + 4; // 4 ist die Länge von "READ"
    string message(msg_start);

    cout << "READ function called with message: " << message << endl;

    bool rtrn = true;
    return rtrn;
}

bool handle_mail(int client_socket, char* buffer) {
    bool is_logged_in = false;

    is_logged_in = true; // TEMPORÄR FÜR TESTS

    if (!is_logged_in) {
        // Einfaches Login-Handling
        if (strncmp(buffer, "LOGIN", 5) == 0) {
            is_logged_in = true;
            cout << "User logged in." << endl;
            return true;
        } else {
            sendall(client_socket, ERR, strlen(ERR));
            return false;
        }
    }

    //USER MUSS FÜR JEDEN BEFEHL EINGELOGGT SEIN!
    if (is_logged_in) {
        // SEND
        if (strncmp(buffer, "SEND", 4) == 0) {
            bool rtrn = function_send(buffer);

            return rtrn;
        }

        // READ
        if (strncmp(buffer, "READ", 4) == 0) {
            bool rtrn = function_read(buffer);

            return rtrn;
        }


        return false;
    }
    return false;
}


// Funktion zur Behandlung einer Client-Verbindung (für Thread)
void handle_client(int client_socket, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    cout << "Verbindung hergestellt mit " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;

    memset(buffer, 0, BUFFER_SIZE);

    
    // Initial message handling
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        if (strncmp(buffer, connectedmsg, strlen(connectedmsg)) == 0) {
            cout << "Client succesfully connected" << endl;
            // Acknowledge the connection
            if (sendall(client_socket, ACK, strlen(ACK)) == -1) {
                cerr << "Fehler beim Senden der connected-Antwort" << endl;
                return;
            } else {
                cout << "connected-Antwort gesendet .. continuing" << endl;
            }
        } else {
            cerr << "Fehler beim connecten mit client" << endl;

            sendall(client_socket, ERR, strlen(ERR));
            return;
        }
    }

    //start main client loop
    
    bool is_running = true;
  
    while (is_running) {

        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            cout << "Nachricht empfangen: " << buffer << endl;

            bool rtrn = handle_mail(client_socket, buffer);

            ack_handler(client_socket, rtrn);
        } else if (bytes_received == 0) {
            cout << "Client hat die Verbindung geschlossen" << endl;
            break;
        } else {
            cerr << "Fehler beim Empfangen der Daten - schließe Verbindung" << endl;
            break;
        }
    }
    close(client_socket);
}



int main(int argc, char* argv[]) {
    // Standardwerte
    int port = SERVER_PORT;
    string mail_spool_dir = MAIL_SPOOL_DIR;

    // Argumente auswerten
    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (argc >= 3) {
        mail_spool_dir = argv[2];
    }

    // Configure base dir for serverfunctions
    set_base_dir(mail_spool_dir);

    //TEST
    // save_mail("testuser", "This is a test message.");


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
    server_addr.sin_port = htons(port);

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

    cout << "Server gestartet auf " << SERVER_IP << ":" << port << endl;
    cout << "Mail-Spool-Verzeichnis: " << get_base_dir() << endl;
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