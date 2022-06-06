//netboxglobal begin

#ifndef CRYPTO_RSA_ENCRYPT_H_
#define CRYPTO_RSA_ENCRYPT_H_

#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

bool CRYPTO_EXPORT RSAEncrypt(base::span<const uint8_t> public_key_info,
                              base::span<const uint8_t> orig_data,
                              std::vector<uint8_t> &enc_data);

bool CRYPTO_EXPORT RSAEncrypt(base::span<const uint8_t> public_key_info,
                              base::span<const uint8_t> orig_data,
                              std::vector<std::vector<uint8_t>> &enc_data);
}

#endif

//netboxglobal end