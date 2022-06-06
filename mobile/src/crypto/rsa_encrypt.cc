//netboxglobal begin

#include "base/logging.h"
#include "crypto/rsa_encrypt.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

#define R(func, error_message) \
{\
    if ((func) <= 0)\
    {\
        VLOG(1) << error_message;\
        return false;\
    }\
}

namespace crypto {

bool RSAEncrypt(base::span<const uint8_t> public_key_info,
                base::span<const uint8_t> orig_data,
                std::vector<uint8_t> &enc_data) {
                    
    OpenSSLErrStackTracer err_tracer(FROM_HERE);

    CBS cbs;
    CBS_init(&cbs, public_key_info.data(), public_key_info.size());

    bssl::UniquePtr<EVP_PKEY> public_key(EVP_parse_public_key(&cbs));

    if (!public_key || CBS_len(&cbs) != 0){
        VLOG(1) << "cant load RSA public key";
        return false;
    }

    bssl::UniquePtr<EVP_PKEY_CTX> ctx(EVP_PKEY_CTX_new(public_key.get(), nullptr));

   	R(EVP_PKEY_encrypt_init(ctx.get()), "cant initialize public key encrypt context");
    R(EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING), "cant set PKCS1 padding");
    
    size_t outlen = 0;

    R(EVP_PKEY_encrypt(ctx.get(),
                       nullptr,
                       &outlen,
                       orig_data.data(),
                       orig_data.size()), "cant calculate RSA encrypted len");

    enc_data.resize(outlen);

    R(EVP_PKEY_encrypt(ctx.get(),
                       enc_data.data(),
                       &outlen,
                       orig_data.data(),
                       orig_data.size()), "cant RSA encrypt");
    return true;
}

static bool encrypt_block(const unsigned char *data, size_t data_len, EVP_PKEY *pb_key, std::vector<std::vector<unsigned char>> &blocks)
{
    bssl::UniquePtr<EVP_PKEY_CTX> ctx(EVP_PKEY_CTX_new(pb_key, nullptr));

   	R(EVP_PKEY_encrypt_init(ctx.get()), "cant initialize public key encrypt context");
    R(EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING), "cant set PKCS1 padding");

    size_t outlen = 0;

    R(EVP_PKEY_encrypt(ctx.get(),
                       nullptr,
                       &outlen,
                       data,
                       data_len), "cant calculate encrypted len");

    std::vector<unsigned char> enc_data(outlen);

    R(EVP_PKEY_encrypt(ctx.get(),
                       enc_data.data(),
                       &outlen,
                       data,
                       data_len), "cant encrypt");

    blocks.push_back(std::move(enc_data));

    return 1;
}

bool RSAEncrypt(base::span<const uint8_t> public_key_info,
                base::span<const uint8_t> orig_data,
                std::vector<std::vector<uint8_t>> &enc_data) {

    OpenSSLErrStackTracer err_tracer(FROM_HERE);

    CBS cbs;
    CBS_init(&cbs, public_key_info.data(), public_key_info.size());

    bssl::UniquePtr<EVP_PKEY> pb_key(EVP_parse_public_key(&cbs));

    bssl::UniquePtr<RSA> rsa_key(EVP_PKEY_get1_RSA(pb_key.get()));
    int key_length = RSA_size(rsa_key.get());

    const int PKCS1_PADDING_LEN = 11;
    int msg_len = key_length - PKCS1_PADDING_LEN;

    int msg_cnt = orig_data.size() / msg_len;
    int padding = orig_data.size() % msg_len;

    const uint8_t *ptr = orig_data.data();

    for (int i = 0; i < msg_cnt; i++){

        R(encrypt_block(ptr, msg_len, pb_key.get(), enc_data), "cant encrypt block");

        ptr += msg_len;
    }

    if (padding != 0)
        R(encrypt_block(ptr, padding, pb_key.get(), enc_data), "cant encrypt block");

    return true;
}

}

//netboxglobal end