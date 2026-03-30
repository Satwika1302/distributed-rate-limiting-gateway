// JWT Authentication and Authorization Middleware
// Handles bearer token extraction and HMAC-SHA256 signature verification.
// Uses jwt-cpp and OpenSSL for cryptographic operations.

#include "JwtAuth.hpp"
#include <jwt-cpp/jwt.h>
#include <iostream>

JwtAuth::JwtAuth(const std::string& secret) : secret_key(secret) {}

std::string JwtAuth::extract_token(const std::string& auth_header) {
    // Check if the header starts with "Bearer " (7 characters)
    if (auth_header.length() > 7 && auth_header.substr(0, 7) == "Bearer ") {
        return auth_header.substr(7);
    }
    return "";
}

bool JwtAuth::validate_token(const std::string& token) const {
    if (token.empty()) {
        return false;
    }

    try {
        // Decode and verify the token simultaneously
        auto decoded = jwt::decode(token);
        
        // Define the verification rules
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_key});
            
        verifier.verify(decoded);
        
        // If it survives to here, the token is perfectly valid
        return true;
    } catch (const jwt::error::signature_verification_exception& e) {
        std::cerr << "JWT Signature Failed: " << e.what() << "\n";
        return false;
    } catch (const jwt::error::token_verification_exception& e) {
        std::cerr << "JWT Verification Failed (e.g. Expired): " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "JWT Decoding Error: " << e.what() << "\n";
        return false;
    }
}
