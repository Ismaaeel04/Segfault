#include "auth.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace auth {

// ============================================================== SHA-256 ====
namespace {

class Sha256 {
public:
    void Update(const uint8_t* data, size_t len) {
        total += len;
        while (len > 0) {
            size_t n = std::min(len, sizeof(buf) - used);
            std::memcpy(buf + used, data, n);
            used += n; data += n; len -= n;
            if (used == sizeof(buf)) { Compress(); used = 0; }
        }
    }
    void Final(uint8_t out[32]) {
        uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        Update(&pad, 1);
        pad = 0;
        while (used != 56) Update(&pad, 1);
        uint8_t lenBytes[8];
        for (int i = 0; i < 8; i++) lenBytes[i] = (uint8_t)(bits >> (56 - i * 8));
        Update(lenBytes, 8);
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 4; j++) out[i * 4 + j] = (uint8_t)(h[i] >> (24 - j * 8));
    }

private:
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t buf[64] = {};
    size_t used = 0;
    uint64_t total = 0;

    static uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void Compress() {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)buf[i * 4] << 24 | (uint32_t)buf[i * 4 + 1] << 16 | (uint32_t)buf[i * 4 + 2] << 8 | buf[i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
            uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
};

std::string ToHex(const uint8_t* data, size_t len) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; i++) { s += digits[data[i] >> 4]; s += digits[data[i] & 15]; }
    return s;
}

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string RandomSalt() {
    std::random_device rd;
    uint8_t salt[16];
    for (uint8_t& b : salt) b = (uint8_t)(rd() & 0xff);
    return ToHex(salt, sizeof(salt));
}

// Compares every character, so the time taken does not depend on where they differ.
bool SameText(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); i++) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace

std::string Sha256Hex(const std::string& data) {
    Sha256 s;
    s.Update((const uint8_t*)data.data(), data.size());
    uint8_t out[32];
    s.Final(out);
    return ToHex(out, sizeof(out));
}

std::string Pbkdf2Hex(const std::string& password, const std::string& salt, int iterations, int outLen) {
    // HMAC key: hashed if longer than a block, then zero padded.
    uint8_t key[64] = {};
    if (password.size() > sizeof(key)) {
        Sha256 s;
        s.Update((const uint8_t*)password.data(), password.size());
        s.Final(key);
    } else if (!password.empty()) {
        std::memcpy(key, password.data(), password.size());
    }
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; i++) { ipad[i] = key[i] ^ 0x36; opad[i] = key[i] ^ 0x5c; }
    // The hash states after the pads are reused for every HMAC call below.
    Sha256 inner, outer;
    inner.Update(ipad, 64);
    outer.Update(opad, 64);
    auto hmac = [&](const uint8_t* a, size_t aLen, const uint8_t* b, size_t bLen, uint8_t out[32]) {
        Sha256 i = inner, o = outer;
        i.Update(a, aLen);
        if (bLen) i.Update(b, bLen);
        uint8_t digest[32];
        i.Final(digest);
        o.Update(digest, 32);
        o.Final(out);
    };

    std::string out;
    for (uint32_t block = 1; (int)out.size() < outLen * 2; block++) {
        uint8_t counter[4] = {(uint8_t)(block >> 24), (uint8_t)(block >> 16), (uint8_t)(block >> 8), (uint8_t)block};
        uint8_t u[32], t[32];
        hmac((const uint8_t*)salt.data(), salt.size(), counter, 4, u);
        std::memcpy(t, u, 32);
        for (int i = 1; i < iterations; i++) {
            hmac(u, 32, nullptr, 0, u);
            for (int j = 0; j < 32; j++) t[j] ^= u[j];
        }
        out += ToHex(t, 32);
    }
    out.resize((size_t)outLen * 2);
    return out;
}

// ======================================================= recovery codes ====
static const char kCodeAlphabet[] = "ABCDEFGHJKMNPQRSTVWXYZ23456789";   // no 0 O 1 I L U

