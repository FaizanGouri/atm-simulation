#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TXN_TYPE_WITHDRAWAL,
    TXN_TYPE_DEPOSIT,
    TXN_TYPE_TRANSFER,
    TXN_TYPE_BALANCE_ENQUIRY,
    TXN_TYPE_PIN_CHANGE
} TransactionType;

typedef enum {
    TXN_STATUS_SUCCESS,
    TXN_STATUS_FAILED,
    TXN_STATUS_REVERSED
} TransactionStatus;

typedef struct {
    char transaction_reference[36];
    uint64_t account_id;
    TransactionType type;
    char amount[32];
    char balance_before[32];
    char balance_after[32];
    uint64_t *related_account_id; /* NULL if none */
    uint64_t *atm_id;             /* NULL if none */
    TransactionStatus status;
    const char *description;
} TransactionRecordInput;

/**
 * Insert a transaction record using a prepared statement within the current connection/transaction.
 *
 * @param input Transaction record parameters.
 * @return true on success, false on SQL error.
 */
bool transaction_record_insert(const TransactionRecordInput *input);

/**
 * Convert TransactionType enum to schema string.
 */
const char *transaction_type_to_string(TransactionType type);

#endif /* TRANSACTION_H */
