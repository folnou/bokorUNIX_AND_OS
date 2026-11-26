#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>

using namespace std;
namespace fs = filesystem;

string calculate_hash(const fs::path& path) {
    ifstream file(path, ios::binary);
    if (!file) return "";

    uint64_t hash = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;
    char buffer[8192];

    while (file.read(buffer, sizeof(buffer))) {
        for (size_t i = 0; i < static_cast<size_t>(file.gcount()); ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= prime;
        }
    }
    for (size_t i = 0; i < static_cast<size_t>(file.gcount()); ++i) {
        hash ^= static_cast<uint8_t>(buffer[i]);
        hash *= prime;
    }

    stringstream ss;
    ss << hex << setfill('0') << setw(16) << hash;
    return ss.str();
}

bool same_content(const fs::path& a, const fs::path& b) {
    ifstream fa(a, ios::binary), fb(b, ios::binary);
    if (!fa || !fb) return false;

    char ba[8192], bb[8192];
    while (fa && fb) {
        fa.read(ba, sizeof(ba));
        fb.read(bb, sizeof(bb));
        if (fa.gcount() != fb.gcount()) return false;
        if (memcmp(ba, bb, static_cast<size_t>(fa.gcount())) != 0) return false;
    }
    return fa.eof() && fb.eof();
}

bool replace_with_hard_link(const fs::path& original, const fs::path& duplicate) {
    fs::path tmp = duplicate;
    tmp += ".hl_tmp";

    error_code ec;
    if (fs::exists(tmp, ec)) {
        cerr << "    Error: temp file already exists: " << tmp << "\n";
        return false;
    }

    fs::create_hard_link(original, tmp, ec);
    if (ec) {
        cerr << "    Error creating hard link: " << ec.message() << "\n";
        return false;
    }

    fs::rename(tmp, duplicate, ec);
    if (ec) {
        cerr << "    Error replacing file: " << ec.message() << "\n";
        error_code ec2;
        fs::remove(tmp, ec2);
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <directory_path>\n";
        return 1;
    }

    fs::path target_dir = argv[1];
    error_code ec;
    if (!fs::exists(target_dir, ec) || !fs::is_directory(target_dir, ec)) {
        cerr << "Error: Directory does not exist or is not a directory.\n";
        return 1;
    }

    map<string, vector<fs::path>> hash_map;

    fs::recursive_directory_iterator it(
        target_dir, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            cerr << "Walk error: " << ec.message() << "\n";
            ec.clear();
            continue;
        }
        if (it->symlink_status(ec).type() != fs::file_type::regular) continue;

        string hash = calculate_hash(it->path());
        if (!hash.empty()) {
            hash_map[hash].push_back(it->path());
        }
    }

    int replaced_count = 0;
    int error_count = 0;

    for (auto& [hash, files] : hash_map) {
        if (files.size() > 1) {
            fs::path original = files[0];
            cout << "Found duplicates (Hash: " << hash << "):\n";
            cout << "  [KEEP] " << original << "\n";

            for (size_t i = 1; i < files.size(); ++i) {
                fs::path duplicate = files[i];

                if (fs::equivalent(original, duplicate, ec)) {
                    cout << "  [SKIP] " << duplicate << " (already a hard link)\n";
                    continue;
                }

                if (!same_content(original, duplicate)) {
                    cout << "  [SKIP] " << duplicate << " (hash collision, content differs)\n";
                    continue;
                }

                cout << "  [LINK] " << duplicate << "\n";

                if (replace_with_hard_link(original, duplicate)) {
                    replaced_count++;
                } else {
                    error_count++;
                }
            }
        }
    }

    cout << "\nDone. Replaced " << replaced_count << " files with hard links.";
    if (error_count > 0) cout << " Errors: " << error_count << ".";
    cout << "\n";
    return error_count > 0 ? 2 : 0;
}
