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
    std::cout << endl << "Closing Server..." << endl;
    close(server_socket);
    exit(EXIT_SUCCESS);
}

bool ack_handler(int client_socket, bool rtrn) {
    if (rtrn) {
        if (sendall(client_socket, ACK, strlen(ACK)) == -1) {
            cerr << "Error Sending ACK-Response" << endl;
            return false;
        } else {
            std::cout << "ACK-Response Sent" << endl;
            return true;
        }
    } else {
        if (sendall(client_socket, ERR, strlen(ERR)) == -1) {
            cerr << "Error Sending ERR-Response" << endl;
            return false;
        } else {
            std::cout << "ERR-Response Sent" << endl;
            return false;
        }
    }
}

bool function_login(int client_socket) {
    string username, password;

    // 1. Username vom Client empfangen
    char buffer[256] = {0};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    if (bytes_received <= 0) {
        cerr << "function_login: failed to receive username\n";
        return false;
    }
    buffer[bytes_received] = '\0';
    username = buffer;

    // 2. Passwort vom Client empfangen
    memset(buffer, 0, sizeof(buffer));
    bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
    if (bytes_received <= 0) {
        cerr << "function_login: failed to receive password\n";
        return false;
    }
    buffer[bytes_received] = '\0';
    password = buffer;

    // 3. Login prüfen
    if (validate_login(username, password)) {
        string ok = "OK|Login successful\n";
        send(client_socket, ok.c_str(), ok.size(), 0);
        std::cout << "User '" << username << "' logged in successfully.\n";
        return true;
    } else {
        string err = "ERR|Invalid username or password\n";
        send(client_socket, err.c_str(), err.size(), 0);
        std::cout << "Failed login attempt for user '" << username << "'.\n";
        return false;
    }
}

bool function_send(char* buffer, string username) {
    // Extrahiere den Nachrichtentext nach "SEND|"
    const char* msg_start = buffer + 5; // 5 ist die Länge von "SEND|"
    string message(msg_start);

    std::cout << "SEND Function Called With Message: " << message << endl;

    bool rtrn = save_mail(username, message);
    return rtrn;
}

//simplified for basic hand-in, return all messages
bool function_list(int client_socket) {
    std::cout << "LIST Function Called" << std::endl;

    // Liste alle Mails (kein User-Verzeichnis nötig)
    string list_result = list_mails(); // leerer String, da wir keinen user_dir mehr verwenden

    // Sende Ergebnis an Client
    int bytes_sent = send(client_socket, list_result.c_str(), list_result.size(), 0);
    if (bytes_sent < 0) {
        cerr << "function_list: failed to send mail list to client" << std::endl;
        return false;
    }

    std::cout << "function_list: sent " << bytes_sent << " bytes to client" << std::endl;
    return true;
}

/* saved for later PRO-Implementation
bool function_list(int client_socket) {
    std::cout << "LIST Function Called" << endl;

    // Da wir noch kein Benutzerkonzept haben, verwenden wir einfach einen festen Platzhalter.
    string username = "testuser";

    // Rufe die Funktion auf, die alle Mails für diesen Benutzer auflistet.
    string list_result = read_mail(username, ""); // Leerer Suchstring => alle Mails

    // Sende das Ergebnis an den Client zurück.
    int bytes_sent = send(client_socket, list_result.c_str(), list_result.size(), 0);
    if (bytes_sent < 0) {
        cerr << "function_list: failed to send mail list to client" << endl;
        return false;
    }

    std::cout << "function_list: sent " << bytes_sent << " bytes to client" << endl;
    return true;
}
*/

bool function_read(int client_socket, char* buffer) {
    // Extrahiere den Nachrichtentext nach "READ|"
    const char* msg_start = buffer + 5; // 5 ist die Länge von "READ|"
    string message(msg_start);

    std::cout << "READ Function Called With Message: " << message << endl;

    // Parse message format: username|subject
    size_t pipe_pos = message.find('|');
    
    if (pipe_pos == string::npos) {
        cerr << "read_mail: Invalid Message Format (Expected: username|subject)\n";
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
    
    std::cout << "READ-Response Sent (" << result.length() << " bytes)" << endl;
    return true;
}

bool handle_commands(int client_socket, char* buffer, const std::string& username) {
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

    // LIST
    if (strncmp(buffer, "LIST", 4) == 0) {
        bool rtrn = function_list(client_socket);
        return rtrn;
    }

    // QUIT is handled in server.cpp->handle_client

    // Unbekanntes Kommando
    std::cout << "Unknown command received: " << buffer << endl;
    return false;
}


// Funktion zur Behandlung einer Client-Verbindung (für Thread)
void handle_client(int client_socket, sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    std::cout << "Connection Established With " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

    // --- Initiale Verbindungsbestätigung ---
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';

    if (std::string(buffer) == "connected") {
        if (sendall(client_socket, ACK, strlen(ACK)) == -1) {
            std::cerr << "Failed to send initial ACK" << std::endl;
            close(client_socket);
            return;
        }
        std::cout << "Client connection acknowledged." << std::endl;
    } else {
        std::cerr << "Unexpected initial message: " << buffer << std::endl;
        sendall(client_socket, ERR, strlen(ERR));
        close(client_socket);
        return;
    }

    // --- Login ---
    bool logged_in = false;
    std::string username;

    while (!logged_in) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // Username
        if (bytes_received <= 0) break;
        buffer[bytes_received] = '\0';
        std::string user(buffer);

        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // Password
        if (bytes_received <= 0) break;
        buffer[bytes_received] = '\0';
        std::string pass(buffer);

        if (validate_login(user, pass)) {
            logged_in = true;
            username = user;
            send(client_socket, "OK|Login successful\n", 21, 0);
            std::cout << "User Logged In: " << username << std::endl;
        } else {
            send(client_socket, "ERR|Invalid username or password\n", 34, 0);
            std::cout << "Login Failed For: " << user << std::endl;
        }
    }

    if (!logged_in) {
        close(client_socket);
        return;
    }

    // --- Mail-Kommandos ---
    bool is_running = true;
    while (is_running) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received == 0) {
            // Client hat Socket sauber geschlossen
            std::cout << "Client (" << client_ip << ":" << ntohs(client_addr.sin_port) << ") has closed connection" << std::endl;
            break;
        } else if (bytes_received < 0) {
            std::cerr << "Failed receiving data - closing connection" << std::endl;
            break;
        }
        buffer[bytes_received] = '\0';
        std::string cmd(buffer);

        //Quit is handled here instead of handle_commands -> loop break necessary
        if (str_tolower(cmd) == "quit" || str_tolower(cmd) == "exit") {
        std::cout << "Client (" << client_ip << ":" << ntohs(client_addr.sin_port) << ") requested to quit" << std::endl;
        break;
        }

        bool rtrn = handle_commands(client_socket, buffer, username);
        ack_handler(client_socket, rtrn);
    }

    close(client_socket);
    std::cout << "Connection with client (" << client_ip << ":" << ntohs(client_addr.sin_port) << ") closed" << std::endl;
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
    std::cout << "Server Started On " << SERVER_IP << ":" << port << endl;
    std::cout << "Mail-Spool-Directory: " << get_base_dir() << endl;
    std::cout << "Waiting For Connection..." << endl;

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