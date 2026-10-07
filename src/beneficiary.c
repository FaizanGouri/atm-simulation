#include "beneficiary.h"
#include "account.h"
#include "database.h"
#include <stdio.h>
#include <string.h>

const char *beneficiary_result_to_string(BeneficiaryResult result)
{
    switch (result) {
        case BENEFICIARY_SUCCESS:
            return "Beneficiary operation successful";
        case BENEFICIARY_ERR_INVALID_ACCOUNT:
            return "Invalid beneficiary account number format";
        case BENEFICIARY_ERR_ACCOUNT_NOT_FOUND:
            return "Beneficiary account not found";
        case BENEFICIARY_ERR_ACCOUNT_INACTIVE:
            return "Beneficiary account is not active or has been closed";
        case BENEFICIARY_ERR_SELF_ADD:
            return "Cannot add your own account as a beneficiary";
        case BENEFICIARY_ERR_DUPLICATE:
            return "This account is already registered as an active beneficiary";
        case BENEFICIARY_ERR_NOT_FOUND:
            return "Beneficiary record not found";
        case BENEFICIARY_ERR_UNAUTHORIZED:
            return "Unauthorized access: Beneficiary does not belong to this account";
        case BENEFICIARY_ERR_DATABASE:
            return "Database operation failed";
        case BENEFICIARY_ERR_SYSTEM:
            return "System or connection error";
        default:
            return "Unknown error";
    }
}

