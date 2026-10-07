#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "beneficiary.h"

typedef enum {
    TRANSFER_SUCCESS = 0,
    TRANSFER_ERR_INVALID_AMOUNT,            /* Negative, zero, or malformed */
    TRANSFER_ERR_SOURCE_INACTIVE,           /* Source account not active */
    TRANSFER_ERR_DEST_INACTIVE,             /* Destination account not active */
    TRANSFER_ERR_INSUFFICIENT_FUNDS,        /* Source balance < transfer amount */
    TRANSFER_ERR_SELF_TRANSFER,             /* Cannot transfer to own account */
    TRANSFER_ERR_BENEFICIARY_INVALID,       /* Beneficiary not found or unauthorized */
    TRANSFER_ERR_ACCOUNT_NOT_FOUND,         /* Source or dest account not found */
    TRANSFER_ERR_TRANSACTION_FAILED,        /* Database commit/execution error */
    TRANSFER_ERR_SYSTEM
} TransferResult;

typedef struct {
    char transaction_reference[36];
    char beneficiary_name[100];
    char dest_account_masked[32];
    char transferred_amount[32];
    char source_prev_balance[32];
    char source_new_balance[32];
} TransferReceipt;

/**
 * Execute an atomic fund transfer between the source account and a selected beneficiary.
 *
 * @param source_account_id ID of authenticated source account.
 * @param beneficiary_id ID of beneficiary row.
 * @param amount_str Decimal amount string (e.g. "1500.00").
 * @param receipt Output structure populated with receipt details on success.
 * @return TransferResult code.
 */
TransferResult transfer_execute(uint64_t source_account_id,
                                uint64_t beneficiary_id,
                                const char *amount_str,
                                TransferReceipt *receipt);

/**
 * Convert TransferResult enum to human-readable error description.
 */
const char *transfer_result_to_string(TransferResult result);

#endif /* TRANSFER_H */
