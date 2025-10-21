#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <atomic>
#include <thread>
#include <unistd.h>

using namespace std;

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

    cout << "<< Recipient" << endl <<">> ";
    getline(cin, recipient);
    recipient = trim(recipient);

    cout << "<< Subject "<< endl << ">> ";
    getline(cin, subject);
    subject = trim(subject);

    cout << "<< Message (end with a single '.' on a line)"<<endl;
    while (true) {
        cout << ">> ";
        getline(cin, line);
        if (line == ".") break;
        message += line + "\n";
    }
    message = trim(message);

    if (recipient.empty() || message.empty()) {
        cerr << "Error: Recipient Or Message is Empty."<<endl;
        return;
    }

    // Construct message string (format: SEND|recipient|subject|message)
    string full_msg = "SEND|" + recipient + "|" + subject + "|" + message;

    if (send(sock, full_msg.c_str(), full_msg.size(), 0) == -1) {
        cerr << "Error Sending The Message.\n";
    } else {
        cout << "Message Sent To Server.\n";
    }

    // TODO ACK|ERR empfangen und auswerten

}

void read_message(int sock,std::string& username, const std::string& index_str) {
    string input = trim(index_str);
    if (input.empty()) {
        cout << "No index provided.\n";
        return;
    }

    // Prüfen, ob es eine Zahl ist
    for (char c : input) {
        if (!isdigit(c)) {
            cout << "Invalid index. Please enter a number.\n";
            return;
        }
    }

    // Nachricht an Server senden: READ|index
    string txt = "READ|" + username + "|" + input;
    if (send(sock, txt.c_str(), txt.size(), 0) == -1) {
        cerr << "Fehler beim Senden der Nachricht.\n";
        return;
    }

    // Server-Antwort empfangen (Loop für längere Nachrichten)
    const int BUFFER_SIZE = 4096;
    string response;
    char buffer[BUFFER_SIZE];
    int bytes_received = 0;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        response += buffer;
        if (bytes_received < BUFFER_SIZE - 1)
            break;
    }

    if (bytes_received < 0) {
        cerr << "Fehler beim Empfangen der Server-Antwort.\n";
        return;
    }

    // Antwort auswerten
    if (response.rfind("OK|", 0) == 0) {
        cout << "Mail gelesen:\n" << response.substr(3) << endl;
    } else if (response.rfind("ERR|", 0) == 0) {
        cerr << "Server Error: " << response.substr(4) << endl;
    } else {
        cout << "Unbekannte Antwort vom Server:\n" << response << endl;
    }
}


void list_messages(int sock) {
    string cmd = "LIST";
    if (send(sock, cmd.c_str(), cmd.size(), 0) == -1) {
        cerr << "Error Sending LIST-Command."<< endl;
        return;
    }

    char buffer[8192] = {0}; // größerer Buffer für mehrere Nachrichten
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        cerr << "Error Receiving Message List."<< endl;
        return;
    }

    buffer[bytes_received] = '\0';
    string response = buffer;

    // Fehlermeldung prüfen
    if (response.rfind("ERR", 0) == 0) {
        cerr << "Server Error: " << response.substr(4) << endl;
        return;
    }

    // Wenn OK|n enthalten ist, n extrahieren
    if (response.rfind("OK|", 0) == 0) {
        size_t pos = response.find('\n');
        if (pos == string::npos) pos = response.size();
        string header = response.substr(0, pos);
        cout << "<< Available Messages >>"<< endl;

        // Rest (ab pos+1) enthält eigentliche Nachrichtenliste
        string list = response.substr(pos + 1);
        if (list.empty()) {
            cout << "(No Messages available)"<< endl;
            return;
        }

        // Zeilenweise ausgeben
        size_t start = 0;
        while (start < list.size()) {
            size_t end = list.find('\n', start);
            string line = list.substr(start, end - start);
            if (!line.empty()) cout << "  " << line << endl;
            if (end == string::npos) break;
            start = end + 1;
        }
    } else {
        cout << "Unexpected Response From Server: " << response << endl;
    }
}