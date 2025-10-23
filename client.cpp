#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <atomic>
#include <thread>
#include <unistd.h>

#include "clientfunctions.cpp"

using namespace std;

// Standardwerte Server-Verbindung
string server_ip = "127.0.0.1";
int server_port = 8080;

string connected_msg = "connected";
#define ACK "ACK"
#define ERR "ERR"

bool login_success = false;

atomic<bool> running(true); // Flag für laufende Threads

void user_input_thread(int sock) {
    std::string username;
    while (running) {
        cout << ">> ";
        string command;
        if (!getline(cin, command)) {
            running = false;
            break;
        }
        
        //detach cmd from args -> mainly for read
        size_t space_pos = command.find(' ');
        string cmd = str_tolower(space_pos == string::npos ? command : command.substr(0, space_pos));
        string arg = (space_pos == string::npos) ? "" : trim(command.substr(space_pos + 1));
        if (cmd.empty()) continue;
        
        // QUIT/EXIT jederzeit möglich
        if (cmd == "exit" || cmd == "quit") {
            cout << "Closing Connection...\n";
            running = false;
            break;
        }

        // LOGIN nur möglich, wenn noch nicht eingeloggt
        if (cmd == "login" && !login_success) {
            if(handle_login(sock, username)) login_success = true;
            continue;
        }

        // Alle anderen Kommandos werden einfach weitergeleitet
        // Server checks login-status
        if (!login_success) {
            cout << "Please login first using the 'login' command.\n";
            continue;
        }
        bool res = false;
        if (cmd == "send") {
            send_message(sock);
            res = handle_ack(sock);
            if (res) {
                cout << "Message sent successfully.\n";
            } else {
                cout << "Failed to send message.\n";
            }
        }
        else if (cmd == "list") {
            list_messages(sock);
        }
        else if (cmd == "read") {
            if (arg.empty()) {
                cout << "Usage: read <index>"<< endl;
                continue;
            }
            read_message(sock,username, arg); // arg = Index
        }
        else if (cmd == "delete") {
            if (arg.empty()) {
                cout << "Usage: delete <index>"<< endl;
                continue;
            }
            delete_message(sock, username, arg);
            res = handle_ack(sock);
            
            if (res) {
                cout << "Message deleted successfully.\n";
            } else {
                cout << "Server-error: Failed to delete message.\n";
            }
        }
        else {
            cout << "Unknown command: " << command << endl;
            continue;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc >= 2) server_ip = argv[1];
    if (argc >= 3) server_port = atoi(argv[2]);

    // Socket erstellen
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Error creating socket\n";
        return 1;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Invalid Address\n";
        return 1;
    }

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Connection Failed\n";
        return 1;
    }

    // Initiale „connected“-Nachricht an Server
    if (send(sock, connected_msg.c_str(), connected_msg.size(), 0) == -1) {
        cerr << "Error sending connection message\n";
        close(sock);
        return 1;
    }

    // Server-Antwort empfangen
    char buffer[1024] = {0};
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        cerr << "Error receiving connection ACK\n";
        close(sock);
        return 1;
    }
    buffer[bytes_received] = '\0';
    string response(buffer);

    // Prüfen ob Server ACK gesendet hat
    if (response.find(ACK) != 0) {
        cerr << "Unexpected response from server: " << response << endl;
        close(sock);
        return 1;
    }

    cout << "Successfully connected to Server(" << server_ip << ") over Port " << server_port << "!\n";

    // Starte Thread für User-Input
    thread input_thread(user_input_thread, sock);

    while (running) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    if (input_thread.joinable()) input_thread.join();

    close(sock);
    return 0;
}