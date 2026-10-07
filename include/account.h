#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    ACCOUNT_STATUS_ACTIVE,
    ACCOUNT_STATUS_BLOCKED,
    ACCOUNT_STATUS_CLOSED,
    ACCOUNT_STATUS_UNKNOWN
} AccountStatus;

typedef struct {
    uint64_t account_id;
    uint64_t customer_id;
    char account_number[35];
    char account_type[16];
    char balance[32];               /* Kept as exact decimal string to avoid float errors */
    char daily_withdrawal_limit[32];/* Kept as exact decimal string */
    AccountStatus status;
} AccountRecord;

/**
 * Fetch fresh account record by account_id using a prepared statement.
 *
 * @param account_id ID of account to retrieve.
 * @param account Output struct to receive account details.
 * @return true if found, false on error or not found.
 */
bool account_get_by_id(uint64_t account_id, AccountRecord *account);

/**
 * Fetch fresh account record by account_number using a prepared statement.
 *
 * @param account_number Full account number string.
 * @param account Output struct to receive account details.
 * @return true if found, false on error or not found.
 */
bool account_get_by_number(const char *account_number, AccountRecord *account);

/**
 * Format raw currency decimal string (e.g. "45000.00") into Indian currency format (e.g. "Rs. 45,000.00").
 *
 * @param raw_amount Input numeric string.
 * @param formatted Output buffer.
 * @param formatted_size Buffer capacity.
 * @return true on success, false on buffer overflow.
 */
bool account_format_currency(const char *raw_amount, char *formatted, size_t formatted_size);

/**
 * Format account number with masking (e.g. "XXXX-XXXX-0001").
 *
 * @param account_number Full account number.
 * @param masked Output buffer of at least 20 bytes.
 * @param masked_size Buffer capacity.
 */
void account_mask_number(const char *account_number, char *masked, size_t masked_size);

#endif /* ACCOUNT_H */
