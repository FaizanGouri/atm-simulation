#ifndef SECURITY_H
#define SECURITY_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Compute standard SHA-256 hash of an input string and output a 64-char hex string.
 *
 * @param input Null-terminated input string (e.g., entered PIN or password).
 * @param output Buffer of at least 65 bytes (64 hex characters + null terminator).
 * @param output_size Size of the output buffer.
 * @return true on success, false on buffer overflow or null pointer.
 */
bool security_hash_sha256(const char *input, char *output, size_t output_size);

/**
 * Securely compare two hashes in constant time to mitigate timing attacks.
 *
 * @param a First hash string.
 * @param b Second hash string.
 * @return true if both hashes match, false otherwise.
 */
bool security_constant_time_compare(const char *a, const char *b);

/**
 * Masked terminal input for sensitive values (PIN/password).
 * Echoes '*' for each character on Windows/CLI terminals where supported.
 *
 * @param prompt Prompt message to display.
 * @param buffer Output buffer to receive entered string.
 * @param max_len Maximum characters to read.
 * @return true if input was read, false on cancellation/empty.
 */
bool security_read_masked_input(const char *prompt, char *buffer, size_t max_len);

/**
 * Securely wipe a memory buffer using a compiler-resistant technique.
 * Prevents dead-store elimination from optimizing away memory clearing.
 *
 * @param ptr Pointer to memory buffer to wipe.
 * @param len Size of buffer in bytes.
 */
void security_secure_zero(void *ptr, size_t len);

#endif /* SECURITY_H */
