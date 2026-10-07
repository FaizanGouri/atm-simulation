#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Generate a unique transaction reference (e.g., TXN20261007142301987).
 *
 * @param buffer Output buffer of at least 32 bytes.
 * @param buffer_size Capacity of buffer.
 */
void utils_generate_txn_reference(char *buffer, size_t buffer_size);

/**
 * Parse an amount string into integer paisa/cents (1 rupee = 100 paise).
 * Returns true if valid positive amount, false on invalid format or negative.
 */
bool utils_parse_amount_to_paise(const char *amount_str, int64_t *out_paise);

/**
 * Safely parse a balance string into exact integer paise (1 rupee = 100 paise).
 * Accepts valid zero balances ("0", "0.0", "0.00") and positive values.
 * Rejects negative values, malformed characters, and values exceeding int64_t.
 *
 * @param balance_str Decimal string representing balance.
 * @param out_paise Output integer paise.
 * @return true on valid parse, false on malformed input.
 */
bool utils_parse_balance_to_paise(const char *balance_str, int64_t *out_paise);

/**
 * Convert integer paise back into formatted decimal string "XXXX.XX".
 */
void utils_paise_to_decimal_str(int64_t paise, char *buffer, size_t buffer_size);

#endif /* UTILS_H */
