#pragma once
#include <string>

// A simple JWT validator class specifically for our gateway
class JwtAuth {
private:
    std::string secret_key;

public:
    // Initialize with the symmetric HS256 secret key
    explicit JwtAuth(const std::string& secret);

    // Extracts the Bearer token safely from an HTTP Authorization header
    static std::string extract_token(const std::string& auth_header);

    // Verifies the token algorithm and signature
    bool validate_token(const std::string& token) const;
};
