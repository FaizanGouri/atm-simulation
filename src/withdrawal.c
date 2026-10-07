#include "withdrawal.h"
#include "database.h"
#include "transaction.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

const char *withdrawal_result_to_string(WithdrawalResult result)
{
    switch (result) {
        case WITHDRAWAL_SUCCESS:
            return "Withdrawal successful";
        case WITHDRAWAL_ERR_INVALID_AMOUNT:
            return "Invalid withdrawal amount. Amount must be a positive multiple of Rs. 50 (e.g. 500, 1000)";
        case WITHDRAWAL_ERR_ACCOUNT_INACTIVE:
            return "Account is not active or has been blocked";
        case WITHDRAWAL_ERR_ACCOUNT_NOT_FOUND:
            return "Account not found";
        case WITHDRAWAL_ERR_INSUFFICIENT_FUNDS:
            return "Insufficient funds in your account";
        case WITHDRAWAL_ERR_DAILY_LIMIT_EXCEEDED:
            return "Daily withdrawal limit exceeded for this account";
        case WITHDRAWAL_ERR_ATM_CASH_UNAVAILABLE:
            return "ATM cannot dispense the requested amount with currently available cash denominations";
        case WITHDRAWAL_ERR_TRANSACTION_FAILED:
            return "Transaction failed during database execution";
        case WITHDRAWAL_ERR_SYSTEM:
            return "System or connection error";
        default:
            return "Unknown error";
    }
}

