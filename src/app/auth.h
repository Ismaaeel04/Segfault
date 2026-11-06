// ============================================================================
// auth.h  -  Local student accounts: register, log in, per-student data folder.
// No UI and no raylib here, so the unit tests (auth_tests) can link it directly.
//
// On disk, under <root>/:
//   users.txt          one line per student: user <name> <iterations> <salt> <hash>
//   user_<name>/       that student's progress and saved code (name in lowercase)
// Passwords are never stored: only a salted PBKDF2-HMAC-SHA256 hash.
// ============================================================================
#pragma once
#include <string>
#include <vector>

namespace auth {

constexpr int kDefaultIterations = 600000;   // OWASP guidance for PBKDF2-HMAC-SHA256
constexpr int kMinUsername = 3, kMaxUsername = 24;
constexpr int kMinPassword = 8, kMaxPassword = 64;

// Lowercase hex digest of SHA-256(data).
std::string Sha256Hex(const std::string& data);
// Lowercase hex of PBKDF2-HMAC-SHA256.
std::string Pbkdf2Hex(const std::string& password, const std::string& salt, int iterations, int outLen);

// Both return an error message for the student, or "" when the value is fine.
std::string ValidateUsername(const std::string& username);
std::string ValidatePassword(const std::string& password);

struct AuthResult {
    bool ok = false;
    std::string error;          // shown to the student when !ok
    std::string username;       // spelled as it was registered
    bool firstAccount = false;  // Register only: no account existed before this one
};

class UserStore {
public:
    explicit UserStore(std::string rootDir, int iterations = kDefaultIterations);

    AuthResult Register(const std::string& username, const std::string& password) const;
    AuthResult Login(const std::string& username, const std::string& password) const;

    // Folder for a student's data, with a trailing slash. Names are restricted
    // to letters, digits, '_' and '-', and the "user_" prefix keeps Windows
    // reserved names such as "con" safe to use.
    std::string UserDir(const std::string& username) const;

    // Moves data saved before accounts existed (<legacyDir>/segfault_progress.txt
    // and <legacyDir>/segfault_code/) into this student's folder.
    void AdoptLegacyData(const std::string& username, const std::string& legacyDir) const;

private:
    struct Record { std::string username, salt, hash; int iterations = 0; };
    std::vector<Record> ReadRecords() const;
    std::string root;
    int iterations;
};

} // namespace auth
