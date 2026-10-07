#include "transfer.h"
#include "account.h"
#include "database.h"
#include "transaction.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

const char *transfer_result_to_string(TransferResult result)
{
    switch (result) {
        case TRANSFER_SUCCESS:
            return "Fund transfer completed successfully";
        case TRANSFER_ERR_INVALID_AMOUNT:
            return "Invalid transfer amount. Must be a positive amount with up to 2 decimal places";
        case TRANSFER_ERR_SOURCE_INACTIVE:
            return "Source account is not active or has been blocked";
        case TRANSFER_ERR_DEST_INACTIVE:
            return "Beneficiary destination account is not active or has been closed";
        case TRANSFER_ERR_INSUFFICIENT_FUNDS:
            return "Insufficient funds in your account for this transfer";
        case TRANSFER_ERR_SELF_TRANSFER:
            return "Self-transfer is not permitted";
        case TRANSFER_ERR_BENEFICIARY_INVALID:
            return "Selected beneficiary is invalid or not registered for your account";
        case TRANSFER_ERR_ACCOUNT_NOT_FOUND:
            return "Bank account record not found";
        case TRANSFER_ERR_TRANSACTION_FAILED:
            return "Transfer transaction failed during database execution";
        case TRANSFER_ERR_SYSTEM:
            return "System or connection error";
        default:
            return "Unknown error";
    }
}

