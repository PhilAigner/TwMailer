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

using namespace std;

struct User {
    std::string username;
    std::string password;
};
//hardcoded test user
const User test_user = {"testuser", "testpassword"};
//



// Configurable base directory for user mail storage. Defaults to "users".
static filesystem::path BASE_DIR = filesystem::path("~/mailspool");

// set_base_dir: change the base directory used by save_mail
void set_base_dir(const string& path) {
	BASE_DIR = filesystem::path(path);
}
filesystem::path get_base_dir() {
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
    return username == test_user.username && password == test_user.password;
}

// save_mail: saves `msg` for `username` under <BASE_DIR>/<username>/<uuid>.txt
// The file content is structured with recipient, subject, and message body
// Returns true on success, false otherwise.
bool save_mail(const string& username, const string& msg) {
	try {
		namespace fs = filesystem;

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
        namespace fs = filesystem;

        fs::path base = BASE_DIR;
        fs::path user_dir = base / username;

        // Prüfen, ob Verzeichnis existiert
        if (!fs::exists(user_dir) || !fs::is_directory(user_dir)) {
            return "ERR|User directory not found";
        }

        ostringstream result;
        int count = 0;

        // Alle Dateien im User-Verzeichnis durchgehen
        for (const auto& entry : fs::directory_iterator(user_dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".txt")
                continue;

            ifstream ifs(entry.path());
            if (!ifs) {
                cerr << "list_mails: failed to open file '" << entry.path() << "'\n";
                continue;
            }

            string line, sender, subject, date;
            while (getline(ifs, line)) {
                if (line.find("Sender: ") == 0)
                    sender = line.substr(8);
                else if (line.find("Subject: ") == 0)
                    subject = line.substr(9);
                else if (line.find("Date: ") == 0)
                    date = line.substr(6);
            }
            ifs.close();

            // Dateiname als ID
            string filename = entry.path().filename().string();
            if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".txt")
                filename.erase(filename.size() - 4); // ".txt" entfernen

            // Zeile im Format: ID|Sender|Subject|Date
            result << filename << "|" << sender << "|" << subject << "|" << date << "\n";
            count++;
        }

        if (count == 0) {
            return "ERR|No messages available";
        }

        return "OK|" + to_string(count) + "\n" + result.str();

    } catch (const exception& e) {
        cerr << "list_mails: exception: " << e.what() << "\n";
        return "ERR|Exception while listing mails";
    }
}

// read_mail: searches for mails in username's directory matching the subject (partial match)
// Returns a string with all matching mails or an error message
string read_mail(const string& username, const string& subject_search) {
	try {
		namespace fs = filesystem;
		
		// Get user directory
		fs::path base = BASE_DIR;
		fs::path user_dir = base / username;
		
		// Check if directory exists
		if (!fs::exists(user_dir) || !fs::is_directory(user_dir)) {
			return "ERR: No mails found for user '" + username + "'";
		}
		
		ostringstream result;
		int count = 0;
		
		// Iterate through all files in user directory
		for (const auto& entry : fs::directory_iterator(user_dir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".txt") {
				// Read file content
				ifstream ifs(entry.path());
				if (!ifs) {
					cerr << "read_mail: failed to open file '" << entry.path() << "'\n";
					continue;
				}
				
				string line;
				string sender, recipient, subject, date, message;
				bool in_message = false;
				
				// Parse file content
				while (getline(ifs, line)) {
					if (line.find("Sender: ") == 0) {
						sender = line.substr(8);
					} else if (line.find("Recipient: ") == 0) {
						recipient = line.substr(11);
					} else if (line.find("Subject: ") == 0) {
						subject = line.substr(9);
					} else if (line.find("Date: ") == 0) {
						date = line.substr(6);
					} else if (line.find("Message:") == 0) {
						in_message = true;
					} else if (in_message) {
						if (!message.empty()) message += "\n";
						message += line;
					}
				}
				ifs.close();
				
				// Check if subject matches (case-insensitive partial match)
				string subject_lower = subject;
				string search_lower = subject_search;
				transform(subject_lower.begin(), subject_lower.end(), subject_lower.begin(), ::tolower);
				transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
				
				if (subject_lower.find(search_lower) != string::npos) {
					count++;
					result << "=== Mail #" << count << " ===\n";
					result << "From: " << sender << "\n";
					result << "To: " << recipient << "\n";
					result << "Subject: " << subject << "\n";
					result << "Date: " << date << "\n";
					result << "Message:\n" << message << "\n";
					result << "==================\n\n";
				}
			}
		}
		
		if (count == 0) {
			return "No mails found matching subject '" + subject_search + "'";
		}
		
		return to_string(count) + " mail(s) found:\n\n" + result.str();
		
	} catch (const exception& e) {
		cerr << "read_mail: exception: " << e.what() << "\n";
		return "ERR: Exception during mail search";
	}
}

