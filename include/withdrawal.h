#ifndef WITHDRAWAL_H
#define WITHDRAWAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "atm.h"

typedef enum {
    WITHDRAWAL_SUCCESS = 0,
    WITHDRAWAL_ERR_INVALID_AMOUNT,          /* Amount not positive or not a multiple of 50 */
    WITHDRAWAL_ERR_ACCOUNT_INACTIVE,        /* Account is not active */
    WITHDRAWAL_ERR_ACCOUNT_NOT_FOUND,       /* Account record missing */
    WITHDRAWAL_ERR_INSUFFICIENT_FUNDS,      /* Account balance < requested amount */
    WITHDRAWAL_ERR_DAILY_LIMIT_EXCEEDED,    /* Exceeds daily limit for today */
    WITHDRAWAL_ERR_ATM_CASH_UNAVAILABLE,    /* ATM cannot dispense required notes/denominations */
    WITHDRAWAL_ERR_TRANSACTION_FAILED,      /* SQL/commit failure */
    WITHDRAWAL_ERR_SYSTEM                   /* Internal/connection error */
} WithdrawalResult;

typedef struct {
    char transaction_reference[36];
    char withdrawn_amount[32];
    char previous_balance[32];
    char new_balance[32];
    DenominationBreakdown dispensed_notes;
} WithdrawalReceipt;

/**
 * Perform an atomic cash withdrawal from an account.
 *
 * @param account_id ID of account to withdraw from.
 * @param atm_id ATM ID (defaults to 1).
 * @param amount_str Withdrawal amount in rupees (e.g. "2000" or "500").
 * @param receipt Output structure populated with receipt details on success.
 * @return WithdrawalResult code.
 */
WithdrawalResult withdrawal_execute(uint64_t account_id,
                                    uint64_t atm_id,
                                    const char *amount_str,
                                    WithdrawalReceipt *receipt);

/**
 * Get human-readable description for WithdrawalResult code.
 */
const char *withdrawal_result_to_string(WithdrawalResult result);

/**
 * Query total successful withdrawals performed by this account today.
 *
 * @param account_id ID of account.
 * @param out_today_withdrawn_paise Output receiving total withdrawn today in paise.
 * @return true on success, false on error.
 */
bool withdrawal_get_today_total(uint64_t account_id, int64_t *out_today_withdrawn_paise);

#endif /* WITHDRAWAL_H */
