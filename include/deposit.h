#ifndef DEPOSIT_H
#define DEPOSIT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    DEPOSIT_SUCCESS = 0,
    DEPOSIT_ERR_INVALID_AMOUNT,
    DEPOSIT_ERR_ACCOUNT_INACTIVE,
    DEPOSIT_ERR_ACCOUNT_NOT_FOUND,
    DEPOSIT_ERR_TRANSACTION_FAILED,
    DEPOSIT_ERR_SYSTEM
} DepositResult;

typedef struct {
    char transaction_reference[36];
    char deposited_amount[32];
    char previous_balance[32];
    char new_balance[32];
} DepositReceipt;

/**
 * Perform an atomic cash deposit into an account.
 *
 * @param account_id ID of account to deposit into.
 * @param atm_id ATM ID (e.g. 1).
 * @param amount_str Positive decimal amount string (e.g. "5000.00" or "5000").
 * @param receipt Output structure populated with receipt details on success.
 * @return DepositResult code.
 */
DepositResult deposit_execute(uint64_t account_id,
                              uint64_t atm_id,
                              const char *amount_str,
                              DepositReceipt *receipt);

/**
 * Get human-readable description for DepositResult code.
 */
const char *deposit_result_to_string(DepositResult result);

#endif /* DEPOSIT_H */
