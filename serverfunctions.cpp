// serverfunctions.cpp
// Utility functions for server operations (saving mails to disk)

#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <array>
#include <random>
#include <fstream>
#include <iostream>
#include <algorithm>	
#include <uuid/uuid.h>

#define ACK "ACK"
#define ERR "ERR"

#include "ldap.cpp"

using namespace std;

struct User {
    std::string username;
    std::string password;
};
//hardcoded test user
const User test_user = {"testuser", "testpwd"};
//



// Configurable base directory for user mail storage. Defaults to "users".
namespace fs = filesystem;
static fs::path BASE_DIR = fs::path("~/mailspool");

// set_base_dir: change the base directory used by save_mail
void set_base_dir(const string& path) {
	BASE_DIR = fs::path(path);
}
fs::path get_base_dir() {
	return BASE_DIR;
}

// generate_uuid: wrapper around libuuid to produce a lower-case UUID string
static string generate_uuid() {
	uuid_t bin;
	uuid_generate_random(bin);
	char str[37]; // 36 chars + null
	uuid_unparse_lower(bin, str);
	return string(str);
}

string str_tolower(const string& s) {
    string result = s;
    for (char& c : result) {
        c = tolower(c);
    }
    return result;
}

bool validate_login(const std::string& username, const std::string& password) {
    // first check hardcoded test user
    if (username == test_user.username && password == test_user.password) {
        return true;
    }
    
    // TODO implement max 3 tries
    return (ldap_login(username.c_str(), password.c_str()) == EXIT_SUCCESS);
}

// save_mail: saves `msg` for `username` under <BASE_DIR>/<username>/<uuid>.txt
// Returns true on success, false otherwise.
bool save_mail(const string& username, const string& msg) {
	try {
		
		// Parse message format: recipient|subject|message
		size_t first_pipe = msg.find('|');
		size_t second_pipe = msg.find('|', first_pipe + 1);
		
		if (first_pipe == string::npos || second_pipe == string::npos) {
			cerr << "save_mail: invalid message format (expected: recipient|subject|message)\n";
			return false;
		}
		
		string recipient = msg.substr(0, first_pipe);
		string subject = msg.substr(first_pipe + 1, second_pipe - first_pipe - 1);
		string message = msg.substr(second_pipe + 1);

		// Ensure base users directory and user directory exist
		fs::path base = BASE_DIR;
		fs::path user_dir = base / recipient; // Speichere im Verzeichnis des Empfängers
		error_code ec;
		if (!fs::create_directories(user_dir, ec) && ec) {
			cerr << "save_mail: failed to create directory '" << user_dir << "': " << ec.message() << "\n";
			return false;
		}

		// Generate a UUID v4 (using libuuid)
		string uuid = generate_uuid();

		// Timestamp (milliseconds since epoch) für Dateiname
		auto now = chrono::system_clock::now();
		auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
		string timestamp_ms = to_string(ms);

		// Formatiertes Datum und Uhrzeit für Anzeige
		time_t now_c = chrono::system_clock::to_time_t(now);
		tm local_tm = *localtime(&now_c);
		ostringstream oss;
		oss << put_time(&local_tm, "%d.%m.%Y %H:%M:%S");
		string datetime_str = oss.str();

		// Compose id and content with structured format
		string id = timestamp_ms + "_" + uuid;
		string content = "Sender: " + username + "\n";
		content += "Recipient: " + recipient + "\n";
		content += "Subject: " + subject + "\n";
		content += "Date: " + datetime_str + "\n";
		content += "Message:\n" + message;

		fs::path file_path = user_dir / (id + ".txt");
		ofstream ofs(file_path, ios::binary);
		if (!ofs) {
			cerr << "save_mail: failed to open file '" << file_path << "' for writing\n";
			return false;
		}
		ofs << content;
		ofs.close();
		if (!ofs) {
			cerr << "save_mail: error while writing file '" << file_path << "'\n";
			return false;
		}

		cout << "save_mail: saved mail for user '" << recipient << "' to '" << file_path << "'\n";
		return true;
	} catch (const exception& e) {
		cerr << "save_mail: exception: " << e.what() << "\n";
		return false;
	}
}


string list_mails(const string& username) {
    try {
        fs::path user_dir = BASE_DIR / username;

        if (!fs::exists(user_dir) || !fs::is_directory(user_dir)) {
            return string(ERR) + "User directory not found";
        }

        ostringstream result;
        vector<filesystem::path> mails;

        // Alle .txt Dateien sammeln
        for (const auto& entry : fs::directory_iterator(user_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                mails.push_back(entry.path());
            }
        }

        if (mails.empty()) {
            return "No messages available";
        }

        // Mails sortieren (optional nach Dateiname)
        //sort(mails.begin(), mails.end());
        // DARF MAN NICHT SORTIEREN, WEIL SONST INDEX NICHT STIMMT BEIM LÖSCHEN/LESEN!!!
        // TODO sort immer (Datum) dann beim Lesen/delete auch danach sortieren

        // Indexierte Ausgabe
        for (size_t i = 0; i < mails.size(); ++i) {
            ifstream ifs(mails[i]);
            if (!ifs) continue;

            string line, sender, subject, date;
            while (getline(ifs, line)) {
                if (line.find("Sender: ") == 0) sender = line.substr(8);
                else if (line.find("Subject: ") == 0) subject = line.substr(9);
                else if (line.find("Date: ") == 0) date = line.substr(6);
            }
            ifs.close();

            result << "[" << (i + 1) << "] " << sender << "|" << subject << "|" << date << "\n";
        }

        return to_string(mails.size()) + "\n" + result.str();

    } catch (const exception& e) {
        cerr << "list_mails: exception: " << e.what() << "\n";
        return string(ERR) + "Exception while listing mails";
    }
}

