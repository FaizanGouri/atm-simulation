#include "transaction.h"
#include "database.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

const char *transaction_type_to_string(TransactionType type)
{
    switch (type) {
        case TXN_TYPE_WITHDRAWAL: return "WITHDRAWAL";
        case TXN_TYPE_DEPOSIT: return "DEPOSIT";
        case TXN_TYPE_TRANSFER: return "TRANSFER";
        case TXN_TYPE_BALANCE_ENQUIRY: return "BALANCE_ENQUIRY";
        case TXN_TYPE_PIN_CHANGE: return "PIN_CHANGE";
        default: return "WITHDRAWAL";
    }
}

static TransactionType transaction_type_from_string(const char *str)
{
    if (!str) return TXN_TYPE_WITHDRAWAL;
    if (strcmp(str, "DEPOSIT") == 0) return TXN_TYPE_DEPOSIT;
    if (strcmp(str, "TRANSFER") == 0) return TXN_TYPE_TRANSFER;
    if (strcmp(str, "BALANCE_ENQUIRY") == 0) return TXN_TYPE_BALANCE_ENQUIRY;
    if (strcmp(str, "PIN_CHANGE") == 0) return TXN_TYPE_PIN_CHANGE;
    return TXN_TYPE_WITHDRAWAL;
}

const char *transaction_status_to_string(TransactionStatus status)
{
    switch (status) {
        case TXN_STATUS_SUCCESS: return "SUCCESS";
        case TXN_STATUS_FAILED: return "FAILED";
        case TXN_STATUS_REVERSED: return "REVERSED";
        default: return "SUCCESS";
    }
}

static TransactionStatus transaction_status_from_string(const char *str)
{
    if (!str) return TXN_STATUS_SUCCESS;
    if (strcmp(str, "FAILED") == 0) return TXN_STATUS_FAILED;
    if (strcmp(str, "REVERSED") == 0) return TXN_STATUS_REVERSED;
    return TXN_STATUS_SUCCESS;
}