std::string NewRecoveryCode() {
    std::random_device rd;
    const int n = (int)sizeof(kCodeAlphabet) - 1;
    std::string code;
    for (int i = 0; i < 12; i++) {
        if (i && i % 4 == 0) code += '-';
        code += kCodeAlphabet[rd() % n];
    }
    return code;
}

std::string NormalizeRecoveryCode(const std::string& code) {
    std::string out;
    for (char c : code)
        if (std::isalnum((unsigned char)c)) out += (char)std::toupper((unsigned char)c);
    return out;
}

// =========================================================== validation ====
std::string ValidateUsername(const std::string& u) {
    if ((int)u.size() < kMinUsername || (int)u.size() > kMaxUsername)
        return "Username must be " + std::to_string(kMinUsername) + "-" + std::to_string(kMaxUsername) + " characters long";
    for (char c : u)
        if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
            return "Username can only use letters, digits, _ and -";
    return "";
}

std::string ValidatePassword(const std::string& p) {
    if ((int)p.size() < kMinPassword) return "Password must be at least " + std::to_string(kMinPassword) + " characters long";
    if ((int)p.size() > kMaxPassword) return "Password can be at most " + std::to_string(kMaxPassword) + " characters long";
    return "";
}

// ============================================================ user store ====
UserStore::UserStore(std::string rootDir, int iters) : root(std::move(rootDir)), iterations(iters) {
    if (!root.empty() && root.back() != '/' && root.back() != '\\') root += '/';
}

std::string UserStore::UserDir(const std::string& username) const {
    return root + "user_" + Lower(username) + "/";
}

// Read from disk every time, so two copies of the app stay in step.
std::vector<UserStore::Record> UserStore::ReadRecords() const {
    std::vector<Record> records;
    std::ifstream in(root + "users.txt");
    std::string line;
    for (size_t n = 0; std::getline(in, line); n++) {
        std::istringstream ss(line);
        std::string tag;
        Record r;
        r.line = n;
        if (ss >> tag >> r.username >> r.iterations >> r.salt >> r.hash && tag == "user" &&
            r.iterations > 0 && ValidateUsername(r.username).empty()) {
            if (!(ss >> r.codeSalt >> r.codeHash)) r.codeSalt.clear(), r.codeHash.clear();
            records.push_back(r);
        }
    }
    return records;
}

bool UserStore::FindRecord(const std::string& username, Record& out) const {
    for (const Record& r : ReadRecords())
        if (Lower(r.username) == Lower(username)) { out = r; return true; }
    return false;
}

std::string UserStore::Line(const Record& r) {
    std::string s = "user " + r.username + " " + std::to_string(r.iterations) + " " + r.salt + " " + r.hash;
    if (!r.codeHash.empty()) s += " " + r.codeSalt + " " + r.codeHash;
    return s;
}

bool UserStore::WriteRecord(const Record& r) const {
    std::vector<std::string> lines;
    {
        std::ifstream in(root + "users.txt");
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
    }
    if (r.line >= lines.size()) return false;
    lines[r.line] = Line(r);
    // Write a new file and swap it in, so a crash can't leave half a file.
    std::string tmp = root + "users.txt.tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        for (const std::string& l : lines) out << l << "\n";
        out.flush();
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmp, root + "users.txt", ec);
    return !ec;
}

