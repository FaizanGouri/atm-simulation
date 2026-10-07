#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MINI_STATEMENT_MAX_ITEMS 20

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

typedef struct {
    uint64_t transaction_id;
    char transaction_reference[36];
    TransactionType type;
    char amount[32];
    char balance_before[32];
    char balance_after[32];
    bool has_related_account;
    uint64_t related_account_id;
    char related_account_number[35];
    TransactionStatus status;
    char description[255];
    char created_at_formatted[32];
    bool is_credit;
} StatementItem;

typedef struct {
    StatementItem items[MINI_STATEMENT_MAX_ITEMS];
    size_t count;
} StatementList;

/**
 * Insert a transaction record using a prepared statement within the current connection/transaction.
 *
 * @param input Transaction record parameters.
 * @return true on success, false on SQL error.
 */
bool transaction_record_insert(const TransactionRecordInput *input);

/**
 * Retrieve recent transactions for an account using a prepared statement.
 * Transactions are ordered newest first (created_at DESC, transaction_id DESC).
 *
 * @param account_id Authenticated customer's account ID.
 * @param limit Maximum number of records to retrieve (1 to 20).
 * @param list Output structure to be populated.
 * @return true on success, false on database error.
 */
bool transaction_get_statement(uint64_t account_id, unsigned int limit, StatementList *list);

/**
 * Convert TransactionType enum to schema string.
 */
const char *transaction_type_to_string(TransactionType type);

/**
 * Convert TransactionStatus enum to schema string.
 */
const char *transaction_status_to_string(TransactionStatus status);

#endif /* TRANSACTION_H */