static bool lock_account_row(MYSQL *conn,
                             uint64_t account_id,
                             char *out_balance,
                             char *out_status,
                             char *out_account_number)
{
    const char *query =
        "SELECT CAST(balance AS CHAR), status, account_number "
        "FROM accounts WHERE account_id = ? FOR UPDATE";

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

    char s_bal[32] = {0};
    char s_stat[20] = {0};
    char s_acc_num[35] = {0};
    unsigned long l_bal = 0, l_stat = 0, l_num = 0;

    MYSQL_BIND b_out[3];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = s_bal;
    b_out[0].buffer_length = sizeof(s_bal);
    b_out[0].length = &l_bal;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = s_stat;
    b_out[1].buffer_length = sizeof(s_stat);
    b_out[1].length = &l_stat;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = s_acc_num;
    b_out[2].buffer_length = sizeof(s_acc_num);
    b_out[2].length = &l_num;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    strncpy(out_balance, s_bal, 31);
    out_balance[31] = '\0';
    strncpy(out_status, s_stat, 19);
    out_status[19] = '\0';
    strncpy(out_account_number, s_acc_num, 34);
    out_account_number[34] = '\0';

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

static bool update_account_balance(MYSQL *conn, uint64_t account_id, const char *new_balance)
{
    const char *query = "UPDATE accounts SET balance = ? WHERE account_id = ?";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b[2];
    memset(b, 0, sizeof(b));

    unsigned long l_bal = (unsigned long)strlen(new_balance);
    b[0].buffer_type = MYSQL_TYPE_STRING;
    b[0].buffer = (char *)new_balance;
    b[0].length = &l_bal;

    unsigned long long aid = account_id;
    b[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b[1].buffer = &aid;

    if (mysql_stmt_bind_param(stmt, b) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

TransferResult transfer_execute(uint64_t source_account_id,
                                uint64_t beneficiary_id,
                                const char *amount_str,
                                TransferReceipt *receipt)
{
    if (!amount_str) return TRANSFER_ERR_INVALID_AMOUNT;

    int64_t transfer_paise = 0;
    if (!utils_parse_amount_to_paise(amount_str, &transfer_paise) || transfer_paise <= 0) {
        return TRANSFER_ERR_INVALID_AMOUNT;
    }

    char transfer_amt_str[32];
    utils_paise_to_decimal_str(transfer_paise, transfer_amt_str, sizeof(transfer_amt_str));

    MYSQL *conn = db_get_connection();
    if (!conn) return TRANSFER_ERR_SYSTEM;

    /* Start ACID Transaction */
    if (!db_transaction_begin()) return TRANSFER_ERR_SYSTEM;

    /* 1. Fetch beneficiary details and verify ownership */
    const char *b_query =
        "SELECT b.beneficiary_account_id, b.beneficiary_name, b.status "
        "FROM beneficiaries b "
        "WHERE b.beneficiary_id = ? AND b.account_id = ? LIMIT 1";

    MYSQL_STMT *b_stmt = mysql_stmt_init(conn);
    if (!b_stmt) {
        db_transaction_rollback();
        return TRANSFER_ERR_SYSTEM;
    }

    if (mysql_stmt_prepare(b_stmt, b_query, (unsigned long)strlen(b_query)) != 0) {
        mysql_stmt_close(b_stmt);
        db_transaction_rollback();
        return TRANSFER_ERR_SYSTEM;
    }

    MYSQL_BIND b_in[2];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long bid = beneficiary_id;
    unsigned long long src_id = source_account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &bid;
    b_in[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[1].buffer = &src_id;

    if (mysql_stmt_bind_param(b_stmt, b_in) != 0 || mysql_stmt_execute(b_stmt) != 0) {
        mysql_stmt_close(b_stmt);
        db_transaction_rollback();
        return TRANSFER_ERR_TRANSACTION_FAILED;
    }

    unsigned long long dest_account_id = 0;
    char b_name[100] = {0};
    char b_status[20] = {0};
    unsigned long l_bname = 0, l_bstat = 0;

    MYSQL_BIND b_out[3];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &dest_account_id;
    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = b_name;
    b_out[1].buffer_length = sizeof(b_name);
    b_out[1].length = &l_bname;
    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = b_status;
    b_out[2].buffer_length = sizeof(b_status);
    b_out[2].length = &l_bstat;

    if (mysql_stmt_bind_result(b_stmt, b_out) != 0 || mysql_stmt_store_result(b_stmt) != 0) {
        mysql_stmt_close(b_stmt);
        db_transaction_rollback();
        return TRANSFER_ERR_TRANSACTION_FAILED;
    }

    if (mysql_stmt_fetch(b_stmt) != 0) {
        mysql_stmt_free_result(b_stmt);
        mysql_stmt_close(b_stmt);
        db_transaction_rollback();
        return TRANSFER_ERR_BENEFICIARY_INVALID;
    }
    mysql_stmt_free_result(b_stmt);
    mysql_stmt_close(b_stmt);

    if (strcmp(b_status, "ACTIVE") != 0) {
        db_transaction_rollback();
        return TRANSFER_ERR_BENEFICIARY_INVALID;
    }

    /* 2. Prevent self transfer */
    if (source_account_id == dest_account_id) {
        db_transaction_rollback();
        return TRANSFER_ERR_SELF_TRANSFER;
    }

    /* 3. Deadlock-free account locking (ascending account_id order) */
    uint64_t lock_first = (source_account_id < dest_account_id) ? source_account_id : dest_account_id;
    uint64_t lock_second = (source_account_id < dest_account_id) ? dest_account_id : source_account_id;

    char bal1[32] = {0}, stat1[20] = {0}, acc_num1[35] = {0};
    char bal2[32] = {0}, stat2[20] = {0}, acc_num2[35] = {0};

    if (!lock_account_row(conn, lock_first, bal1, stat1, acc_num1)) {
        db_transaction_rollback();
        return TRANSFER_ERR_ACCOUNT_NOT_FOUND;
    }
    if (!lock_account_row(conn, lock_second, bal2, stat2, acc_num2)) {
        db_transaction_rollback();
        return TRANSFER_ERR_ACCOUNT_NOT_FOUND;
    }

    char *src_bal_str = (source_account_id == lock_first) ? bal1 : bal2;
    char *src_stat = (source_account_id == lock_first) ? stat1 : stat2;
    char *src_num = (source_account_id == lock_first) ? acc_num1 : acc_num2;

    char *dst_bal_str = (dest_account_id == lock_first) ? bal1 : bal2;
    char *dst_stat = (dest_account_id == lock_first) ? stat1 : stat2;
    char *dst_num = (dest_account_id == lock_first) ? acc_num1 : acc_num2;

    /* 4. Validate Account Statuses */
    if (strcmp(src_stat, "ACTIVE") != 0) {
        db_transaction_rollback();
        return TRANSFER_ERR_SOURCE_INACTIVE;
    }
    if (strcmp(dst_stat, "ACTIVE") != 0) {
        db_transaction_rollback();
        return TRANSFER_ERR_DEST_INACTIVE;
    }

    /* 5. Check Sufficient Balance */
    int64_t src_bal_paise = 0;
    if (!utils_parse_amount_to_paise(src_bal_str, &src_bal_paise)) {
        src_bal_paise = 0;
    }
    if (src_bal_paise < transfer_paise) {
        db_transaction_rollback();
        return TRANSFER_ERR_INSUFFICIENT_FUNDS;
    }

    int64_t dst_bal_paise = 0;
    if (!utils_parse_amount_to_paise(dst_bal_str, &dst_bal_paise)) {
        dst_bal_paise = 0;
    }

    /* 6. Calculate New Balances in exact integer paise */
    int64_t src_new_paise = src_bal_paise - transfer_paise;
    int64_t dst_new_paise = dst_bal_paise + transfer_paise;

    char src_new_str[32], dst_new_str[32];
    utils_paise_to_decimal_str(src_new_paise, src_new_str, sizeof(src_new_str));
    utils_paise_to_decimal_str(dst_new_paise, dst_new_str, sizeof(dst_new_str));

    /* 7. Update Balances */
    if (!update_account_balance(conn, source_account_id, src_new_str)) {
        db_transaction_rollback();
        return TRANSFER_ERR_TRANSACTION_FAILED;
    }
    if (!update_account_balance(conn, dest_account_id, dst_new_str)) {
        db_transaction_rollback();
        return TRANSFER_ERR_TRANSACTION_FAILED;
    }

    /* 8. Generate Unique Transaction References */
    char base_ref[36];
    utils_generate_txn_reference(base_ref, sizeof(base_ref));

    char ref_dr[48], ref_cr[48];
    snprintf(ref_dr, sizeof(ref_dr), "%s-DR", base_ref);
    snprintf(ref_cr, sizeof(ref_cr), "%s-CR", base_ref);

    char masked_src[32], masked_dst[32];
    account_mask_number(src_num, masked_src, sizeof(masked_src));
    account_mask_number(dst_num, masked_dst, sizeof(masked_dst));

    /* 9. Insert Debit Transaction Record */
    char desc_dr[255];
    snprintf(desc_dr, sizeof(desc_dr), "Fund transfer to %s (%s)", b_name, masked_dst);

    uint64_t d_aid = dest_account_id;
    uint64_t s_aid = source_account_id;

    TransactionRecordInput txn_dr;
    memset(&txn_dr, 0, sizeof(txn_dr));
    strncpy(txn_dr.transaction_reference, ref_dr, sizeof(txn_dr.transaction_reference) - 1);
    txn_dr.account_id = source_account_id;
    txn_dr.type = TXN_TYPE_TRANSFER;
    strncpy(txn_dr.amount, transfer_amt_str, sizeof(txn_dr.amount) - 1);
    strncpy(txn_dr.balance_before, src_bal_str, sizeof(txn_dr.balance_before) - 1);
    strncpy(txn_dr.balance_after, src_new_str, sizeof(txn_dr.balance_after) - 1);
    txn_dr.related_account_id = &d_aid;
    txn_dr.atm_id = NULL;
    txn_dr.status = TXN_STATUS_SUCCESS;
    txn_dr.description = desc_dr;

    if (!transaction_record_insert(&txn_dr)) {
        db_transaction_rollback();
        return TRANSFER_ERR_TRANSACTION_FAILED;
    }

    /* 10. Insert Credit Transaction Record */
    char desc_cr[255];
    snprintf(desc_cr, sizeof(desc_cr), "Fund transfer from %s", masked_src);

    TransactionRecordInput txn_cr;
    memset(&txn_cr, 0, sizeof(txn_cr));
    strncpy(txn_cr.transaction_reference, ref_cr, sizeof(txn_cr.transaction_reference) - 1);
    txn_cr.account_id = dest_account_id;
    txn_cr.type = TXN_TYPE_TRANSFER;
    strncpy(txn_cr.amount, transfer_amt_str, sizeof(txn_cr.amount) - 1);
    strncpy(txn_cr.balance_before, dst_bal_str, sizeof(txn_cr.balance_before) - 1);
    strncpy(txn_cr.balance_after, dst_new_str, sizeof(txn_cr.balance_after) - 1);
    txn_cr.related_account_id = &s_aid;
    txn_cr.atm_id = NULL;
    txn_cr.status = TXN_STATUS_SUCCESS;
    txn_cr.description = desc_cr;

    if (!transaction_record_insert(&txn_cr)) {
        db_transaction_rollback();
        return TRANSFER_ERR_TRANSACTION_FAILED;
    }

    /* 11. Commit ACID Transaction */
    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return TRANSFER_ERR_TRANSACTION_FAILED;
    }

    /* 12. Fill Receipt */
    if (receipt) {
        strncpy(receipt->transaction_reference, base_ref, sizeof(receipt->transaction_reference) - 1);
        receipt->transaction_reference[sizeof(receipt->transaction_reference) - 1] = '\0';

        strncpy(receipt->beneficiary_name, b_name, sizeof(receipt->beneficiary_name) - 1);
        receipt->beneficiary_name[sizeof(receipt->beneficiary_name) - 1] = '\0';

        strncpy(receipt->dest_account_masked, masked_dst, sizeof(receipt->dest_account_masked) - 1);
        receipt->dest_account_masked[sizeof(receipt->dest_account_masked) - 1] = '\0';

        strncpy(receipt->transferred_amount, transfer_amt_str, sizeof(receipt->transferred_amount) - 1);
        receipt->transferred_amount[sizeof(receipt->transferred_amount) - 1] = '\0';

        strncpy(receipt->source_prev_balance, src_bal_str, sizeof(receipt->source_prev_balance) - 1);
        receipt->source_prev_balance[sizeof(receipt->source_prev_balance) - 1] = '\0';

        strncpy(receipt->source_new_balance, src_new_str, sizeof(receipt->source_new_balance) - 1);
        receipt->source_new_balance[sizeof(receipt->source_new_balance) - 1] = '\0';
    }

    return TRANSFER_SUCCESS;
}
