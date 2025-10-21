
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

string server_response = "ACK";

atomic<bool> running(true); // https://cplusplus.com/reference/atomic/

void user_input_thread(int sock) {
    while (running) {
        cout << ">> ";
        string command;
        if (!getline(cin, command)) {
            running = false;
            break;
        }

        if (str_tolower(command) == "exit" or str_tolower(command) == "quit") {
            cout << "Closing Connection...\n";
            running = false;
            break;
        }

        if (str_tolower(command) == "send") {
            send_message(sock);
        } else if (str_tolower(command) == "read") {
            read_message(sock);
        } else if (str_tolower(command) == "list") {
            list_messages(sock);
        } else {
            cout << "Entered Command Unknown: " << command << endl;
        }
    }
}

int server_error(int sock) {
	close(sock);
	return -1;
}


int main(int argc, char* argv[]) {
    // Argumente auswerten falls vorhanden
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = atoi(argv[2]);
    }

    // Socket erstellen
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Error Creating Socket\n";
        return 1;
    }

    // Server-Adresse konfigurieren
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Invalid Address\n";
        return 1;
    }

    // Verbindung herstellen
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Connection Failed\n";
        return 1;
    }

    // Anfangsnachricht empfangen
    string welcome_msg = "connected";
    if (send(sock, welcome_msg.c_str(), welcome_msg.size(), 0) == -1) {
        cerr << "Error Sending Message\n";
        close(sock);
        return 1;
    }

    // Antwort empfangen
    char buffer[1024] = {0};
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        if (string(buffer) == server_response) {
			cout << "Succesfully connected to Server("<<server_ip<<") over Port "<<server_port<<" !" << endl;
            thread input_thread(user_input_thread, sock);
            while (running) {
                this_thread::sleep_for(chrono::milliseconds(100));
            }
            if (input_thread.joinable())
                input_thread.join();
		} else {
			cout << "Unexpected Response from Server: " << buffer << endl;
    		return server_error(sock);
		}
    } else if (bytes_received == 0) {
        cerr << "Server Closed Connection"<< endl;
    	return server_error(sock);
    } else {
        cerr << "Error Connecting To Server"<< endl;
    	return server_error(sock);
    }

    // Socket schließen
    close(sock);
    return 0;
}