bool withdrawal_get_today_total(uint64_t account_id, int64_t *out_today_withdrawn_paise)
{
    if (!out_today_withdrawn_paise) return false;
    *out_today_withdrawn_paise = 0;

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "SELECT CAST(COALESCE(SUM(amount), 0.00) AS CHAR) "
        "FROM transactions "
        "WHERE account_id = ? "
        "  AND transaction_type = 'WITHDRAWAL' "
        "  AND transaction_status = 'SUCCESS' "
        "  AND DATE(created_at) = CURRENT_DATE";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long aid = account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    char sum_str[32] = {0};
    unsigned long l_sum = 0;
    MYSQL_BIND b_out[1];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = sum_str;
    b_out[0].buffer_length = sizeof(sum_str);
    b_out[0].length = &l_sum;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt) == 0 && l_sum > 0) {
        int64_t paise = 0;
        if (utils_parse_amount_to_paise(sum_str, &paise)) {
            *out_today_withdrawn_paise = paise;
        }
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

WithdrawalResult withdrawal_execute(uint64_t account_id,
                                    uint64_t atm_id,
                                    const char *amount_str,
                                    WithdrawalReceipt *receipt)
{
    if (!amount_str) {
        return WITHDRAWAL_ERR_INVALID_AMOUNT;
    }

    int64_t withdraw_paise = 0;
    if (!utils_parse_amount_to_paise(amount_str, &withdraw_paise) || withdraw_paise <= 0) {
        return WITHDRAWAL_ERR_INVALID_AMOUNT;
    }

    /* Withdrawal must be whole rupees and a multiple of 50 */
    if ((withdraw_paise % 100LL) != 0 || ((withdraw_paise / 100LL) % 50LL) != 0) {
        return WITHDRAWAL_ERR_INVALID_AMOUNT;
    }

    uint64_t amount_rupees = (uint64_t)(withdraw_paise / 100LL);

    MYSQL *conn = db_get_connection();
    if (!conn) {
        return WITHDRAWAL_ERR_SYSTEM;
    }

    /* Start ACID Transaction */
    if (!db_transaction_begin()) {
        return WITHDRAWAL_ERR_SYSTEM;
    }

    /* 1. Lock account row and fetch balance, status, and daily limit */
    const char *lock_query =
        "SELECT CAST(balance AS CHAR), status, CAST(daily_withdrawal_limit AS CHAR) "
        "FROM accounts WHERE account_id = ? FOR UPDATE";

    MYSQL_STMT *lock_stmt = mysql_stmt_init(conn);
    if (!lock_stmt) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_SYSTEM;
    }

    if (mysql_stmt_prepare(lock_stmt, lock_query, (unsigned long)strlen(lock_query)) != 0) {
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return WITHDRAWAL_ERR_SYSTEM;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long aid = account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid;

    if (mysql_stmt_bind_param(lock_stmt, b_in) != 0 || mysql_stmt_execute(lock_stmt) != 0) {
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return WITHDRAWAL_ERR_SYSTEM;
    }

    char cur_bal_str[32] = {0};
    char status_str[20] = {0};
    char daily_limit_str[32] = {0};
    unsigned long l_bal = 0, l_stat = 0, l_lim = 0;

    MYSQL_BIND b_out[3];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = cur_bal_str;
    b_out[0].buffer_length = sizeof(cur_bal_str);
    b_out[0].length = &l_bal;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = status_str;
    b_out[1].buffer_length = sizeof(status_str);
    b_out[1].length = &l_stat;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = daily_limit_str;
    b_out[2].buffer_length = sizeof(daily_limit_str);
    b_out[2].length = &l_lim;

    if (mysql_stmt_bind_result(lock_stmt, b_out) != 0 || mysql_stmt_store_result(lock_stmt) != 0) {
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return WITHDRAWAL_ERR_SYSTEM;
    }

    if (mysql_stmt_fetch(lock_stmt) != 0) {
        mysql_stmt_free_result(lock_stmt);
        mysql_stmt_close(lock_stmt);
        db_transaction_rollback();
        return WITHDRAWAL_ERR_ACCOUNT_NOT_FOUND;
    }

    mysql_stmt_free_result(lock_stmt);
    mysql_stmt_close(lock_stmt);

    if (strcmp(status_str, "ACTIVE") != 0) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_ACCOUNT_INACTIVE;
    }

    /* 2. Check account balance */
    int64_t cur_bal_paise = 0;
    if (!utils_parse_amount_to_paise(cur_bal_str, &cur_bal_paise)) {
        cur_bal_paise = 0;
    }

    if (cur_bal_paise < withdraw_paise) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_INSUFFICIENT_FUNDS;
    }

    /* 3. Check daily withdrawal limit */
    int64_t daily_limit_paise = 0;
    if (!utils_parse_amount_to_paise(daily_limit_str, &daily_limit_paise)) {
        daily_limit_paise = 0;
    }

    int64_t today_withdrawn_paise = 0;
    withdrawal_get_today_total(account_id, &today_withdrawn_paise);

    if (today_withdrawn_paise + withdraw_paise > daily_limit_paise) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_DAILY_LIMIT_EXCEEDED;
    }

    /* 4. Check ATM inventory and calculate denomination breakdown */
    DenominationBreakdown available_notes;
    if (!atm_get_cash_inventory(atm_id, &available_notes)) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_SYSTEM;
    }

    DenominationBreakdown dispensed_notes;
    if (!atm_calculate_denominations(amount_rupees, &available_notes, &dispensed_notes)) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_ATM_CASH_UNAVAILABLE;
    }

    /* 5. Deduct customer account balance */
    int64_t new_bal_paise = cur_bal_paise - withdraw_paise;
    char new_bal_str[32];
    utils_paise_to_decimal_str(new_bal_paise, new_bal_str, sizeof(new_bal_str));

    char wth_amt_str[32];
    utils_paise_to_decimal_str(withdraw_paise, wth_amt_str, sizeof(wth_amt_str));

    const char *upd_query = "UPDATE accounts SET balance = ? WHERE account_id = ?";
    MYSQL_STMT *upd_stmt = mysql_stmt_init(conn);
    if (!upd_stmt) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_TRANSACTION_FAILED;
    }

    if (mysql_stmt_prepare(upd_stmt, upd_query, (unsigned long)strlen(upd_query)) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return WITHDRAWAL_ERR_TRANSACTION_FAILED;
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
        return WITHDRAWAL_ERR_TRANSACTION_FAILED;
    }
    mysql_stmt_close(upd_stmt);

    /* 6. Deduct cash inventory from ATM */
    if (!atm_deduct_cash_inventory(atm_id, &dispensed_notes)) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_TRANSACTION_FAILED;
    }

    /* 7. Generate transaction reference and insert transaction record */
    char txn_ref[36];
    utils_generate_txn_reference(txn_ref, sizeof(txn_ref));

    uint64_t aid_val = atm_id;
    TransactionRecordInput txn_in;
    memset(&txn_in, 0, sizeof(txn_in));
    strncpy(txn_in.transaction_reference, txn_ref, sizeof(txn_in.transaction_reference) - 1);
    txn_in.account_id = account_id;
    txn_in.type = TXN_TYPE_WITHDRAWAL;
    strncpy(txn_in.amount, wth_amt_str, sizeof(txn_in.amount) - 1);
    strncpy(txn_in.balance_before, cur_bal_str, sizeof(txn_in.balance_before) - 1);
    strncpy(txn_in.balance_after, new_bal_str, sizeof(txn_in.balance_after) - 1);
    txn_in.related_account_id = NULL;
    txn_in.atm_id = (atm_id > 0) ? &aid_val : NULL;
    txn_in.status = TXN_STATUS_SUCCESS;
    txn_in.description = "ATM Cash Withdrawal";

    if (!transaction_record_insert(&txn_in)) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_TRANSACTION_FAILED;
    }

    /* 8. Commit ACID transaction */
    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return WITHDRAWAL_ERR_TRANSACTION_FAILED;
    }

    /* Populate receipt if requested */
    if (receipt) {
        strncpy(receipt->transaction_reference, txn_ref, sizeof(receipt->transaction_reference) - 1);
        receipt->transaction_reference[sizeof(receipt->transaction_reference) - 1] = '\0';

        strncpy(receipt->withdrawn_amount, wth_amt_str, sizeof(receipt->withdrawn_amount) - 1);
        receipt->withdrawn_amount[sizeof(receipt->withdrawn_amount) - 1] = '\0';

        strncpy(receipt->previous_balance, cur_bal_str, sizeof(receipt->previous_balance) - 1);
        receipt->previous_balance[sizeof(receipt->previous_balance) - 1] = '\0';

        strncpy(receipt->new_balance, new_bal_str, sizeof(receipt->new_balance) - 1);
        receipt->new_balance[sizeof(receipt->new_balance) - 1] = '\0';

        receipt->dispensed_notes = dispensed_notes;
    }

    return WITHDRAWAL_SUCCESS;
}
