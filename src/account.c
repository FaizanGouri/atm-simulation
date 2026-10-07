#include "account.h"
#include "database.h"
#include <stdio.h>
#include <string.h>

static AccountStatus account_status_from_string(const char *status_str)
{
    if (!status_str) return ACCOUNT_STATUS_UNKNOWN;
    if (strcmp(status_str, "ACTIVE") == 0) return ACCOUNT_STATUS_ACTIVE;
    if (strcmp(status_str, "BLOCKED") == 0) return ACCOUNT_STATUS_BLOCKED;
    if (strcmp(status_str, "CLOSED") == 0) return ACCOUNT_STATUS_CLOSED;
    return ACCOUNT_STATUS_UNKNOWN;
}

bool account_get_by_id(uint64_t account_id, AccountRecord *account)
{
    if (!account) return false;

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query = "SELECT account_id, customer_id, account_number, account_type, "
                        "CAST(balance AS CHAR), CAST(daily_withdrawal_limit AS CHAR), status "
                        "FROM accounts WHERE account_id = ? LIMIT 1";

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

    unsigned long long s_aid = 0, s_cid = 0;
    char s_acc_num[35] = {0};
    char s_acc_type[16] = {0};
    char s_bal[32] = {0};
    char s_limit[32] = {0};
    char s_status[20] = {0};

    unsigned long l_num = 0, l_type = 0, l_bal = 0, l_limit = 0, l_stat = 0;

    MYSQL_BIND b_out[7];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &s_aid;

    b_out[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[1].buffer = &s_cid;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = s_acc_num;
    b_out[2].buffer_length = sizeof(s_acc_num);
    b_out[2].length = &l_num;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = s_acc_type;
    b_out[3].buffer_length = sizeof(s_acc_type);
    b_out[3].length = &l_type;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = s_bal;
    b_out[4].buffer_length = sizeof(s_bal);
    b_out[4].length = &l_bal;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = s_limit;
    b_out[5].buffer_length = sizeof(s_limit);
    b_out[5].length = &l_limit;

    b_out[6].buffer_type = MYSQL_TYPE_STRING;
    b_out[6].buffer = s_status;
    b_out[6].buffer_length = sizeof(s_status);
    b_out[6].length = &l_stat;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool found = false;
    if (mysql_stmt_fetch(stmt) == 0) {
        account->account_id = s_aid;
        account->customer_id = s_cid;
        strncpy(account->account_number, s_acc_num, sizeof(account->account_number) - 1);
        account->account_number[sizeof(account->account_number) - 1] = '\0';
        strncpy(account->account_type, s_acc_type, sizeof(account->account_type) - 1);
        account->account_type[sizeof(account->account_type) - 1] = '\0';
        strncpy(account->balance, s_bal, sizeof(account->balance) - 1);
        account->balance[sizeof(account->balance) - 1] = '\0';
        strncpy(account->daily_withdrawal_limit, s_limit, sizeof(account->daily_withdrawal_limit) - 1);
        account->daily_withdrawal_limit[sizeof(account->daily_withdrawal_limit) - 1] = '\0';
        account->status = account_status_from_string(s_status);
        found = true;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return found;
}

bool account_get_by_number(const char *account_number, AccountRecord *account)
{
    if (!account_number || !account) return false;

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query = "SELECT account_id, customer_id, account_number, account_type, "
                        "CAST(balance AS CHAR), CAST(daily_withdrawal_limit AS CHAR), status "
                        "FROM accounts WHERE account_number = ? LIMIT 1";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long l_in_num = (unsigned long)strlen(account_number);
    b_in[0].buffer_type = MYSQL_TYPE_STRING;
    b_in[0].buffer = (char *)account_number;
    b_in[0].length = &l_in_num;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long long s_aid = 0, s_cid = 0;
    char s_acc_num[35] = {0};
    char s_acc_type[16] = {0};
    char s_bal[32] = {0};
    char s_limit[32] = {0};
    char s_status[20] = {0};

    unsigned long l_num = 0, l_type = 0, l_bal = 0, l_limit = 0, l_stat = 0;

    MYSQL_BIND b_out[7];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &s_aid;

    b_out[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[1].buffer = &s_cid;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = s_acc_num;
    b_out[2].buffer_length = sizeof(s_acc_num);
    b_out[2].length = &l_num;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = s_acc_type;
    b_out[3].buffer_length = sizeof(s_acc_type);
    b_out[3].length = &l_type;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = s_bal;
    b_out[4].buffer_length = sizeof(s_bal);
    b_out[4].length = &l_bal;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = s_limit;
    b_out[5].buffer_length = sizeof(s_limit);
    b_out[5].length = &l_limit;

    b_out[6].buffer_type = MYSQL_TYPE_STRING;
    b_out[6].buffer = s_status;
    b_out[6].buffer_length = sizeof(s_status);
    b_out[6].length = &l_stat;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool found = false;
    if (mysql_stmt_fetch(stmt) == 0) {
        account->account_id = s_aid;
        account->customer_id = s_cid;
        strncpy(account->account_number, s_acc_num, sizeof(account->account_number) - 1);
        account->account_number[sizeof(account->account_number) - 1] = '\0';
        strncpy(account->account_type, s_acc_type, sizeof(account->account_type) - 1);
        account->account_type[sizeof(account->account_type) - 1] = '\0';
        strncpy(account->balance, s_bal, sizeof(account->balance) - 1);
        account->balance[sizeof(account->balance) - 1] = '\0';
        strncpy(account->daily_withdrawal_limit, s_limit, sizeof(account->daily_withdrawal_limit) - 1);
        account->daily_withdrawal_limit[sizeof(account->daily_withdrawal_limit) - 1] = '\0';
        account->status = account_status_from_string(s_status);
        found = true;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return found;
}

void account_mask_number(const char *account_number, char *masked, size_t masked_size)
{
    if (!masked || masked_size == 0) return;
    if (!account_number) {
        masked[0] = '\0';
        return;
    }

    size_t len = strlen(account_number);
    if (len <= 4) {
        strncpy(masked, account_number, masked_size - 1);
        masked[masked_size - 1] = '\0';
        return;
    }

    /* Keep last 4 digits, mask previous */
    const char *last4 = account_number + (len - 4);
    snprintf(masked, masked_size, "XXXX-XXXX-%s", last4);
}

bool account_format_currency(const char *raw_amount, char *formatted, size_t formatted_size)
{
    if (!raw_amount || !formatted || formatted_size < 16) return false;

    /* Parse integer and fraction parts from decimal string (e.g., "45000.00") */
    const char *dot = strchr(raw_amount, '.');
    char int_part[32] = {0};
    char frac_part[8] = "00";

    if (dot) {
        size_t int_len = (size_t)(dot - raw_amount);
        if (int_len >= sizeof(int_part)) int_len = sizeof(int_part) - 1;
        strncpy(int_part, raw_amount, int_len);
        int_part[int_len] = '\0';
        strncpy(frac_part, dot + 1, sizeof(frac_part) - 1);
        frac_part[sizeof(frac_part) - 1] = '\0';
    } else {
        strncpy(int_part, raw_amount, sizeof(int_part) - 1);
        int_part[sizeof(int_part) - 1] = '\0';
    }

    /* Format integer part with standard Indian comma grouping:
       Last 3 digits grouped, then in pairs of 2 digits (e.g. 12,34,567) */
    size_t n = strlen(int_part);
    char grouped[48] = {0};
    int g_idx = 0;

    if (n <= 3) {
        strncpy(grouped, int_part, sizeof(grouped) - 1);
    } else {
        /* Work backwards */
        char rev_grouped[48] = {0};
        int rev_idx = 0;
        int count = 0;
        bool first_group = true;

        for (int i = (int)n - 1; i >= 0; i--) {
            rev_grouped[rev_idx++] = int_part[i];
            count++;
            if (first_group && count == 3 && i > 0) {
                rev_grouped[rev_idx++] = ',';
                count = 0;
                first_group = false;
            } else if (!first_group && count == 2 && i > 0) {
                rev_grouped[rev_idx++] = ',';
                count = 0;
            }
        }
        rev_grouped[rev_idx] = '\0';

        /* Reverse back */
        for (int i = rev_idx - 1; i >= 0; i--) {
            grouped[g_idx++] = rev_grouped[i];
        }
        grouped[g_idx] = '\0';
    }

    snprintf(formatted, formatted_size, "Rs. %s.%s", grouped, frac_part);
    return true;
}
