#pragma once

#include <cstddef>

namespace openssl_compat {
    /// Utility function to mimic OpenSSL's SHA1 function signature
    /// @param data Pointer to input data  
    /// @param size Size of input data in bytes
    /// @param output Pointer to output buffer (must be at least 20 bytes)
    void SHA1(const unsigned char* data, size_t size, unsigned char* output);
}