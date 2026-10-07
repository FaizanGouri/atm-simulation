#include "deposit.h"
#include "database.h"
#include "transaction.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

const char *deposit_result_to_string(DepositResult result)
{
    switch (result) {
        case DEPOSIT_SUCCESS:
            return "Deposit successful";
        case DEPOSIT_ERR_INVALID_AMOUNT:
            return "Invalid deposit amount. Amount must be positive with up to 2 decimal places";
        case DEPOSIT_ERR_ACCOUNT_INACTIVE:
            return "Account is not active or has been blocked";
        case DEPOSIT_ERR_ACCOUNT_NOT_FOUND:
            return "Account not found";
        case DEPOSIT_ERR_TRANSACTION_FAILED:
            return "Transaction failed during database execution";
        case DEPOSIT_ERR_SYSTEM:
            return "System or connection error";
        default:
            return "Unknown error";
    }
}

DepositResult deposit_execute(uint64_t account_id,
                              uint64_t atm_id,
                              const char *amount_str,
                              DepositReceipt *receipt)
{
    if (!amount_str) {
        return DEPOSIT_ERR_INVALID_AMOUNT;
    }

    int64_t deposit_paise = 0;
    if (!utils_parse_amount_to_paise(amount_str, &deposit_paise) || deposit_paise <= 0) {
        return DEPOSIT_ERR_INVALID_AMOUNT;
    }

    MYSQL *conn = db_get_connection();
    if (!conn) {
        return DEPOSIT_ERR_SYSTEM;
    }

    /* Start ACID Transaction */
    if (!db_transaction_begin()) {
        return DEPOSIT_ERR_SYSTEM;
    }

    /* 1. Lock Account and fetch current balance and status */
    const char *lock_query =
        "SELECT CAST(balance AS CHAR), status FROM accounts WHERE account_id = ? FOR UPDATE";
    MYSQL_STMT *lock_stmt = mysql_stmt_init(conn);
    if (!lock_stmt) {
        db_transaction_rollback();
        return DEPOSIT_ERR_SYSTEM;
    }

    if (mysql_stmt_prepare(lock_stmt, lock_query, (unsigned long)strlen(lock_query)) != 0) {
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return DEPOSIT_ERR_SYSTEM;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long aid = account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid;

    if (mysql_stmt_bind_param(lock_stmt, b_in) != 0 || mysql_stmt_execute(lock_stmt) != 0) {
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return DEPOSIT_ERR_SYSTEM;
    }

    char cur_bal_str[32] = {0};
    char status_str[20] = {0};
    unsigned long l_bal = 0, l_stat = 0;

    MYSQL_BIND b_out[2];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = cur_bal_str;
    b_out[0].buffer_length = sizeof(cur_bal_str);
    b_out[0].length = &l_bal;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = status_str;
    b_out[1].buffer_length = sizeof(status_str);
    b_out[1].length = &l_stat;

    if (mysql_stmt_bind_result(lock_stmt, b_out) != 0 || mysql_stmt_store_result(lock_stmt) != 0) {
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return DEPOSIT_ERR_SYSTEM;
    }

    if (mysql_stmt_fetch(lock_stmt) != 0) {
        /* Account row not found */
        mysql_stmt_free_result(lock_stmt);
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return DEPOSIT_ERR_ACCOUNT_NOT_FOUND;
    }

    mysql_stmt_free_result(lock_stmt);
    mysql_stmt_close(lock_stmt);

    if (strcmp(status_str, "ACTIVE") != 0) {
        db_transaction_rollback();
        return DEPOSIT_ERR_ACCOUNT_INACTIVE;
    }

    /* 2. Compute new balance in exact integer paise */
    int64_t cur_bal_paise = 0;
    if (!utils_parse_balance_to_paise(cur_bal_str, &cur_bal_paise)) {
        db_transaction_rollback();
        return DEPOSIT_ERR_TRANSACTION_FAILED;
    }

    int64_t new_bal_paise = cur_bal_paise + deposit_paise;

    char new_bal_str[32];
    utils_paise_to_decimal_str(new_bal_paise, new_bal_str, sizeof(new_bal_str));

    char dep_amt_str[32];
    utils_paise_to_decimal_str(deposit_paise, dep_amt_str, sizeof(dep_amt_str));

    /* 3. Update account balance */
    const char *update_query = "UPDATE accounts SET balance = ? WHERE account_id = ?";
    MYSQL_STMT *upd_stmt = mysql_stmt_init(conn);
    if (!upd_stmt) {
        db_transaction_rollback();
        return DEPOSIT_ERR_TRANSACTION_FAILED;
    }

    if (mysql_stmt_prepare(upd_stmt, update_query, (unsigned long)strlen(update_query)) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return DEPOSIT_ERR_TRANSACTION_FAILED;
    }

    MYSQL_BIND b_upd[2];
    memset(b_upd, 0, sizeof(b_upd));

    unsigned long l_nbal = (unsigned long)strlen(new_bal_str);
    b_upd[0].buffer_type = MYSQL_TYPE_STRING;
    b_upd[0].buffer = new_bal_str;
    b_upd[0].length = &l_nbal;

    b_upd[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_upd[1].buffer = &aid;

    if (mysql_stmt_bind_param(upd_stmt, b_upd) != 0 || mysql_stmt_execute(upd_stmt) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return DEPOSIT_ERR_TRANSACTION_FAILED;
    }
    mysql_stmt_close(upd_stmt);

    /* 4. Generate transaction reference and record transaction */
    char txn_ref[36];
    utils_generate_txn_reference(txn_ref, sizeof(txn_ref));

    uint64_t aid_val = atm_id;
    TransactionRecordInput txn_in;
    memset(&txn_in, 0, sizeof(txn_in));
    strncpy(txn_in.transaction_reference, txn_ref, sizeof(txn_in.transaction_reference) - 1);
    txn_in.account_id = account_id;
    txn_in.type = TXN_TYPE_DEPOSIT;
    strncpy(txn_in.amount, dep_amt_str, sizeof(txn_in.amount) - 1);
    strncpy(txn_in.balance_before, cur_bal_str, sizeof(txn_in.balance_before) - 1);
    strncpy(txn_in.balance_after, new_bal_str, sizeof(txn_in.balance_after) - 1);
    txn_in.related_account_id = NULL;
    txn_in.atm_id = (atm_id > 0) ? &aid_val : NULL;
    txn_in.status = TXN_STATUS_SUCCESS;
    txn_in.description = "ATM Cash Deposit";

    if (!transaction_record_insert(&txn_in)) {
        db_transaction_rollback();
        return DEPOSIT_ERR_TRANSACTION_FAILED;
    }

    /* 5. Commit transaction */
    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return DEPOSIT_ERR_TRANSACTION_FAILED;
    }

    /* Populate receipt if requested */
    if (receipt) {
        strncpy(receipt->transaction_reference, txn_ref, sizeof(receipt->transaction_reference) - 1);
        receipt->transaction_reference[sizeof(receipt->transaction_reference) - 1] = '\0';

        strncpy(receipt->deposited_amount, dep_amt_str, sizeof(receipt->deposited_amount) - 1);
        receipt->deposited_amount[sizeof(receipt->deposited_amount) - 1] = '\0';

        strncpy(receipt->previous_balance, cur_bal_str, sizeof(receipt->previous_balance) - 1);
        receipt->previous_balance[sizeof(receipt->previous_balance) - 1] = '\0';

        strncpy(receipt->new_balance, new_bal_str, sizeof(receipt->new_balance) - 1);
        receipt->new_balance[sizeof(receipt->new_balance) - 1] = '\0';
    }

    return DEPOSIT_SUCCESS;
}
