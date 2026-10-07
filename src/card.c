#include "card.h"
#include "database.h"
#include <stdio.h>
#include <string.h>

CardStatus card_status_from_string(const char *status_str)
{
    if (!status_str) return CARD_STATUS_UNKNOWN;
    if (strcmp(status_str, "ACTIVE") == 0) return CARD_STATUS_ACTIVE;
    if (strcmp(status_str, "BLOCKED") == 0) return CARD_STATUS_BLOCKED;
    if (strcmp(status_str, "EXPIRED") == 0) return CARD_STATUS_EXPIRED;
    if (strcmp(status_str, "CANCELLED") == 0) return CARD_STATUS_CANCELLED;
    return CARD_STATUS_UNKNOWN;
}

const char *card_status_to_string(CardStatus status)
{
    switch (status) {
        case CARD_STATUS_ACTIVE: return "ACTIVE";
        case CARD_STATUS_BLOCKED: return "BLOCKED";
        case CARD_STATUS_EXPIRED: return "EXPIRED";
        case CARD_STATUS_CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

bool card_find_by_number(const char *card_number, CardRecord *card)
{
    if (!card_number || !card) return false;

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query = "SELECT card_id, account_id, card_number, pin_hash, failed_pin_attempts, card_status, expiry_date "
                        "FROM cards WHERE card_number = ? LIMIT 1";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[1];
    memset(param_bind, 0, sizeof(param_bind));
    unsigned long card_len = (unsigned long)strlen(card_number);

    param_bind[0].buffer_type = MYSQL_TYPE_STRING;
    param_bind[0].buffer = (char *)card_number;
    param_bind[0].length = &card_len;

    if (mysql_stmt_bind_param(stmt, param_bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    /* Result buffers */
    unsigned long long b_card_id = 0;
    unsigned long long b_account_id = 0;
    char b_card_number[21] = {0};
    char b_pin_hash[65] = {0};
    unsigned char b_failed_attempts = 0;
    char b_status_str[20] = {0};
    char b_expiry_date[15] = {0};

    unsigned long l_card_num = 0, l_pin = 0, l_status = 0, l_expiry = 0;

    MYSQL_BIND res_bind[7];
    memset(res_bind, 0, sizeof(res_bind));

    res_bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    res_bind[0].buffer = &b_card_id;

    res_bind[1].buffer_type = MYSQL_TYPE_LONGLONG;
    res_bind[1].buffer = &b_account_id;

    res_bind[2].buffer_type = MYSQL_TYPE_STRING;
    res_bind[2].buffer = b_card_number;
    res_bind[2].buffer_length = sizeof(b_card_number);
    res_bind[2].length = &l_card_num;

    res_bind[3].buffer_type = MYSQL_TYPE_STRING;
    res_bind[3].buffer = b_pin_hash;
    res_bind[3].buffer_length = sizeof(b_pin_hash);
    res_bind[3].length = &l_pin;

    res_bind[4].buffer_type = MYSQL_TYPE_TINY;
    res_bind[4].buffer = &b_failed_attempts;

    res_bind[5].buffer_type = MYSQL_TYPE_STRING;
    res_bind[5].buffer = b_status_str;
    res_bind[5].buffer_length = sizeof(b_status_str);
    res_bind[5].length = &l_status;

    res_bind[6].buffer_type = MYSQL_TYPE_STRING;
    res_bind[6].buffer = b_expiry_date;
    res_bind[6].buffer_length = sizeof(b_expiry_date);
    res_bind[6].length = &l_expiry;

    if (mysql_stmt_bind_result(stmt, res_bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool found = false;
    if (mysql_stmt_fetch(stmt) == 0) {
        card->card_id = b_card_id;
        card->account_id = b_account_id;
        strncpy(card->card_number, b_card_number, sizeof(card->card_number) - 1);
        card->card_number[sizeof(card->card_number) - 1] = '\0';
        strncpy(card->pin_hash, b_pin_hash, sizeof(card->pin_hash) - 1);
        card->pin_hash[sizeof(card->pin_hash) - 1] = '\0';
        card->failed_pin_attempts = b_failed_attempts;
        card->status = card_status_from_string(b_status_str);
        strncpy(card->expiry_date, b_expiry_date, sizeof(card->expiry_date) - 1);
        card->expiry_date[sizeof(card->expiry_date) - 1] = '\0';
        found = true;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return found;
}

bool card_increment_failed_attempts(uint64_t card_id, uint8_t *updated_attempts, bool *is_now_blocked)
{
    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    if (!db_transaction_begin()) return false;

    /* Read current failed attempts and status */
    const char *sel_query = "SELECT failed_pin_attempts FROM cards WHERE card_id = ? FOR UPDATE";
    MYSQL_STMT *sel_stmt = mysql_stmt_init(conn);
    if (!sel_stmt) {
        db_transaction_rollback();
        return false;
    }

    if (mysql_stmt_prepare(sel_stmt, sel_query, (unsigned long)strlen(sel_query)) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return false;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long cid = card_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &cid;

    if (mysql_stmt_bind_param(sel_stmt, b_in) != 0 || mysql_stmt_execute(sel_stmt) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return false;
    }

    unsigned char current_attempts = 0;
    MYSQL_BIND b_out[1];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_TINY;
    b_out[0].buffer = &current_attempts;

    if (mysql_stmt_bind_result(sel_stmt, b_out) != 0 || mysql_stmt_store_result(sel_stmt) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return false;
    }

    if (mysql_stmt_fetch(sel_stmt) != 0) {
        mysql_stmt_free_result(sel_stmt);
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return false;
    }

    mysql_stmt_free_result(sel_stmt);
    mysql_stmt_close(sel_stmt);

    current_attempts++;
    bool block = (current_attempts >= 3);
    if (current_attempts > 3) current_attempts = 3;

    /* Update attempts and status */
    const char *upd_query = block ?
        "UPDATE cards SET failed_pin_attempts = ?, card_status = 'BLOCKED' WHERE card_id = ?" :
        "UPDATE cards SET failed_pin_attempts = ? WHERE card_id = ?";

    MYSQL_STMT *upd_stmt = mysql_stmt_init(conn);
    if (!upd_stmt) {
        db_transaction_rollback();
        return false;
    }

    if (mysql_stmt_prepare(upd_stmt, upd_query, (unsigned long)strlen(upd_query)) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return false;
    }

    MYSQL_BIND b_upd[2];
    memset(b_upd, 0, sizeof(b_upd));
    b_upd[0].buffer_type = MYSQL_TYPE_TINY;
    b_upd[0].buffer = &current_attempts;
    b_upd[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_upd[1].buffer = &cid;

    if (mysql_stmt_bind_param(upd_stmt, b_upd) != 0 || mysql_stmt_execute(upd_stmt) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return false;
    }

    mysql_stmt_close(upd_stmt);

    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return false;
    }

    if (updated_attempts) *updated_attempts = current_attempts;
    if (is_now_blocked) *is_now_blocked = block;
    return true;
}

bool card_reset_failed_attempts(uint64_t card_id)
{
    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query = "UPDATE cards SET failed_pin_attempts = 0, last_used_at = CURRENT_TIMESTAMP WHERE card_id = ?";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b[1];
    memset(b, 0, sizeof(b));
    unsigned long long cid = card_id;
    b[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b[0].buffer = &cid;

    if (mysql_stmt_bind_param(stmt, b) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool card_update_status(uint64_t card_id, CardStatus new_status)
{
    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *status_str = card_status_to_string(new_status);
    const char *query = "UPDATE cards SET card_status = ? WHERE card_id = ?";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b[2];
    memset(b, 0, sizeof(b));
    unsigned long status_len = (unsigned long)strlen(status_str);
    unsigned long long cid = card_id;

    b[0].buffer_type = MYSQL_TYPE_STRING;
    b[0].buffer = (char *)status_str;
    b[0].length = &status_len;

    b[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b[1].buffer = &cid;

    if (mysql_stmt_bind_param(stmt, b) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}
