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
    cout << endl << "Closing Server..." << endl;
    close(server_socket);
    exit(EXIT_SUCCESS);
}

bool ack_handler(int client_socket, bool rtrn) {
    if (rtrn) {
        if (sendall(client_socket, ACK, strlen(ACK)) == -1) {
            cerr << "Error Sending ACK-Response" << endl;
            return false;
        } else {
            cout << "ACK-Response Sent" << endl;
            return true;
        }
    } else {
        if (sendall(client_socket, ERR, strlen(ERR)) == -1) {
            cerr << "Error Sending ERR-Response" << endl;
            return false;
        } else {
            cout << "ERR-Response Sent" << endl;
            return false;
        }
    }
}


bool function_send(char* buffer, string username) {
    // Extrahiere den Nachrichtentext nach "SEND|"
    const char* msg_start = buffer + 5; // 5 ist die Länge von "SEND|"
    string message(msg_start);

    cout << "SEND function called with message: " << message << endl;

    bool rtrn = save_mail(username, message);
    return rtrn;
}

bool function_read(int client_socket, char* buffer) {
    // Extrahiere den Nachrichtentext nach "READ|"
    const char* msg_start = buffer + 5; // 5 ist die Länge von "READ|"
    string message(msg_start);

    cout << "READ function called with message: " << message << endl;

    // Parse message format: username|subject
    size_t pipe_pos = message.find('|');
    
    if (pipe_pos == string::npos) {
        cerr << "read_mail: invalid message format (expected: username|subject)\n";
        return false;
    }
    
    string search_username = message.substr(0, pipe_pos);
    string subject = message.substr(pipe_pos + 1);
    
    // Call read_mail function
    string result = read_mail(search_username, subject);
    
    // Send result back to client
    if (sendall(client_socket, result.c_str(), result.length()) == -1) {
        cerr << "Error Sending READ-Response" << endl;
        return false;
    }
    
    cout << "READ-Response Sent (" << result.length() << " bytes)" << endl;
    return true;
}

bool handle_mail(int client_socket, char* buffer) {
    bool is_logged_in = false;
    string username = "testuser"; // Placeholder für Benutzernamen

    is_logged_in = true; // TEMPORÄR FÜR TESTS

    if (!is_logged_in) {
        // Einfaches Login-Handling
        if (strncmp(buffer, "LOGIN", 5) == 0) {
            is_logged_in = true;
            username = "testuser"; // In einer echten Anwendung sollte dies dynamisch sein
            cout << "User logged in." << endl;
            return true;
        } else {
            return false;
        }
    }

    //USER MUSS FÜR JEDEN BEFEHL EINGELOGGT SEIN!
    if (is_logged_in) {
        // SEND
        if (strncmp(buffer, "SEND", 4) == 0) {
            bool rtrn = function_send(buffer, username);

            return rtrn;
        }

        // READ
        if (strncmp(buffer, "READ", 4) == 0) {
            bool rtrn = function_read(client_socket, buffer);

            return rtrn;
        }
        //LIST
        if (strncmp(buffer, "LIST", 4) == 0) {
            bool rtrn = function_list(client_socket, username);
        return false;
    }
    return false;
}


// Funktion zur Behandlung einer Client-Verbindung (für Thread)
void handle_client(int client_socket, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    cout << "Connection Established With " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;

    memset(buffer, 0, BUFFER_SIZE);

    
    // Initial message handling
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        if (strncmp(buffer, connectedmsg, strlen(connectedmsg)) == 0) {
            cout << "Client ("<< client_ip << ":" << ntohs(client_addr.sin_port) << ") Connected" << endl;
            // Acknowledge the connection
            if (sendall(client_socket, ACK, strlen(ACK)) == -1) {
                cerr << "Error Sending Connected-Response" << endl;
                return;
            } else {
                cout << "Connected-Response Sent To "<< client_ip << ":" << ntohs(client_addr.sin_port) <<" .. Continuing" << endl;
            }
        } else {
            cerr << "Failed Connection to Client" << endl;

            sendall(client_socket, ERR, strlen(ERR));
            return;
        }
    }

    //start main client loop
    
    bool is_running = true;
  
    while (is_running) {

        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            cout << "Message Received: " << buffer << endl;

            bool rtrn = handle_mail(client_socket, buffer);

            ack_handler(client_socket, rtrn);
        } else if (bytes_received == 0) {
            cout << "Client ("<< client_ip << ":" << ntohs(client_addr.sin_port) <<") Has Closed Connection" << endl;
            break;
        } else {
            cerr << "Failed To Receiving Data - Closing Connection" << endl;
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

    struct sockaddr_in server_addr, client_addr;
    int client_socket;
    socklen_t client_addr_size;

    // Signal-Handler einrichten
    signal(SIGINT, signal_handler);

    // Socket erstellen
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        cerr << "Failed To Create Socket" << endl;
        return EXIT_FAILURE;
    }

    // Socket-Optionen setzen (Socket-Wiederverwendung)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed To Set Socket Options" << endl;
        return EXIT_FAILURE;
    }

    // Server-Adresse konfigurieren
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // IP-Adresse konvertieren und setzen
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        cerr << "Invalid Address" << endl;
        return EXIT_FAILURE;
    }

    // Socket an Adresse binden
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Bind Failed" << endl;
        return EXIT_FAILURE;
    }

    // Auf Verbindungen warten
    if (listen(server_socket, BACKLOG) < 0) {
        cerr << "List Failed" << endl;
        return EXIT_FAILURE;
    }

    //SERVER START
    cout << "Server Started On " << SERVER_IP << ":" << port << endl;
    cout << "Mail-Spool-Directory: " << get_base_dir() << endl;
    cout << "Waiting For Connection..." << endl;

    while (true) {
        client_addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket < 0) {
            cerr << "Failed To Accept Connection" << endl;
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