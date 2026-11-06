// ============================================================================
// test_auth.cpp  -  Console unit tests for the student accounts (src/app/auth).
// Build target: auth_tests   (run it, or use `ctest` in the build folder)
// ============================================================================
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "../src/app/auth.h"

namespace fs = std::filesystem;

static int passed = 0, failed = 0;

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    fs::path base = fs::temp_directory_path() / "segfault_auth_test";
    std::error_code ec;
    fs::remove_all(base, ec);
    std::string root = (base / "users").string();
    const int fast = 10;                      // low iteration count to keep the tests quick
    auth::UserStore store(root, fast);

    printf("[SHA-256: NIST vectors]\n");
    CHECK(auth::Sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(auth::Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(auth::Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(auth::Sha256Hex(std::string(1000000, 'a')) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    printf("[PBKDF2-HMAC-SHA256: published vectors]\n");
    CHECK(auth::Pbkdf2Hex("password", "salt", 1, 32) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    CHECK(auth::Pbkdf2Hex("password", "salt", 4096, 32) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    CHECK(auth::Pbkdf2Hex("passwd", "salt", 1, 64) ==                 // RFC 7914, two output blocks
          "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
          "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");

    printf("[validation]\n");
    CHECK(auth::ValidateUsername("C00123456").empty());
    CHECK(auth::ValidateUsername("a_b-c").empty());
    CHECK(!auth::ValidateUsername("ab").empty());
    CHECK(!auth::ValidateUsername(std::string(25, 'a')).empty());
    for (const char* bad : {"../evil", "a/b", "a\\b", "a b", "a.b", "...", "ma\xc3\xb1" "ana"})
        CHECK(!auth::ValidateUsername(bad).empty());
    CHECK(auth::ValidatePassword("12345678").empty());
    CHECK(!auth::ValidatePassword("1234567").empty());
    CHECK(!auth::ValidatePassword(std::string(65, 'x')).empty());

    printf("[register and log in]\n");
    {
        auth::AuthResult r = store.Register("Alice", "correct horse");
        CHECK(r.ok && r.username == "Alice" && r.firstAccount);
        CHECK(fs::is_directory(store.UserDir("Alice")));
        CHECK(store.Login("Alice", "correct horse").ok);
        CHECK(store.Login("alice", "correct horse").username == "Alice");     // any case logs in
        CHECK(!store.Login("Alice", "correct horsE").ok);
        CHECK(!store.Login("Alice", "").ok);
        CHECK(!store.Login("nobody", "correct horse").ok);
        // the same message for both, so it doesn't reveal which usernames exist
        CHECK(store.Login("Alice", "wrong").error == store.Login("nobody", "wrong").error);
    }

    printf("[rejected registrations]\n");
    {
        CHECK(!store.Register("Alice", "another password").ok);
        CHECK(!store.Register("ALICE", "another password").ok);
        CHECK(store.Login("Alice", "correct horse").ok);                      // not overwritten
        CHECK(!store.Register("../evil", "password123").ok);
        CHECK(!fs::exists(base / "evil"));
        CHECK(!store.Register("bob", "short").ok);
        CHECK(!store.Login("bob", "short").ok);
    }

    printf("[what is on disk]\n");
    {
        auth::AuthResult r = store.Register("bob", "correct horse");          // same password as Alice
        CHECK(r.ok && !r.firstAccount);
        std::string file = readFile(root + "/users.txt");
        CHECK(file.find("correct horse") == std::string::npos);
        std::istringstream lines(file);
        std::string tag, name, salt1, hash1, salt2, hash2;
        int it1 = 0, it2 = 0;
        lines >> tag >> name >> it1 >> salt1 >> hash1 >> tag >> name >> it2 >> salt2 >> hash2;
        CHECK(it1 == fast && salt1.size() == 32 && hash1.size() == 64);
        CHECK(salt1 != salt2 && hash1 != hash2);                              // salted per student
        CHECK(store.UserDir("Alice") != store.UserDir("bob"));
    }

    printf("[persistence and iteration upgrades]\n");
    {
        // A new store with a different default still checks old accounts
        // against the iteration count saved with each of them.
        auth::UserStore other(root, fast + 5);
        CHECK(other.Login("Alice", "correct horse").ok);
        CHECK(other.Register("carol", "password123").ok);
        CHECK(store.Login("carol", "password123").ok);
    }

    printf("[damaged accounts file]\n");
    {
        { std::ofstream out(root + "/users.txt", std::ios::app); out << "garbage line\nuser x\nuser ../evil 10 aa bb\nuser dave -1 aa bb\n"; }
        CHECK(store.Login("Alice", "correct horse").ok);
        CHECK(!store.Login("dave", "aa").ok);
        CHECK(store.Register("erin", "password123").ok);
        CHECK(store.Login("erin", "password123").ok);
    }

    printf("[reserved names and legacy data]\n");
    {
        CHECK(store.Register("con", "password123").ok);                       // reserved name on Windows
        CHECK(fs::is_directory(store.UserDir("con")));
        fs::path legacy = base / "legacy";
        fs::create_directories(legacy / "segfault_code");
        { std::ofstream(legacy / "segfault_progress.txt", std::ios::binary) << "xp 40\n"; }
        { std::ofstream(legacy / "segfault_code" / "p1.cpp", std::ios::binary) << "int x;\n"; }
        store.AdoptLegacyData("Alice", legacy.string());
        CHECK(readFile(store.UserDir("Alice") + "segfault_progress.txt") == "xp 40\n");
        CHECK(readFile(store.UserDir("Alice") + "segfault_code/p1.cpp") == "int x;\n");
        CHECK(!fs::exists(legacy / "segfault_progress.txt") && !fs::exists(legacy / "segfault_code"));
        store.AdoptLegacyData("bob", legacy.string());                        // nothing left to move
        CHECK(!fs::exists(store.UserDir("bob") + "segfault_progress.txt"));
    }

    fs::remove_all(base, ec);
    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
