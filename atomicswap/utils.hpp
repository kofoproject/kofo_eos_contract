#include <eosiolib/crypto.h>
#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/time.hpp>

using namespace eosio;
using namespace std;

uint8_t from_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    eosio_assert(false, "Invalid hex character");
    return 0;
}

size_t from_hex(const string& hex_str, char* out_data, size_t out_data_len) {
    auto i = hex_str.begin();
    uint8_t* out_pos = (uint8_t*)out_data;
    uint8_t* out_end = out_pos + out_data_len;
    while (i != hex_str.end() && out_end != out_pos) {
        *out_pos = from_hex((char)(*i)) << 4;
        ++i;
        if (i != hex_str.end()) {
            *out_pos |= from_hex((char)(*i));
            ++i;
        }
        ++out_pos;
    }
    return out_pos - (uint8_t*)out_data;
}

string to_hex(const char* d, uint32_t s) {
    std::string r;
    const char* to_hex = "0123456789abcdef";
    uint8_t* c = (uint8_t*)d;
    for (uint32_t i = 0; i < s; ++i)
        (r += to_hex[(c[i] >> 4)]) += to_hex[(c[i] & 0x0f)];
    return r;
}

string sha256_to_hex(const capi_checksum256& sha256) {
    return to_hex((char*)sha256.hash, sizeof(sha256.hash));
}

uint64_t uint64_hash(const string& hash) {
    return std::hash<string>{}(hash);
}

uint64_t uint64_hash(const capi_checksum256& hash) {
    return uint64_hash(sha256_to_hex(hash));
}

capi_checksum256 hex_to_sha256(const string& hex_str) {
    eosio_assert(hex_str.length() == 64, "invalid sha256");
    capi_checksum256 checksum;
    from_hex(hex_str, (char*)checksum.hash, sizeof(checksum.hash));
    return checksum;
}

size_t sub2sep(const string& input,
               string* output,
               const char& separator,
               const size_t& first_pos = 0,
               const bool& required = false) {
    eosio_assert(first_pos != string::npos, "invalid first pos");
    auto pos = input.find(separator, first_pos);
    if (pos == string::npos) {
        eosio_assert(!required, "parse decimal string error");
        return string::npos;
    }
    *output = input.substr(first_pos, pos - first_pos);
    return pos;
}
