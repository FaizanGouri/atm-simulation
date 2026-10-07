#include "transaction.h"
#include "database.h"
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

static const char *transaction_status_to_string(TransactionStatus status)
{
    switch (status) {
        case TXN_STATUS_SUCCESS: return "SUCCESS";
        case TXN_STATUS_FAILED: return "FAILED";
        case TXN_STATUS_REVERSED: return "REVERSED";
        default: return "SUCCESS";
    }
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
