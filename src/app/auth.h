// ============================================================================
// auth.h  -  Local student accounts: register, log in, per-student data folder.
// No UI and no raylib here, so the unit tests (auth_tests) can link it directly.
//
// On disk, under <root>/:
//   users.txt          one line per student:
//                      user <name> <iterations> <salt> <hash> [<code salt> <code hash>]
//   user_<name>/       that student's progress and saved code (name in lowercase)
// Passwords are never stored: only a salted PBKDF2-HMAC-SHA256 hash. The same
// goes for the recovery code, which resets a forgotten password and works once.
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

// A new random recovery code, like "ABCD-EFGH-JKMN" (about 59 bits). It has no
// 0/O, 1/I/L or U, so it is hard to copy down wrongly.
std::string NewRecoveryCode();
// Uppercase, without dashes or spaces: "abcd efgh-jkmn" -> "ABCDEFGHJKMN".
std::string NormalizeRecoveryCode(const std::string& code);

struct AuthResult {
    bool ok = false;
    std::string error;          // shown to the student when !ok
    std::string username;       // spelled as it was registered
    bool firstAccount = false;  // Register only: no account existed before this one
    std::string recoveryCode;   // Register, ResetPassword, CreateRecoveryCode: show it once
};

class UserStore {
public:
    explicit UserStore(std::string rootDir, int iterations = kDefaultIterations);

    AuthResult Register(const std::string& username, const std::string& password) const;
    AuthResult Login(const std::string& username, const std::string& password) const;

    // Forgot password: sets a new password if the recovery code is right. The
    // code is then used up and a new one is returned.
    AuthResult ResetPassword(const std::string& username, const std::string& code,
                             const std::string& newPassword) const;
    // Accounts made before recovery codes existed have none.
    bool HasRecoveryCode(const std::string& username) const;
    // Replaces the student's recovery code. Only call it for a student who has
    // just proved who they are (logged in).
    AuthResult CreateRecoveryCode(const std::string& username) const;

    // Folder for a student's data, with a trailing slash. Names are restricted
    // to letters, digits, '_' and '-', and the "user_" prefix keeps Windows
    // reserved names such as "con" safe to use.
    std::string UserDir(const std::string& username) const;

    // Moves data saved before accounts existed (<legacyDir>/segfault_progress.txt
    // and <legacyDir>/segfault_code/) into this student's folder.
    void AdoptLegacyData(const std::string& username, const std::string& legacyDir) const;

private:
    struct Record {
        std::string username, salt, hash;
        int iterations = 0;
        std::string codeSalt, codeHash;   // empty: no recovery code
        size_t line = 0;                  // where it is in users.txt
    };
    std::vector<Record> ReadRecords() const;
    bool FindRecord(const std::string& username, Record& out) const;
    // Rewrites one student's line; every other line is kept exactly as it was.
    bool WriteRecord(const Record& r) const;
    static std::string Line(const Record& r);
    std::string root;
    int iterations;
};

} // namespace auth