// read_mail: searches for mails in username's directory matching the subject (partial match)
// Returns a string with all matching mails or an error message
string read_mail(const string& username, int index) {
    fs::path base = BASE_DIR;

    vector<fs::path> mail_files;

    // Rekursiv alle .txt Dateien sammeln
    for (const auto& entry : fs::recursive_directory_iterator(base)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt")
            mail_files.push_back(entry.path());
    }

    if (mail_files.empty()) return string(ERR) + "No mails available";
    
    //sort so that index matches
    sort(mail_files.begin(), mail_files.end());


    if (index < 1 || index > (int)mail_files.size())
        return string(ERR) + "Index out of range";

    fs::path target_file = mail_files[index - 1];

    // Mail aus Datei lesen
    ifstream ifs(target_file);
    if (!ifs) return string(ERR) + "Failed to open mail file";

    string line, sender, recipient, subject, date, message;
    bool in_message = false;

    while (getline(ifs, line)) {
        if (line.find("Sender: ") == 0) sender = line.substr(8);
        else if (line.find("Recipient: ") == 0) recipient = line.substr(11);
        else if (line.find("Subject: ") == 0) subject = line.substr(9);
        else if (line.find("Date: ") == 0) date = line.substr(6);
        else if (line.find("Message:") == 0) in_message = true;
        else if (in_message) {
            if (!message.empty()) message += "\n";
            message += line;
        }
    }
    ifs.close();

    ostringstream oss;
    oss << "From: " << sender << "\n";
    oss << "To: " << recipient << "\n";
    oss << "Subject: " << subject << "\n";
    oss << "Date: " << date << "\n";
    oss << "Message:\n" << message << "\n";

    return oss.str();
}

bool function_delete(int client_socket, char* buffer) {
    // Nachricht nach "DELETE|" extrahieren
    const char* msg_start = buffer + 7; // Länge von "DELETE|"
    std::string message(msg_start);

    std::cout << "DELETE Function Called With Message: " << message << std::endl;

    // Format: username|index
    size_t pipe_pos = message.find('|');
    if (pipe_pos == std::string::npos) {
        std::string err = string(ERR) + "Invalid message format";
        send(client_socket, err.c_str(), err.size(), 0);
        return false;
    }

    std::string username = message.substr(0, pipe_pos);
    std::string index_str = message.substr(pipe_pos + 1);

    int mail_index = 0;
    try {
        mail_index = std::stoi(index_str);
    } catch (...) {
        std::string err = string(ERR) + "Invalid mail index";
        send(client_socket, err.c_str(), err.size(), 0);
        return false;
    }

    if (mail_index <= 0) {
        std::string err = string(ERR) + "Mail index must be >= 1";
        send(client_socket, err.c_str(), err.size(), 0);
        return false;
    }

    // Benutzerverzeichnis prüfen
    fs::path user_dir = BASE_DIR / username;
    if (!fs::exists(user_dir) || !fs::is_directory(user_dir)) {
        std::string err = string(ERR) + "User directory not found";
        send(client_socket, err.c_str(), err.size(), 0);
        return false;
    }

    // Alle Mails auflisten
    std::vector<fs::path> user_mails;
    for (const auto& entry : fs::directory_iterator(user_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt")
            user_mails.push_back(entry.path());
    }

    //sort so that index matches
    sort(user_mails.begin(), user_mails.end());

    if (mail_index > (int)user_mails.size()) {
        std::string err = string(ERR) + "Mail index out of range";
        send(client_socket, err.c_str(), err.size(), 0);
        return false;
    }

    // Datei löschen
    cout << mail_index - 1 << endl;
    cout << user_mails[mail_index - 1] << endl;
    cout << user_mails.size() << endl;
    for (const auto& mail : user_mails) {
        std::cout << "Mail file: " << mail << std::endl;
    }

    fs::path mail_to_delete = user_mails[mail_index - 1];
    std::error_code ec;
    fs::remove(mail_to_delete, ec);

    if (ec) {
        std::string err = string(ERR) + "Failed to delete mail";
        send(client_socket, err.c_str(), err.size(), 0);
        return false;
    }

    std::cout << "function_delete: deleted mail #" << mail_index << " for user '" << username << "'\n";
    return true;
}