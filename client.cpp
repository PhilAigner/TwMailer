
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <atomic>
#include <thread>
#include <unistd.h>

using namespace std;

// Standardwerte
string server_ip = "127.0.0.1";
int server_port = 8080;

string server_response = "ACK";

atomic<bool> running(true); // https://cplusplus.com/reference/atomic/

string str_tolower(const string& s) {
    string result = s;
    for (char& c : result) {
        c = tolower(c);
    }
    return result;
}

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

void send_message(int sock) {
    string recipient, subject, message, line;

    cout << "<< Recipient\n>> ";
    getline(cin, recipient);
    recipient = trim(recipient);

    cout << "<< Subject\n>> ";
    getline(cin, subject);
    subject = trim(subject);

    cout << "<< Message (end with a single '.' on a line)\n";
    while (true) {
        cout << ">> ";
        getline(cin, line);
        if (line == ".") break;
        message += line + "\n";
    }
    message = trim(message);

    if (recipient.empty() || message.empty()) {
        cerr << "Fehler: Empfänger oder Nachricht ist leer.\n";
        return;
    }

    // Construct message string (format: SEND|recipient|subject|message)
    string full_msg = "SEND|" + recipient + "|" + subject + "|" + message;

    if (send(sock, full_msg.c_str(), full_msg.size(), 0) == -1) {
        cerr << "Fehler beim Senden der Nachricht.\n";
    } else {
        cout << "Nachricht an Server gesendet.\n";
    }

    // TODO ACK|ERR empfangen und auswerten

}

void read_message(int sock) {
    cout << "Hier muss die Lese-Funktionalität implementiert werden." << endl;

    string txt = "READ|123|123";

    if (send(sock, txt.c_str(), txt.size(), 0) == -1) {
        cerr << "Fehler beim Senden der Nachricht.\n";
    } else {
        cout << "Nachricht an Server gesendet.\n";
    }

    // Antwort empfangen
    char buffer[4096] = {0};
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        cout << "Antwort vom Server: " << buffer << endl;
    } else {
        cerr << "Fehler beim Empfangen der Antwort vom Server.\n";
    }


    // TODO ACK|ERR empfangen und auswerten
    
}

void user_input_thread(int sock) {
    while (running) {
        cout << ">> ";
        string command;
        if (!getline(cin, command)) {
            running = false;
            break;
        }

        if (str_tolower(command) == "exit" or str_tolower(command) == "quit") {
            cout << "Beende Verbindung...\n";
            running = false;
            break;
        }

        if (str_tolower(command) == "send") {
            send_message(sock);
        } else if (str_tolower(command) == "read") {
            read_message(sock);
        } else {
            cout << "Eingegebenes Kommando unbekannt: " << command << endl;
        }
    }
}


void handle_mail() {
	cout << "Hier muss die Mail-Funktionalität implementiert werden." << endl;
	sleep(10); // Simuliere Mail-Verarbeitung
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
			cout << "Erfolgreich mit Server("<<server_ip<<") über Port "<<server_port<<" verbunden!" << endl;
            thread input_thread(user_input_thread, sock);
            while (running) {
                this_thread::sleep_for(chrono::milliseconds(100));
            }
            if (input_thread.joinable())
                input_thread.join();
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