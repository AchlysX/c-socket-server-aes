#ifndef CRYPTO_H
#define CRYPTO_H

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <string.h>

// A hardcoded 32-byte key for testing (AES-256)
#define AES_KEY (unsigned char *)"01234567890123456789012345678901"
#define IV_LEN 12
#define TAG_LEN 16

// Largest plaintext a single message may carry. Every buffer below is sized
// from this, so callers can't trigger an overflow by sending a long message.
#define MAX_PLAINTEXT 1024
#define MAX_BINARY (IV_LEN + MAX_PLAINTEXT + TAG_LEN)
// Base64 of MAX_BINARY, plus the '\n' delimiter and the '\0' terminator.
#define MAX_B64 (((MAX_BINARY + 2) / 3) * 4 + 2)

// Encrypts plaintext, wraps IV + Ciphertext + Tag, and outputs Base64.
// out_cap is the size of base64_out; the function refuses to write past it.
// Returns the Base64 length on success, or -1 on error (including a plaintext
// longer than MAX_PLAINTEXT or an output buffer that is too small).
static inline int encrypt_msg(const char *plaintext, char *base64_out, size_t out_cap) {
    size_t pt_len = strlen(plaintext);
    if (pt_len > MAX_PLAINTEXT) return -1;

    // Bound-check the output before touching it: EVP_EncodeBlock has no
    // length argument, so the buffer must be known-large enough first.
    int total_bin_len = IV_LEN + (int)pt_len + TAG_LEN;
    size_t b64_needed = (size_t)(((total_bin_len + 2) / 3) * 4) + 2; // + '\n' + '\0'
    if (b64_needed > out_cap) return -1;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int len, ciphertext_len;
    unsigned char iv[IV_LEN];
    unsigned char tag[TAG_LEN];
    unsigned char binary_buf[MAX_BINARY]; // Holds IV + Ciphertext + Tag

    // Generating random IV
    if (RAND_bytes(iv, IV_LEN) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    memcpy(binary_buf, iv, IV_LEN); // Put IV at the start of binary_buf

    // Encrypting
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, AES_KEY, iv);
    EVP_EncryptUpdate(ctx, binary_buf + IV_LEN, &len, (unsigned char *)plaintext, (int)pt_len);
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, binary_buf + IV_LEN + len, &len);
    ciphertext_len += len;

    // Get the authentication tag and append it to the end
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag);
    memcpy(binary_buf + IV_LEN + ciphertext_len, tag, TAG_LEN);
    EVP_CIPHER_CTX_free(ctx);

    // Base64 Encode the whole package so it can be sent as text
    total_bin_len = IV_LEN + ciphertext_len + TAG_LEN;
    int b64_len = EVP_EncodeBlock((unsigned char *)base64_out, binary_buf, total_bin_len);
    base64_out[b64_len] = '\n'; // Add newline for socket reading
    base64_out[b64_len + 1] = '\0';

    return b64_len;
}

// Decodes Base64, extracts IV and Tag, and decrypts.
// out_cap is the size of plaintext_out; the function refuses to write past it.
// Returns 1 on success, or -1 on error (bad input, output too small, or a
// failed authentication tag, i.e. wrong key or tampered data).
static inline int decrypt_msg(const char *base64_in, char *plaintext_out, size_t out_cap) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    unsigned char binary_buf[MAX_B64];
    int len, plaintext_len;

    // Strip trailing newlines before decoding. An over-long input is truncated
    // here rather than overflowing; it then fails to authenticate and is rejected.
    char temp_b64[MAX_B64];
    strncpy(temp_b64, base64_in, sizeof(temp_b64) - 1);
    temp_b64[sizeof(temp_b64) - 1] = '\0';
    temp_b64[strcspn(temp_b64, "\r\n")] = '\0';

    size_t in_len = strlen(temp_b64);
    if (in_len == 0) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    int bin_len = EVP_DecodeBlock(binary_buf, (unsigned char *)temp_b64, in_len);
    if (bin_len < 0) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    // Adjust for Base64 padding '='
    while (in_len > 0 && temp_b64[in_len - 1] == '=') {
        bin_len--;
        in_len--;
    }

    int ciphertext_len = bin_len - IV_LEN - TAG_LEN;
    if (ciphertext_len < 0) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    // Make sure the recovered plaintext (GCM: same length as ciphertext) plus a
    // '\0' terminator fits in the caller's buffer before writing anything.
    if ((size_t)ciphertext_len + 1 > out_cap) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    unsigned char *iv = binary_buf;
    unsigned char *ciphertext = binary_buf + IV_LEN;
    unsigned char *tag = binary_buf + IV_LEN + ciphertext_len;

    // Decrypting
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, AES_KEY, iv);
    EVP_DecryptUpdate(ctx, (unsigned char *)plaintext_out, &len, ciphertext, ciphertext_len);
    plaintext_len = len;

    // Tag verification
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag);
    int ret = EVP_DecryptFinal_ex(ctx, (unsigned char *)plaintext_out + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret > 0) {
        plaintext_len += len;
        plaintext_out[plaintext_len] = '\0'; // Null terminate the string
        return 1; // Success
    }
    return -1; // Authentication failed (wrong key or tampered data)
}

#endif