AuthResult UserStore::Register(const std::string& username, const std::string& password) const {
    AuthResult res;
    res.error = ValidateUsername(username);
    if (res.error.empty()) res.error = ValidatePassword(password);
    if (!res.error.empty()) return res;

    std::vector<Record> records = ReadRecords();
    for (const Record& r : records)
        if (Lower(r.username) == Lower(username)) { res.error = "That username is already taken"; return res; }

    std::error_code ec;
    fs::create_directories(UserDir(username), ec);
    if (ec) { res.error = "Could not create your data folder: " + ec.message(); return res; }

    Record r;
    r.username = username;
    r.iterations = iterations;
    r.salt = RandomSalt();
    r.hash = Pbkdf2Hex(password, r.salt, iterations, 32);
    std::string code = NewRecoveryCode();
    r.codeSalt = RandomSalt();
    r.codeHash = Pbkdf2Hex(NormalizeRecoveryCode(code), r.codeSalt, iterations, 32);
    // Appending can't damage the accounts that are already in the file.
    std::ofstream out(root + "users.txt", std::ios::app);
    out << Line(r) << "\n";
    out.flush();
    if (!out) { res.error = "Could not save your account. Is this folder read-only?"; return res; }

    res.ok = true;
    res.username = username;
    res.firstAccount = records.empty();
    res.recoveryCode = code;
    return res;
}

AuthResult UserStore::Login(const std::string& username, const std::string& password) const {
    AuthResult res;
    for (const Record& r : ReadRecords()) {
        if (Lower(r.username) != Lower(username)) continue;
        if (SameText(Pbkdf2Hex(password, r.salt, r.iterations, 32), r.hash)) {
            res.ok = true;
            res.username = r.username;
            return res;
        }
        break;
    }
    res.error = "Incorrect username or password";
    return res;
}

AuthResult UserStore::ResetPassword(const std::string& username, const std::string& code,
                                    const std::string& newPassword) const {
    AuthResult res;
    res.error = ValidatePassword(newPassword);
    if (!res.error.empty()) return res;
    Record r;
    bool found = FindRecord(username, r);
    std::string given = NormalizeRecoveryCode(code);
    // The same message for a wrong name or a wrong code, as for logging in.
    if (!found || r.codeHash.empty() || given.empty() ||
        !SameText(Pbkdf2Hex(given, r.codeSalt, r.iterations, 32), r.codeHash)) {
        res.error = found && r.codeHash.empty() ? "This account has no recovery code yet"
                                                : "Incorrect username or recovery code";
        return res;
    }
    std::string newCode = NewRecoveryCode();
    r.iterations = iterations;
    r.salt = RandomSalt();
    r.hash = Pbkdf2Hex(newPassword, r.salt, iterations, 32);
    r.codeSalt = RandomSalt();
    r.codeHash = Pbkdf2Hex(NormalizeRecoveryCode(newCode), r.codeSalt, iterations, 32);
    if (!WriteRecord(r)) { res.error = "Could not save the new password. Is this folder read-only?"; return res; }
    res.ok = true;
    res.username = r.username;
    res.recoveryCode = newCode;
    return res;
}

bool UserStore::HasRecoveryCode(const std::string& username) const {
    Record r;
    return FindRecord(username, r) && !r.codeHash.empty();
}

AuthResult UserStore::CreateRecoveryCode(const std::string& username) const {
    AuthResult res;
    Record r;
    if (!FindRecord(username, r)) { res.error = "No such account"; return res; }
    std::string code = NewRecoveryCode();
    r.codeSalt = RandomSalt();
    r.codeHash = Pbkdf2Hex(NormalizeRecoveryCode(code), r.codeSalt, r.iterations, 32);
    if (!WriteRecord(r)) { res.error = "Could not save the recovery code. Is this folder read-only?"; return res; }
    res.ok = true;
    res.username = r.username;
    res.recoveryCode = code;
    return res;
}

void UserStore::AdoptLegacyData(const std::string& username, const std::string& legacyDir) const {
    std::string dir = UserDir(username);
    std::error_code ec;
    fs::create_directories(dir, ec);
    for (const char* name : {"segfault_progress.txt", "segfault_code"}) {
        fs::path from = fs::path(legacyDir) / name;
        if (fs::exists(from, ec)) fs::rename(from, fs::path(dir) / name, ec);
    }
}

} // namespace auth
