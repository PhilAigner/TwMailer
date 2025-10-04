
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

// Standardwerte
string server_ip = "127.0.0.1";
int server_port = 8080;

string server_response = "ACK";



void handle_mail() {
	cout << "Hier muss die Mail-Funktionalität implementiert werden." << endl;
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
        cerr << "Fehler beim Erstellen des Sockets\n";
        return 1;
    }

    // Server-Adresse konfigurieren
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Ungültige Adresse/Adresse nicht unterstützt\n";
        return 1;
    }

    // Verbindung herstellen
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Verbindung fehlgeschlagen\n";
        return 1;
    }


    // Anfangsnachricht empfangen
    string welcome_msg = "connected";
    if (send(sock, welcome_msg.c_str(), welcome_msg.size(), 0) == -1) {
        cerr << "Fehler beim Senden der Nachricht\n";
        close(sock);
        return 1;
    }

    // Antwort empfangen
    char buffer[1024] = {0};
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        if (string(buffer) == server_response) {
			cout << "Erfolgreich mit Server verbunden!" << endl;
			handle_mail();
		} else {
			cout << "Unerwartete Antwort vom Server: " << buffer << endl;
    		return server_error(sock);
		}
    } else if (bytes_received == 0) {
        cerr << "Server hat die Verbindung geschlossen\n";
    	return server_error(sock);
    } else {
        cerr << "Fehler beim Verbinden mit Server\n";
    	return server_error(sock);
    }

    // Socket schließen
    close(sock);
    return 0;
}