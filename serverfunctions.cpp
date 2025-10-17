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
#include <uuid/uuid.h>

using namespace std;

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


// save_mail: saves `msg` for `username` under <BASE_DIR>/<username>/<uuid>.txt
// The file content is prefixed with <timestamp>_<uuid>\n followed by the original message.
// Returns true on success, false otherwise.
bool save_mail(const string& username, const string& msg) {
	try {
		namespace fs = filesystem;

		// Ensure base users directory and user directory exist
		fs::path base = BASE_DIR;
		fs::path user_dir = base / username;
		error_code ec;
		if (!fs::create_directories(user_dir, ec) && ec) {
			cerr << "save_mail: failed to create directory '" << user_dir << "': " << ec.message() << "\n";
			return false;
		}

		// Generate a UUID v4 (using libuuid)
		string uuid = generate_uuid();

		// Timestamp (milliseconds since epoch)
		auto now = chrono::system_clock::now();
		auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
		string timestamp = to_string(ms);

		// Compose id and content
		string id = timestamp + "_" + uuid;
		string content = timestamp + "\n" + msg;

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

		cout << "save_mail: saved mail for user '" << username << "' to '" << file_path << "'\n";
		return true;
	} catch (const exception& e) {
		cerr << "save_mail: exception: " << e.what() << "\n";
		return false;
	}
}

