#ifndef CRYPTO_H
#define CRYPTO_H

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <string.h>

// A hardcoded 32-byte key for testing (AES-256)
#define AES_KEY (unsigned char *)"01234567890123456789012345678901"
#define IV_LEN 12
#define TAG_LEN 16

// Encrypts plaintext, wraps IV + Ciphertext + Tag, and outputs Base64
static inline int encrypt_msg(const char *plaintext, char *base64_out) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int len, ciphertext_len;
    unsigned char iv[IV_LEN];
    unsigned char tag[TAG_LEN];
    unsigned char binary_buf[2048]; // Holds IV + Ciphertext + Tag

    // Generating random IV
    RAND_bytes(iv, IV_LEN);
    memcpy(binary_buf, iv, IV_LEN); // Put IV at the start of binary_buf

    // Encrypting
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, AES_KEY, iv);
    EVP_EncryptUpdate(ctx, binary_buf + IV_LEN, &len, (unsigned char *)plaintext, strlen(plaintext));
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, binary_buf + IV_LEN + len, &len);
    ciphertext_len += len;
    
    // Get the authentication tag and append it to the end
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag);
    memcpy(binary_buf + IV_LEN + ciphertext_len, tag, TAG_LEN);
    EVP_CIPHER_CTX_free(ctx);

    // Base64 Encode the whole package so it can be sent as text
    int total_bin_len = IV_LEN + ciphertext_len + TAG_LEN;
    int b64_len = EVP_EncodeBlock((unsigned char *)base64_out, binary_buf, total_bin_len);
    base64_out[b64_len] = '\n'; // Add newline for socket reading
    base64_out[b64_len + 1] = '\0';
    
    return b64_len;
}

// Decodes Base64, extracts IV and Tag, and decrypts
static inline int decrypt_msg(const char *base64_in, char *plaintext_out) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    unsigned char binary_buf[2048];
    int len, plaintext_len;

    // Strip trailing newlines before decoding
    char temp_b64[2048];
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
