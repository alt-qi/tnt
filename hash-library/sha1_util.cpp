#include "sha1_util.h"
#include "sha1.h"

namespace openssl_compat {
    void SHA1(const unsigned char* data, size_t size, unsigned char* output) {
        ::SHA1 sha1_hasher;
        sha1_hasher.add(data, size);
        sha1_hasher.getHash(output);
    }
}