static bool fetch_customer_name(MYSQL *conn, uint64_t customer_id, char *out_name, size_t name_size)
{
    if (!conn || !out_name || name_size == 0) return false;

    const char *query = "SELECT full_name FROM customers WHERE customer_id = ? LIMIT 1";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long cid = customer_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &cid;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    char name_buf[100] = {0};
    unsigned long l_name = 0;
    MYSQL_BIND b_out[1];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = name_buf;
    b_out[0].buffer_length = sizeof(name_buf);
    b_out[0].length = &l_name;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool found = false;
    if (mysql_stmt_fetch(stmt) == 0) {
        strncpy(out_name, name_buf, name_size - 1);
        out_name[name_size - 1] = '\0';
        found = true;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return found;
}

BeneficiaryResult beneficiary_add(uint64_t source_account_id,
                                  const char *target_account_number,
                                  const char *nickname,
                                  uint64_t *out_beneficiary_id)
{
    if (!target_account_number || strlen(target_account_number) < 5) {
        return BENEFICIARY_ERR_INVALID_ACCOUNT;
    }

    MYSQL *conn = db_get_connection();
    if (!conn) return BENEFICIARY_ERR_SYSTEM;

    /* 1. Retrieve destination account details */
    AccountRecord dest_acc;
    if (!account_get_by_number(target_account_number, &dest_acc)) {
        return BENEFICIARY_ERR_ACCOUNT_NOT_FOUND;
    }

    /* 2. Disallow self as beneficiary */
    if (dest_acc.account_id == source_account_id) {
        return BENEFICIARY_ERR_SELF_ADD;
    }

    /* 3. Verify destination account is active */
    if (dest_acc.status != ACCOUNT_STATUS_ACTIVE) {
        return BENEFICIARY_ERR_ACCOUNT_INACTIVE;
    }

    /* 4. Get customer full name for beneficiary record */
    char beneficiary_name[100] = "Beneficiary Customer";
    fetch_customer_name(conn, dest_acc.customer_id, beneficiary_name, sizeof(beneficiary_name));

    /* 5. Check if relation already exists */
    const char *chk_query =
        "SELECT beneficiary_id, status FROM beneficiaries WHERE account_id = ? AND beneficiary_account_id = ? LIMIT 1";
    MYSQL_STMT *chk_stmt = mysql_stmt_init(conn);
    if (!chk_stmt) return BENEFICIARY_ERR_SYSTEM;

    if (mysql_stmt_prepare(chk_stmt, chk_query, (unsigned long)strlen(chk_query)) != 0) {
        mysql_stmt_close(chk_stmt);
        return BENEFICIARY_ERR_SYSTEM;
    }

    MYSQL_BIND b_chk_in[2];
    memset(b_chk_in, 0, sizeof(b_chk_in));
    unsigned long long src_id = source_account_id;
    unsigned long long dst_id = dest_acc.account_id;
    b_chk_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_chk_in[0].buffer = &src_id;
    b_chk_in[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_chk_in[1].buffer = &dst_id;

    if (mysql_stmt_bind_param(chk_stmt, b_chk_in) != 0 || mysql_stmt_execute(chk_stmt) != 0) {
        mysql_stmt_close(chk_stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    unsigned long long existing_id = 0;
    char existing_status[20] = {0};
    unsigned long l_stat = 0;
    MYSQL_BIND b_chk_out[2];
    memset(b_chk_out, 0, sizeof(b_chk_out));
    b_chk_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_chk_out[0].buffer = &existing_id;
    b_chk_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_chk_out[1].buffer = existing_status;
    b_chk_out[1].buffer_length = sizeof(existing_status);
    b_chk_out[1].length = &l_stat;

    if (mysql_stmt_bind_result(chk_stmt, b_chk_out) != 0 || mysql_stmt_store_result(chk_stmt) != 0) {
        mysql_stmt_close(chk_stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    bool row_exists = (mysql_stmt_fetch(chk_stmt) == 0);
    mysql_stmt_free_result(chk_stmt);
    mysql_stmt_close(chk_stmt);

    const char *nick_val = (nickname && nickname[0] != '\0') ? nickname : "";
    unsigned long l_nick = (unsigned long)strlen(nick_val);
    unsigned long l_bname = (unsigned long)strlen(beneficiary_name);

    if (row_exists) {
        if (strcmp(existing_status, "ACTIVE") == 0) {
            return BENEFICIARY_ERR_DUPLICATE;
        }

        /* Reactivate previously inactive beneficiary */
        const char *upd_query =
            "UPDATE beneficiaries SET status = 'ACTIVE', nickname = ?, beneficiary_name = ? WHERE beneficiary_id = ?";
        MYSQL_STMT *upd_stmt = mysql_stmt_init(conn);
        if (!upd_stmt) return BENEFICIARY_ERR_SYSTEM;
        if (mysql_stmt_prepare(upd_stmt, upd_query, (unsigned long)strlen(upd_query)) != 0) {
            mysql_stmt_close(upd_stmt);
            return BENEFICIARY_ERR_SYSTEM;
        }

        MYSQL_BIND b_upd[3];
        memset(b_upd, 0, sizeof(b_upd));
        b_upd[0].buffer_type = MYSQL_TYPE_STRING;
        b_upd[0].buffer = (char *)nick_val;
        b_upd[0].length = &l_nick;
        b_upd[1].buffer_type = MYSQL_TYPE_STRING;
        b_upd[1].buffer = beneficiary_name;
        b_upd[1].length = &l_bname;
        b_upd[2].buffer_type = MYSQL_TYPE_LONGLONG;
        b_upd[2].buffer = &existing_id;

        if (mysql_stmt_bind_param(upd_stmt, b_upd) != 0 || mysql_stmt_execute(upd_stmt) != 0) {
            mysql_stmt_close(upd_stmt);
            return BENEFICIARY_ERR_DATABASE;
        }
        mysql_stmt_close(upd_stmt);

        if (out_beneficiary_id) *out_beneficiary_id = existing_id;
        return BENEFICIARY_SUCCESS;
    }

    /* 6. Insert new beneficiary */
    const char *ins_query =
        "INSERT INTO beneficiaries (account_id, beneficiary_account_id, beneficiary_name, nickname, status) "
        "VALUES (?, ?, ?, ?, 'ACTIVE')";
    MYSQL_STMT *ins_stmt = mysql_stmt_init(conn);
    if (!ins_stmt) return BENEFICIARY_ERR_SYSTEM;
    if (mysql_stmt_prepare(ins_stmt, ins_query, (unsigned long)strlen(ins_query)) != 0) {
        mysql_stmt_close(ins_stmt);
        return BENEFICIARY_ERR_SYSTEM;
    }

    MYSQL_BIND b_ins[4];
    memset(b_ins, 0, sizeof(b_ins));
    b_ins[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_ins[0].buffer = &src_id;
    b_ins[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_ins[1].buffer = &dst_id;
    b_ins[2].buffer_type = MYSQL_TYPE_STRING;
    b_ins[2].buffer = beneficiary_name;
    b_ins[2].length = &l_bname;
    b_ins[3].buffer_type = MYSQL_TYPE_STRING;
    b_ins[3].buffer = (char *)nick_val;
    b_ins[3].length = &l_nick;

    if (mysql_stmt_bind_param(ins_stmt, b_ins) != 0 || mysql_stmt_execute(ins_stmt) != 0) {
        mysql_stmt_close(ins_stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    uint64_t new_id = (uint64_t)mysql_stmt_insert_id(ins_stmt);
    mysql_stmt_close(ins_stmt);

    if (out_beneficiary_id) *out_beneficiary_id = new_id;
    return BENEFICIARY_SUCCESS;
}

bool beneficiary_get_list(uint64_t source_account_id, BeneficiaryList *list)
{
    if (!list) return false;
    memset(list, 0, sizeof(BeneficiaryList));

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "SELECT b.beneficiary_id, b.account_id, b.beneficiary_account_id, "
        "b.beneficiary_name, COALESCE(b.nickname, ''), a.account_number, b.status "
        "FROM beneficiaries b "
        "JOIN accounts a ON b.beneficiary_account_id = a.account_id "
        "WHERE b.account_id = ? AND b.status = 'ACTIVE' "
        "ORDER BY b.beneficiary_id ASC";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long src_id = source_account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &src_id;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long long bid = 0, aid = 0, baid = 0;
    char bname[100] = {0};
    char nick[50] = {0};
    char acc_num[35] = {0};
    char status[16] = {0};
    unsigned long l_bname = 0, l_nick = 0, l_num = 0, l_stat = 0;

    MYSQL_BIND b_out[7];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &bid;
    b_out[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[1].buffer = &aid;
    b_out[2].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[2].buffer = &baid;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = bname;
    b_out[3].buffer_length = sizeof(bname);
    b_out[3].length = &l_bname;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = nick;
    b_out[4].buffer_length = sizeof(nick);
    b_out[4].length = &l_nick;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = acc_num;
    b_out[5].buffer_length = sizeof(acc_num);
    b_out[5].length = &l_num;

    b_out[6].buffer_type = MYSQL_TYPE_STRING;
    b_out[6].buffer = status;
    b_out[6].buffer_length = sizeof(status);
    b_out[6].length = &l_stat;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0 && list->count < BENEFICIARY_MAX_LIST) {
        BeneficiaryRecord *rec = &list->items[list->count++];
        rec->beneficiary_id = bid;
        rec->account_id = aid;
        rec->beneficiary_account_id = baid;
        strncpy(rec->beneficiary_name, bname, sizeof(rec->beneficiary_name) - 1);
        strncpy(rec->nickname, nick, sizeof(rec->nickname) - 1);
        strncpy(rec->beneficiary_account_number, acc_num, sizeof(rec->beneficiary_account_number) - 1);
        strncpy(rec->status, status, sizeof(rec->status) - 1);
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

BeneficiaryResult beneficiary_remove(uint64_t source_account_id, uint64_t beneficiary_id)
{
    MYSQL *conn = db_get_connection();
    if (!conn) return BENEFICIARY_ERR_SYSTEM;

    /* Verify ownership and existence */
    const char *chk_query =
        "SELECT beneficiary_id FROM beneficiaries WHERE beneficiary_id = ? AND account_id = ? LIMIT 1";
    MYSQL_STMT *chk_stmt = mysql_stmt_init(conn);
    if (!chk_stmt) return BENEFICIARY_ERR_SYSTEM;

    if (mysql_stmt_prepare(chk_stmt, chk_query, (unsigned long)strlen(chk_query)) != 0) {
        mysql_stmt_close(chk_stmt);
        return BENEFICIARY_ERR_SYSTEM;
    }

    MYSQL_BIND b_in[2];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long bid = beneficiary_id;
    unsigned long long aid = source_account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &bid;
    b_in[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[1].buffer = &aid;

    if (mysql_stmt_bind_param(chk_stmt, b_in) != 0 || mysql_stmt_execute(chk_stmt) != 0) {
        mysql_stmt_close(chk_stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    if (mysql_stmt_store_result(chk_stmt) != 0) {
        mysql_stmt_close(chk_stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    if (mysql_stmt_fetch(chk_stmt) != 0) {
        mysql_stmt_free_result(chk_stmt);
        mysql_stmt_close(chk_stmt);
        return BENEFICIARY_ERR_UNAUTHORIZED;
    }
    mysql_stmt_free_result(chk_stmt);
    mysql_stmt_close(chk_stmt);

    /* Remove record */
    const char *del_query = "DELETE FROM beneficiaries WHERE beneficiary_id = ? AND account_id = ?";
    MYSQL_STMT *del_stmt = mysql_stmt_init(conn);
    if (!del_stmt) return BENEFICIARY_ERR_SYSTEM;

    if (mysql_stmt_prepare(del_stmt, del_query, (unsigned long)strlen(del_query)) != 0) {
        mysql_stmt_close(del_stmt);
        return BENEFICIARY_ERR_SYSTEM;
    }

    if (mysql_stmt_bind_param(del_stmt, b_in) != 0 || mysql_stmt_execute(del_stmt) != 0) {
        mysql_stmt_close(del_stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    mysql_stmt_close(del_stmt);
    return BENEFICIARY_SUCCESS;
}

BeneficiaryResult beneficiary_get_by_id(uint64_t source_account_id,
                                        uint64_t beneficiary_id,
                                        BeneficiaryRecord *record)
{
    if (!record) return BENEFICIARY_ERR_SYSTEM;
    memset(record, 0, sizeof(BeneficiaryRecord));

    MYSQL *conn = db_get_connection();
    if (!conn) return BENEFICIARY_ERR_SYSTEM;

    const char *query =
        "SELECT b.beneficiary_id, b.account_id, b.beneficiary_account_id, "
        "b.beneficiary_name, COALESCE(b.nickname, ''), a.account_number, b.status "
        "FROM beneficiaries b "
        "JOIN accounts a ON b.beneficiary_account_id = a.account_id "
        "WHERE b.beneficiary_id = ? AND b.account_id = ? AND b.status = 'ACTIVE' LIMIT 1";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return BENEFICIARY_ERR_SYSTEM;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return BENEFICIARY_ERR_SYSTEM;
    }

    MYSQL_BIND b_in[2];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long bid = beneficiary_id;
    unsigned long long aid = source_account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &bid;
    b_in[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[1].buffer = &aid;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    unsigned long long s_bid = 0, s_aid = 0, s_baid = 0;
    char s_bname[100] = {0}, s_nick[50] = {0}, s_num[35] = {0}, s_stat[16] = {0};
    unsigned long l_bname = 0, l_nick = 0, l_num = 0, l_stat = 0;

    MYSQL_BIND b_out[7];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &s_bid;
    b_out[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[1].buffer = &s_aid;
    b_out[2].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[2].buffer = &s_baid;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = s_bname;
    b_out[3].buffer_length = sizeof(s_bname);
    b_out[3].length = &l_bname;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = s_nick;
    b_out[4].buffer_length = sizeof(s_nick);
    b_out[4].length = &l_nick;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = s_num;
    b_out[5].buffer_length = sizeof(s_num);
    b_out[5].length = &l_num;

    b_out[6].buffer_type = MYSQL_TYPE_STRING;
    b_out[6].buffer = s_stat;
    b_out[6].buffer_length = sizeof(s_stat);
    b_out[6].length = &l_stat;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return BENEFICIARY_ERR_DATABASE;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return BENEFICIARY_ERR_NOT_FOUND;
    }

    record->beneficiary_id = s_bid;
    record->account_id = s_aid;
    record->beneficiary_account_id = s_baid;
    strncpy(record->beneficiary_name, s_bname, sizeof(record->beneficiary_name) - 1);
    strncpy(record->nickname, s_nick, sizeof(record->nickname) - 1);
    strncpy(record->beneficiary_account_number, s_num, sizeof(record->beneficiary_account_number) - 1);
    strncpy(record->status, s_stat, sizeof(record->status) - 1);

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return BENEFICIARY_SUCCESS;
}