bool transaction_record_insert(const TransactionRecordInput *input)
{
    if (!input) return false;

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "INSERT INTO transactions (transaction_reference, account_id, transaction_type, "
        "amount, balance_before, balance_after, related_account_id, atm_id, transaction_status, description) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b[10];
    memset(b, 0, sizeof(b));

    unsigned long l_ref = (unsigned long)strlen(input->transaction_reference);
    b[0].buffer_type = MYSQL_TYPE_STRING;
    b[0].buffer = (char *)input->transaction_reference;
    b[0].length = &l_ref;

    unsigned long long aid = input->account_id;
    b[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b[1].buffer = &aid;

    const char *type_str = transaction_type_to_string(input->type);
    unsigned long l_type = (unsigned long)strlen(type_str);
    b[2].buffer_type = MYSQL_TYPE_STRING;
    b[2].buffer = (char *)type_str;
    b[2].length = &l_type;

    unsigned long l_amt = (unsigned long)strlen(input->amount);
    b[3].buffer_type = MYSQL_TYPE_STRING;
    b[3].buffer = (char *)input->amount;
    b[3].length = &l_amt;

    unsigned long l_bbef = (unsigned long)strlen(input->balance_before);
    b[4].buffer_type = MYSQL_TYPE_STRING;
    b[4].buffer = (char *)input->balance_before;
    b[4].length = &l_bbef;

    unsigned long l_baft = (unsigned long)strlen(input->balance_after);
    b[5].buffer_type = MYSQL_TYPE_STRING;
    b[5].buffer = (char *)input->balance_after;
    b[5].length = &l_baft;

    unsigned long long rel_aid = 0;
    bool is_rel_null = (input->related_account_id == NULL);
    if (!is_rel_null) rel_aid = *input->related_account_id;
    b[6].buffer_type = MYSQL_TYPE_LONGLONG;
    b[6].buffer = &rel_aid;
    b[6].is_null = (bool *)&is_rel_null;

    unsigned long long atmid = 0;
    bool is_atm_null = (input->atm_id == NULL);
    if (!is_atm_null) atmid = *input->atm_id;
    b[7].buffer_type = MYSQL_TYPE_LONGLONG;
    b[7].buffer = &atmid;
    b[7].is_null = (bool *)&is_atm_null;

    const char *stat_str = transaction_status_to_string(input->status);
    unsigned long l_stat = (unsigned long)strlen(stat_str);
    b[8].buffer_type = MYSQL_TYPE_STRING;
    b[8].buffer = (char *)stat_str;
    b[8].length = &l_stat;

    char empty_desc[1] = "";
    const char *desc_ptr = input->description ? input->description : empty_desc;
    unsigned long l_desc = (unsigned long)strlen(desc_ptr);
    b[9].buffer_type = MYSQL_TYPE_STRING;
    b[9].buffer = (char *)desc_ptr;
    b[9].length = &l_desc;

    if (mysql_stmt_bind_param(stmt, b) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool transaction_get_statement(uint64_t account_id, unsigned int limit, StatementList *list)
{
    if (!list) return false;
    memset(list, 0, sizeof(StatementList));

    if (limit == 0) limit = 10;
    if (limit > MINI_STATEMENT_MAX_ITEMS) limit = MINI_STATEMENT_MAX_ITEMS;

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "SELECT t.transaction_id, t.transaction_reference, t.transaction_type, "
        "CAST(t.amount AS CHAR), CAST(t.balance_before AS CHAR), CAST(t.balance_after AS CHAR), "
        "t.related_account_id, COALESCE(rel_acc.account_number, ''), "
        "t.transaction_status, COALESCE(t.description, ''), "
        "DATE_FORMAT(t.created_at, '%d-%b-%Y %H:%i') "
        "FROM transactions t "
        "LEFT JOIN accounts rel_acc ON t.related_account_id = rel_acc.account_id "
        "WHERE t.account_id = ? "
        "ORDER BY t.created_at DESC, t.transaction_id DESC "
        "LIMIT ?";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b_in[2];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long aid = account_id;
    unsigned int lim = limit;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid;
    b_in[1].buffer_type = MYSQL_TYPE_LONG;
    b_in[1].buffer = &lim;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long long s_tid = 0;
    char s_ref[36] = {0};
    char s_type[32] = {0};
    char s_amount[32] = {0};
    char s_bbef[32] = {0};
    char s_baft[32] = {0};
    unsigned long long s_rel_id = 0;
    bool is_rel_null = false;
    char s_rel_acc_num[35] = {0};
    char s_status[20] = {0};
    char s_desc[255] = {0};
    char s_dt[32] = {0};

    unsigned long l_ref = 0, l_type = 0, l_amount = 0, l_bbef = 0, l_baft = 0;
    unsigned long l_rel_num = 0, l_status = 0, l_desc = 0, l_dt = 0;

    MYSQL_BIND b_out[11];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &s_tid;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = s_ref;
    b_out[1].buffer_length = sizeof(s_ref);
    b_out[1].length = &l_ref;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = s_type;
    b_out[2].buffer_length = sizeof(s_type);
    b_out[2].length = &l_type;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = s_amount;
    b_out[3].buffer_length = sizeof(s_amount);
    b_out[3].length = &l_amount;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = s_bbef;
    b_out[4].buffer_length = sizeof(s_bbef);
    b_out[4].length = &l_bbef;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = s_baft;
    b_out[5].buffer_length = sizeof(s_baft);
    b_out[5].length = &l_baft;

    b_out[6].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[6].buffer = &s_rel_id;
    b_out[6].is_null = (bool *)&is_rel_null;

    b_out[7].buffer_type = MYSQL_TYPE_STRING;
    b_out[7].buffer = s_rel_acc_num;
    b_out[7].buffer_length = sizeof(s_rel_acc_num);
    b_out[7].length = &l_rel_num;

    b_out[8].buffer_type = MYSQL_TYPE_STRING;
    b_out[8].buffer = s_status;
    b_out[8].buffer_length = sizeof(s_status);
    b_out[8].length = &l_status;

    b_out[9].buffer_type = MYSQL_TYPE_STRING;
    b_out[9].buffer = s_desc;
    b_out[9].buffer_length = sizeof(s_desc);
    b_out[9].length = &l_desc;

    b_out[10].buffer_type = MYSQL_TYPE_STRING;
    b_out[10].buffer = s_dt;
    b_out[10].buffer_length = sizeof(s_dt);
    b_out[10].length = &l_dt;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0 && list->count < limit) {
        StatementItem *item = &list->items[list->count++];
        item->transaction_id = s_tid;
        strncpy(item->transaction_reference, s_ref, sizeof(item->transaction_reference) - 1);
        item->type = transaction_type_from_string(s_type);
        strncpy(item->amount, s_amount, sizeof(item->amount) - 1);
        strncpy(item->balance_before, s_bbef, sizeof(item->balance_before) - 1);
        strncpy(item->balance_after, s_baft, sizeof(item->balance_after) - 1);

        item->has_related_account = !is_rel_null;
        item->related_account_id = is_rel_null ? 0 : s_rel_id;
        strncpy(item->related_account_number, s_rel_acc_num, sizeof(item->related_account_number) - 1);

        item->status = transaction_status_from_string(s_status);
        strncpy(item->description, s_desc, sizeof(item->description) - 1);
        strncpy(item->created_at_formatted, s_dt, sizeof(item->created_at_formatted) - 1);

        /* Determine if credit (+) or debit (-) */
        if (item->type == TXN_TYPE_DEPOSIT) {
            item->is_credit = true;
        } else if (item->type == TXN_TYPE_WITHDRAWAL) {
            item->is_credit = false;
        } else if (item->type == TXN_TYPE_TRANSFER) {
            int64_t bb = 0, ba = 0;
            utils_parse_amount_to_paise(s_bbef, &bb);
            utils_parse_amount_to_paise(s_baft, &ba);
            item->is_credit = (ba > bb);
        } else {
            item->is_credit = false;
        }
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}
