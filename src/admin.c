#include "admin.h"
#include "database.h"
#include "security.h"
#include "validation.h"
#include "card.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char *admin_role_to_string(AdminRole role)
{
    switch (role) {
        case ADMIN_ROLE_SUPER_ADMIN: return "SUPER_ADMIN";
        case ADMIN_ROLE_ADMIN:       return "ADMIN";
        case ADMIN_ROLE_OPERATOR:    return "OPERATOR";
        default:                     return "UNKNOWN";
    }
}

AdminRole admin_role_from_string(const char *role_str)
{
    if (!role_str) return ADMIN_ROLE_UNKNOWN;
    if (strcmp(role_str, "SUPER_ADMIN") == 0) return ADMIN_ROLE_SUPER_ADMIN;
    if (strcmp(role_str, "ADMIN") == 0)       return ADMIN_ROLE_ADMIN;
    if (strcmp(role_str, "OPERATOR") == 0)    return ADMIN_ROLE_OPERATOR;
    return ADMIN_ROLE_UNKNOWN;
}

const char *admin_status_to_string(AdminStatus status)
{
    switch (status) {
        case ADMIN_STATUS_ACTIVE:    return "ACTIVE";
        case ADMIN_STATUS_INACTIVE:  return "INACTIVE";
        case ADMIN_STATUS_SUSPENDED: return "SUSPENDED";
        default:                     return "UNKNOWN";
    }
}

AdminStatus admin_status_from_string(const char *status_str)
{
    if (!status_str) return ADMIN_STATUS_UNKNOWN;
    if (strcmp(status_str, "ACTIVE") == 0)    return ADMIN_STATUS_ACTIVE;
    if (strcmp(status_str, "INACTIVE") == 0)  return ADMIN_STATUS_INACTIVE;
    if (strcmp(status_str, "SUSPENDED") == 0) return ADMIN_STATUS_SUSPENDED;
    return ADMIN_STATUS_UNKNOWN;
}

const char *admin_auth_result_to_message(AdminAuthResult result)
{
    switch (result) {
        case ADMIN_AUTH_SUCCESS:
            return "Administrator authentication successful.";
        case ADMIN_AUTH_ERR_INVALID_CREDENTIALS:
            return "Invalid admin username or password.";
        case ADMIN_AUTH_ERR_ACCOUNT_INACTIVE:
            return "Administrator account is inactive. Access denied.";
        case ADMIN_AUTH_ERR_ACCOUNT_SUSPENDED:
            return "Administrator account is suspended. Access denied.";
        case ADMIN_AUTH_ERR_DB_FAILURE:
            return "Database operation error during administrator authentication.";
        default:
            return "Unknown administrator authentication error.";
    }
}

const char *admin_card_op_result_to_message(AdminCardOpResult result)
{
    switch (result) {
        case ADMIN_CARD_OP_SUCCESS:
            return "Card status updated successfully.";
        case ADMIN_CARD_OP_ERR_UNAUTHENTICATED:
            return "Access denied: Administrator authentication required.";
        case ADMIN_CARD_OP_ERR_NOT_FOUND:
            return "Card ID not found in database.";
        case ADMIN_CARD_OP_ERR_ALREADY_BLOCKED:
            return "Card is already in BLOCKED status.";
        case ADMIN_CARD_OP_ERR_ALREADY_ACTIVE:
            return "Card is already in ACTIVE status.";
        case ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_EXPIRED:
            return "Operation rejected: EXPIRED card cannot be activated.";
        case ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_CANCELLED:
            return "Operation rejected: CANCELLED card cannot be activated.";
        case ADMIN_CARD_OP_ERR_DB_FAILURE:
            return "Database failure while updating card status.";
        default:
            return "Unknown card operation error.";
    }
}

const char *admin_refill_result_to_message(AdminRefillResult result)
{
    switch (result) {
        case ADMIN_REFILL_SUCCESS:
            return "ATM cash inventory refilled successfully.";
        case ADMIN_REFILL_ERR_UNAUTHENTICATED:
            return "Access denied: Administrator authentication required.";
        case ADMIN_REFILL_ERR_ZERO_TOTAL:
            return "Refill rejected: At least one denomination quantity must be greater than zero.";
        case ADMIN_REFILL_ERR_OVERFLOW:
            return "Refill rejected: Quantity exceeds maximum allowable capacity.";
        case ADMIN_REFILL_ERR_DB_FAILURE:
            return "Database failure occurred during ATM cash refill.";
        default:
            return "Unknown ATM refill error.";
    }
}

AdminAuthResult admin_authenticate(const char *username, const char *password, AdminSession *session)
{
    if (session) {
        security_secure_zero(session, sizeof(AdminSession));
    }

    if (!username || !password || !session) {
        return ADMIN_AUTH_ERR_INVALID_CREDENTIALS;
    }

    char clean_user[64];
    strncpy(clean_user, username, sizeof(clean_user) - 1);
    clean_user[sizeof(clean_user) - 1] = '\0';
    validation_trim(clean_user);

    if (clean_user[0] == '\0') {
        return ADMIN_AUTH_ERR_INVALID_CREDENTIALS;
    }

    MYSQL *conn = db_get_connection();
    if (!conn) {
        return ADMIN_AUTH_ERR_DB_FAILURE;
    }

    const char *query =
        "SELECT admin_id, username, password_hash, full_name, role, status "
        "FROM admins WHERE username = ? LIMIT 1";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        return ADMIN_AUTH_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return ADMIN_AUTH_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long l_user = (unsigned long)strlen(clean_user);
    b_in[0].buffer_type = MYSQL_TYPE_STRING;
    b_in[0].buffer = clean_user;
    b_in[0].length = &l_user;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return ADMIN_AUTH_ERR_DB_FAILURE;
    }

    unsigned long long db_id = 0;
    char db_username[51] = {0};
    char db_hash[65] = {0};
    char db_name[101] = {0};
    char db_role[20] = {0};
    char db_status[20] = {0};

    unsigned long l_db_user = 0, l_db_hash = 0, l_db_name = 0, l_db_role = 0, l_db_status = 0;

    MYSQL_BIND b_out[6];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &db_id;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = db_username;
    b_out[1].buffer_length = sizeof(db_username);
    b_out[1].length = &l_db_user;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = db_hash;
    b_out[2].buffer_length = sizeof(db_hash);
    b_out[2].length = &l_db_hash;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = db_name;
    b_out[3].buffer_length = sizeof(db_name);
    b_out[3].length = &l_db_name;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = db_role;
    b_out[4].buffer_length = sizeof(db_role);
    b_out[4].length = &l_db_role;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = db_status;
    b_out[5].buffer_length = sizeof(db_status);
    b_out[5].length = &l_db_status;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return ADMIN_AUTH_ERR_DB_FAILURE;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        /* Unknown username produces generic invalid credentials */
        return ADMIN_AUTH_ERR_INVALID_CREDENTIALS;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);

    AdminStatus st = admin_status_from_string(db_status);
    if (st == ADMIN_STATUS_INACTIVE) {
        security_secure_zero(db_hash, sizeof(db_hash));
        return ADMIN_AUTH_ERR_ACCOUNT_INACTIVE;
    }
    if (st == ADMIN_STATUS_SUSPENDED) {
        security_secure_zero(db_hash, sizeof(db_hash));
        return ADMIN_AUTH_ERR_ACCOUNT_SUSPENDED;
    }

    /* Hash entered password and compare in constant time */
    char entered_hash[65] = {0};
    if (!security_hash_sha256(password, entered_hash, sizeof(entered_hash))) {
        security_secure_zero(db_hash, sizeof(db_hash));
        return ADMIN_AUTH_ERR_DB_FAILURE;
    }

    bool match = security_constant_time_compare(entered_hash, db_hash);
    security_secure_zero(entered_hash, sizeof(entered_hash));
    security_secure_zero(db_hash, sizeof(db_hash));

    if (!match) {
        return ADMIN_AUTH_ERR_INVALID_CREDENTIALS;
    }

    /* Update last_login_at timestamp */
    const char *upd_query = "UPDATE admins SET last_login_at = CURRENT_TIMESTAMP WHERE admin_id = ?";
    MYSQL_STMT *upd_stmt = mysql_stmt_init(conn);
    if (upd_stmt) {
        if (mysql_stmt_prepare(upd_stmt, upd_query, (unsigned long)strlen(upd_query)) == 0) {
            MYSQL_BIND b_u[1];
            memset(b_u, 0, sizeof(b_u));
            unsigned long long aid = db_id;
            b_u[0].buffer_type = MYSQL_TYPE_LONGLONG;
            b_u[0].buffer = &aid;
            if (mysql_stmt_bind_param(upd_stmt, b_u) == 0) {
                mysql_stmt_execute(upd_stmt);
            }
        }
        mysql_stmt_close(upd_stmt);
    }

    /* Populate AdminSession */
    session->admin_id = db_id;
    strncpy(session->username, db_username, sizeof(session->username));
    session->username[sizeof(session->username) - 1] = '\0';
    strncpy(session->full_name, db_name, sizeof(session->full_name));
    session->full_name[sizeof(session->full_name) - 1] = '\0';
    session->role = admin_role_from_string(db_role);
    session->status = st;
    session->is_authenticated = true;

    return ADMIN_AUTH_SUCCESS;
}

void admin_logout(AdminSession *session)
{
    if (session) {
        security_secure_zero(session, sizeof(AdminSession));
    }
}

bool admin_get_customers(const AdminSession *session, AdminCustomerList *list)
{
    if (!session || !session->is_authenticated || !list) {
        return false;
    }
    memset(list, 0, sizeof(AdminCustomerList));

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "SELECT customer_id, customer_number, full_name, email, phone, status, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM customers ORDER BY customer_id ASC LIMIT 50";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0 ||
        mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long long cid = 0;
    char cnum[33] = {0}, name[101] = {0}, email[101] = {0}, phone[21] = {0}, status[16] = {0}, dt[32] = {0};
    unsigned long l_cnum = 0, l_name = 0, l_email = 0, l_phone = 0, l_stat = 0, l_dt = 0;

    MYSQL_BIND b_out[7];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &cid;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = cnum;
    b_out[1].buffer_length = sizeof(cnum);
    b_out[1].length = &l_cnum;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = name;
    b_out[2].buffer_length = sizeof(name);
    b_out[2].length = &l_name;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = email;
    b_out[3].buffer_length = sizeof(email);
    b_out[3].length = &l_email;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = phone;
    b_out[4].buffer_length = sizeof(phone);
    b_out[4].length = &l_phone;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = status;
    b_out[5].buffer_length = sizeof(status);
    b_out[5].length = &l_stat;

    b_out[6].buffer_type = MYSQL_TYPE_STRING;
    b_out[6].buffer = dt;
    b_out[6].buffer_length = sizeof(dt);
    b_out[6].length = &l_dt;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0 && list->count < ADMIN_MAX_CUSTOMERS) {
        AdminCustomerItem *item = &list->items[list->count++];
        item->customer_id = cid;
        snprintf(item->customer_number, sizeof(item->customer_number), "%s", cnum);
        snprintf(item->full_name, sizeof(item->full_name), "%s", name);
        snprintf(item->email, sizeof(item->email), "%s", email);
        snprintf(item->phone, sizeof(item->phone), "%s", phone);
        snprintf(item->status, sizeof(item->status), "%s", status);
        snprintf(item->created_at, sizeof(item->created_at), "%s", dt);
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

bool admin_get_customer_accounts(const AdminSession *session, uint64_t customer_id, AdminAccountList *list)
{
    if (!session || !session->is_authenticated || !list) {
        return false;
    }
    memset(list, 0, sizeof(AdminAccountList));

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "SELECT account_id, customer_id, account_number, account_type, CAST(balance AS CHAR), status "
        "FROM accounts WHERE customer_id = ? ORDER BY account_id ASC LIMIT 20";

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

    unsigned long long aid = 0, cust_id = 0;
    char anum[35] = {0}, atype[16] = {0}, bal[32] = {0}, stat[16] = {0};
    unsigned long l_anum = 0, l_atype = 0, l_bal = 0, l_stat = 0;

    MYSQL_BIND b_out[6];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &aid;

    b_out[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[1].buffer = &cust_id;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = anum;
    b_out[2].buffer_length = sizeof(anum);
    b_out[2].length = &l_anum;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = atype;
    b_out[3].buffer_length = sizeof(atype);
    b_out[3].length = &l_atype;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = bal;
    b_out[4].buffer_length = sizeof(bal);
    b_out[4].length = &l_bal;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = stat;
    b_out[5].buffer_length = sizeof(stat);
    b_out[5].length = &l_stat;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0 && list->count < ADMIN_MAX_ACCOUNTS) {
        AdminAccountItem *item = &list->items[list->count++];
        item->account_id = aid;
        item->customer_id = cust_id;
        snprintf(item->account_number, sizeof(item->account_number), "%s", anum);
        snprintf(item->account_type, sizeof(item->account_type), "%s", atype);
        snprintf(item->balance, sizeof(item->balance), "%s", bal);
        snprintf(item->status, sizeof(item->status), "%s", stat);
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

bool admin_get_account_cards(const AdminSession *session, uint64_t account_id, AdminCardList *list)
{
    if (!session || !session->is_authenticated || !list) {
        return false;
    }
    memset(list, 0, sizeof(AdminCardList));

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "SELECT card_id, account_id, card_number, card_status, expiry_date, failed_pin_attempts "
        "FROM cards WHERE account_id = ? ORDER BY card_id ASC LIMIT 20";

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

    unsigned long long card_id = 0, acc_id = 0;
    char cnum[20] = {0}, cstatus[16] = {0}, exp[12] = {0};
    uint8_t failed_attempts = 0;
    unsigned long l_cnum = 0, l_cstatus = 0, l_exp = 0;

    MYSQL_BIND b_out[6];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &card_id;

    b_out[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[1].buffer = &acc_id;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = cnum;
    b_out[2].buffer_length = sizeof(cnum);
    b_out[2].length = &l_cnum;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = cstatus;
    b_out[3].buffer_length = sizeof(cstatus);
    b_out[3].length = &l_cstatus;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = exp;
    b_out[4].buffer_length = sizeof(exp);
    b_out[4].length = &l_exp;

    b_out[5].buffer_type = MYSQL_TYPE_TINY;
    b_out[5].buffer = &failed_attempts;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0 && list->count < ADMIN_MAX_CARDS) {
        AdminCardItem *item = &list->items[list->count++];
        item->card_id = card_id;
        item->account_id = acc_id;
        snprintf(item->card_number, sizeof(item->card_number), "%s", cnum);
        snprintf(item->status, sizeof(item->status), "%s", cstatus);
        snprintf(item->expiry_date, sizeof(item->expiry_date), "%s", exp);
        item->failed_pin_attempts = failed_attempts;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

AdminCardOpResult admin_block_card(const AdminSession *session, uint64_t card_id)
{
    if (!session || !session->is_authenticated) {
        return ADMIN_CARD_OP_ERR_UNAUTHENTICATED;
    }

    MYSQL *conn = db_get_connection();
    if (!conn) return ADMIN_CARD_OP_ERR_DB_FAILURE;

    if (!db_transaction_begin()) {
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    /* Query current card status with row locking */
    const char *sel_query = "SELECT card_status FROM cards WHERE card_id = ? FOR UPDATE";
    MYSQL_STMT *sel_stmt = mysql_stmt_init(conn);
    if (!sel_stmt) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(sel_stmt, sel_query, (unsigned long)strlen(sel_query)) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long cid = card_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &cid;

    if (mysql_stmt_bind_param(sel_stmt, b_in) != 0 || mysql_stmt_execute(sel_stmt) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    char cur_status[20] = {0};
    unsigned long l_stat = 0;
    MYSQL_BIND b_out[1];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = cur_status;
    b_out[0].buffer_length = sizeof(cur_status);
    b_out[0].length = &l_stat;

    if (mysql_stmt_bind_result(sel_stmt, b_out) != 0 || mysql_stmt_store_result(sel_stmt) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    if (mysql_stmt_fetch(sel_stmt) != 0) {
        mysql_stmt_free_result(sel_stmt);
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_NOT_FOUND;
    }

    mysql_stmt_free_result(sel_stmt);
    mysql_stmt_close(sel_stmt);

    if (strcmp(cur_status, "BLOCKED") == 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_ALREADY_BLOCKED;
    }
    if (strcmp(cur_status, "EXPIRED") == 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_EXPIRED;
    }
    if (strcmp(cur_status, "CANCELLED") == 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_CANCELLED;
    }
    if (strcmp(cur_status, "ACTIVE") != 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    const char *upd_query = "UPDATE cards SET card_status = 'BLOCKED' WHERE card_id = ?";
    MYSQL_STMT *upd_stmt = mysql_stmt_init(conn);
    if (!upd_stmt) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(upd_stmt, upd_query, (unsigned long)strlen(upd_query)) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_u[1];
    memset(b_u, 0, sizeof(b_u));
    b_u[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_u[0].buffer = &cid;

    if (mysql_stmt_bind_param(upd_stmt, b_u) != 0 || mysql_stmt_execute(upd_stmt) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    mysql_stmt_close(upd_stmt);

    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    return ADMIN_CARD_OP_SUCCESS;
}

AdminCardOpResult admin_unblock_card(const AdminSession *session, uint64_t card_id)
{
    if (!session || !session->is_authenticated) {
        return ADMIN_CARD_OP_ERR_UNAUTHENTICATED;
    }

    MYSQL *conn = db_get_connection();
    if (!conn) return ADMIN_CARD_OP_ERR_DB_FAILURE;

    if (!db_transaction_begin()) {
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    /* Query current card status with row locking */
    const char *sel_query = "SELECT card_status FROM cards WHERE card_id = ? FOR UPDATE";
    MYSQL_STMT *sel_stmt = mysql_stmt_init(conn);
    if (!sel_stmt) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(sel_stmt, sel_query, (unsigned long)strlen(sel_query)) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long cid = card_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &cid;

    if (mysql_stmt_bind_param(sel_stmt, b_in) != 0 || mysql_stmt_execute(sel_stmt) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    char cur_status[20] = {0};
    unsigned long l_stat = 0;
    MYSQL_BIND b_out[1];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = cur_status;
    b_out[0].buffer_length = sizeof(cur_status);
    b_out[0].length = &l_stat;

    if (mysql_stmt_bind_result(sel_stmt, b_out) != 0 || mysql_stmt_store_result(sel_stmt) != 0) {
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    if (mysql_stmt_fetch(sel_stmt) != 0) {
        mysql_stmt_free_result(sel_stmt);
        mysql_stmt_close(sel_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_NOT_FOUND;
    }

    mysql_stmt_free_result(sel_stmt);
    mysql_stmt_close(sel_stmt);

    if (strcmp(cur_status, "ACTIVE") == 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_ALREADY_ACTIVE;
    }
    if (strcmp(cur_status, "EXPIRED") == 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_EXPIRED;
    }
    if (strcmp(cur_status, "CANCELLED") == 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_CANCELLED;
    }
    if (strcmp(cur_status, "BLOCKED") != 0) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    /* Unblock card: set status to ACTIVE and reset failed attempts to 0 */
    const char *upd_query = "UPDATE cards SET card_status = 'ACTIVE', failed_pin_attempts = 0 WHERE card_id = ?";
    MYSQL_STMT *upd_stmt = mysql_stmt_init(conn);
    if (!upd_stmt) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(upd_stmt, upd_query, (unsigned long)strlen(upd_query)) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_u[1];
    memset(b_u, 0, sizeof(b_u));
    b_u[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_u[0].buffer = &cid;

    if (mysql_stmt_bind_param(upd_stmt, b_u) != 0 || mysql_stmt_execute(upd_stmt) != 0) {
        mysql_stmt_close(upd_stmt);
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    mysql_stmt_close(upd_stmt);

    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return ADMIN_CARD_OP_ERR_DB_FAILURE;
    }

    return ADMIN_CARD_OP_SUCCESS;
}

bool admin_get_transactions(const AdminSession *session,
                            uint64_t account_id_filter,
                            const char *type_filter,
                            const char *status_filter,
                            unsigned int limit,
                            AdminTransactionList *list)
{
    if (!session || !session->is_authenticated || !list) {
        return false;
    }
    memset(list, 0, sizeof(AdminTransactionList));

    if (limit == 0) limit = 15;
    if (limit > 50) limit = 50;

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query =
        "SELECT t.transaction_id, t.transaction_reference, t.account_id, a.account_number, "
        "t.transaction_type, CAST(t.amount AS CHAR), CAST(t.balance_before AS CHAR), "
        "CAST(t.balance_after AS CHAR), t.related_account_id, t.atm_id, "
        "t.transaction_status, COALESCE(t.description, ''), DATE_FORMAT(t.created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM transactions t "
        "JOIN accounts a ON t.account_id = a.account_id "
        "WHERE (? = 0 OR t.account_id = ?) "
        "  AND (? = '' OR t.transaction_type = ?) "
        "  AND (? = '' OR t.transaction_status = ?) "
        "ORDER BY t.created_at DESC, t.transaction_id DESC "
        "LIMIT ?";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long long aid_param1 = account_id_filter;
    unsigned long long aid_param2 = account_id_filter;
    const char *t_filt = type_filter ? type_filter : "";
    const char *s_filt = status_filter ? status_filter : "";
    unsigned long l_t1 = (unsigned long)strlen(t_filt);
    unsigned long l_t2 = (unsigned long)strlen(t_filt);
    unsigned long l_s1 = (unsigned long)strlen(s_filt);
    unsigned long l_s2 = (unsigned long)strlen(s_filt);
    unsigned long long lim_param = limit;

    MYSQL_BIND b_in[7];
    memset(b_in, 0, sizeof(b_in));

    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid_param1;

    b_in[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[1].buffer = &aid_param2;

    b_in[2].buffer_type = MYSQL_TYPE_STRING;
    b_in[2].buffer = (char *)t_filt;
    b_in[2].length = &l_t1;

    b_in[3].buffer_type = MYSQL_TYPE_STRING;
    b_in[3].buffer = (char *)t_filt;
    b_in[3].length = &l_t2;

    b_in[4].buffer_type = MYSQL_TYPE_STRING;
    b_in[4].buffer = (char *)s_filt;
    b_in[4].length = &l_s1;

    b_in[5].buffer_type = MYSQL_TYPE_STRING;
    b_in[5].buffer = (char *)s_filt;
    b_in[5].length = &l_s2;

    b_in[6].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[6].buffer = &lim_param;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long long tid = 0, aid = 0, rel_aid = 0, atmid = 0;
    char tref[36] = {0}, anum[35] = {0}, ttype[20] = {0}, amt[32] = {0}, bbef[32] = {0}, baft[32] = {0};
    char tstat[16] = {0}, desc[255] = {0}, dt[32] = {0};
    bool is_rel_null = false, is_atm_null = false;

    unsigned long l_ref = 0, l_anum = 0, l_type = 0, l_amt = 0, l_bbef = 0, l_baft = 0;
    unsigned long l_stat = 0, l_desc = 0, l_dt = 0;

    MYSQL_BIND b_out[13];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &tid;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = tref;
    b_out[1].buffer_length = sizeof(tref);
    b_out[1].length = &l_ref;

    b_out[2].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[2].buffer = &aid;

    b_out[3].buffer_type = MYSQL_TYPE_STRING;
    b_out[3].buffer = anum;
    b_out[3].buffer_length = sizeof(anum);
    b_out[3].length = &l_anum;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = ttype;
    b_out[4].buffer_length = sizeof(ttype);
    b_out[4].length = &l_type;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = amt;
    b_out[5].buffer_length = sizeof(amt);
    b_out[5].length = &l_amt;

    b_out[6].buffer_type = MYSQL_TYPE_STRING;
    b_out[6].buffer = bbef;
    b_out[6].buffer_length = sizeof(bbef);
    b_out[6].length = &l_bbef;

    b_out[7].buffer_type = MYSQL_TYPE_STRING;
    b_out[7].buffer = baft;
    b_out[7].buffer_length = sizeof(baft);
    b_out[7].length = &l_baft;

    b_out[8].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[8].buffer = &rel_aid;
    b_out[8].is_null = (bool *)&is_rel_null;

    b_out[9].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[9].buffer = &atmid;
    b_out[9].is_null = (bool *)&is_atm_null;

    b_out[10].buffer_type = MYSQL_TYPE_STRING;
    b_out[10].buffer = tstat;
    b_out[10].buffer_length = sizeof(tstat);
    b_out[10].length = &l_stat;

    b_out[11].buffer_type = MYSQL_TYPE_STRING;
    b_out[11].buffer = desc;
    b_out[11].buffer_length = sizeof(desc);
    b_out[11].length = &l_desc;

    b_out[12].buffer_type = MYSQL_TYPE_STRING;
    b_out[12].buffer = dt;
    b_out[12].buffer_length = sizeof(dt);
    b_out[12].length = &l_dt;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0 && list->count < ADMIN_MAX_TRANSACTIONS) {
        AdminTransactionItem *item = &list->items[list->count++];
        item->transaction_id = tid;
        snprintf(item->transaction_reference, sizeof(item->transaction_reference), "%s", tref);
        item->account_id = aid;
        snprintf(item->account_number, sizeof(item->account_number), "%s", anum);
        snprintf(item->transaction_type, sizeof(item->transaction_type), "%s", ttype);
        snprintf(item->amount, sizeof(item->amount), "%s", amt);
        snprintf(item->balance_before, sizeof(item->balance_before), "%s", bbef);
        snprintf(item->balance_after, sizeof(item->balance_after), "%s", baft);
        item->has_related_account = !is_rel_null;
        item->related_account_id = is_rel_null ? 0 : rel_aid;
        item->has_atm_id = !is_atm_null;
        item->atm_id = is_atm_null ? 0 : atmid;
        snprintf(item->status, sizeof(item->status), "%s", tstat);
        snprintf(item->description, sizeof(item->description), "%s", desc);
        snprintf(item->created_at, sizeof(item->created_at), "%s", dt);
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

bool admin_get_cash_status(const AdminSession *session, uint64_t atm_id, AdminAtmCashStatus *status)
{
    if (!session || !session->is_authenticated || !status) {
        return false;
    }
    memset(status, 0, sizeof(AdminAtmCashStatus));

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query = "SELECT denomination, quantity FROM atm_cash WHERE atm_id = ?";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long aid = atm_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned int denom = 0, qty = 0;
    MYSQL_BIND b_out[2];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_LONG;
    b_out[0].buffer = &denom;
    b_out[1].buffer_type = MYSQL_TYPE_LONG;
    b_out[1].buffer = &qty;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        if (denom == 500) status->qty_500 = qty;
        else if (denom == 200) status->qty_200 = qty;
        else if (denom == 100) status->qty_100 = qty;
        else if (denom == 50)  status->qty_50  = qty;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);

    status->total_cash = ((uint64_t)status->qty_500 * 500) +
                         ((uint64_t)status->qty_200 * 200) +
                         ((uint64_t)status->qty_100 * 100) +
                         ((uint64_t)status->qty_50  * 50);

    return true;
}

AdminRefillResult admin_refill_cash(const AdminSession *session, uint64_t atm_id, const AdminAtmRefillInput *input)
{
    if (!session || !session->is_authenticated) {
        return ADMIN_REFILL_ERR_UNAUTHENTICATED;
    }
    if (!input) {
        return ADMIN_REFILL_ERR_ZERO_TOTAL;
    }

    if (input->add_500 == 0 && input->add_200 == 0 && input->add_100 == 0 && input->add_50 == 0) {
        return ADMIN_REFILL_ERR_ZERO_TOTAL;
    }

    /* Overflow check */
    if (input->add_500 > 1000000 || input->add_200 > 1000000 ||
        input->add_100 > 1000000 || input->add_50  > 1000000) {
        return ADMIN_REFILL_ERR_OVERFLOW;
    }

    MYSQL *conn = db_get_connection();
    if (!conn) return ADMIN_REFILL_ERR_DB_FAILURE;

    if (!db_transaction_begin()) {
        return ADMIN_REFILL_ERR_DB_FAILURE;
    }

    const char *upd_cash = "UPDATE atm_cash SET quantity = quantity + ? WHERE atm_id = ? AND denomination = ?";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        db_transaction_rollback();
        return ADMIN_REFILL_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(stmt, upd_cash, (unsigned long)strlen(upd_cash)) != 0) {
        mysql_stmt_close(stmt);
        db_transaction_rollback();
        return ADMIN_REFILL_ERR_DB_FAILURE;
    }

    const uint32_t denoms[4] = {500, 200, 100, 50};
    const uint32_t adds[4]   = {input->add_500, input->add_200, input->add_100, input->add_50};

    for (int i = 0; i < 4; i++) {
        if (adds[i] == 0) continue;

        MYSQL_BIND b[3];
        memset(b, 0, sizeof(b));
        unsigned int add_qty = adds[i];
        unsigned long long aid = atm_id;
        unsigned int d_val = denoms[i];

        b[0].buffer_type = MYSQL_TYPE_LONG;
        b[0].buffer = &add_qty;

        b[1].buffer_type = MYSQL_TYPE_LONGLONG;
        b[1].buffer = &aid;

        b[2].buffer_type = MYSQL_TYPE_LONG;
        b[2].buffer = &d_val;

        if (mysql_stmt_bind_param(stmt, b) != 0 || mysql_stmt_execute(stmt) != 0) {
            mysql_stmt_close(stmt);
            db_transaction_rollback();
            return ADMIN_REFILL_ERR_DB_FAILURE;
        }
    }
    mysql_stmt_close(stmt);

    /* Update atm last_refilled_at */
    const char *upd_atm = "UPDATE atm SET last_refilled_at = CURRENT_TIMESTAMP WHERE atm_id = ?";
    MYSQL_STMT *atm_stmt = mysql_stmt_init(conn);
    if (!atm_stmt) {
        db_transaction_rollback();
        return ADMIN_REFILL_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(atm_stmt, upd_atm, (unsigned long)strlen(upd_atm)) != 0) {
        mysql_stmt_close(atm_stmt);
        db_transaction_rollback();
        return ADMIN_REFILL_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_atm[1];
    memset(b_atm, 0, sizeof(b_atm));
    unsigned long long aid = atm_id;
    b_atm[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_atm[0].buffer = &aid;

    if (mysql_stmt_bind_param(atm_stmt, b_atm) != 0 || mysql_stmt_execute(atm_stmt) != 0) {
        mysql_stmt_close(atm_stmt);
        db_transaction_rollback();
        return ADMIN_REFILL_ERR_DB_FAILURE;
    }
    mysql_stmt_close(atm_stmt);

    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return ADMIN_REFILL_ERR_DB_FAILURE;
    }

    return ADMIN_REFILL_SUCCESS;
}

bool admin_get_statistics(const AdminSession *session, AdminStatistics *stats)
{
    if (!session || !session->is_authenticated || !stats) {
        return false;
    }
    memset(stats, 0, sizeof(AdminStatistics));
    strncpy(stats->deposit_total, "0.00", sizeof(stats->deposit_total) - 1);
    strncpy(stats->withdrawal_total, "0.00", sizeof(stats->withdrawal_total) - 1);
    strncpy(stats->transfer_total, "0.00", sizeof(stats->transfer_total) - 1);

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    /* 1. Customers count */
    if (mysql_query(conn, "SELECT COUNT(*) FROM customers") == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) stats->total_customers = strtoull(row[0], NULL, 10);
            mysql_free_result(res);
        }
    }

    /* 2. Accounts count and active accounts */
    if (mysql_query(conn, "SELECT COUNT(*), COALESCE(SUM(CASE WHEN status = 'ACTIVE' THEN 1 ELSE 0 END), 0) FROM accounts") == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                if (row[0]) stats->total_accounts = strtoull(row[0], NULL, 10);
                if (row[1]) stats->active_accounts = strtoull(row[1], NULL, 10);
            }
            mysql_free_result(res);
        }
    }

    /* 3. Cards count, active, and blocked */
    if (mysql_query(conn, "SELECT COUNT(*), "
                          "COALESCE(SUM(CASE WHEN card_status = 'ACTIVE' THEN 1 ELSE 0 END), 0), "
                          "COALESCE(SUM(CASE WHEN card_status = 'BLOCKED' THEN 1 ELSE 0 END), 0) "
                          "FROM cards") == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                if (row[0]) stats->total_cards = strtoull(row[0], NULL, 10);
                if (row[1]) stats->active_cards = strtoull(row[1], NULL, 10);
                if (row[2]) stats->blocked_cards = strtoull(row[2], NULL, 10);
            }
            mysql_free_result(res);
        }
    }

    /* 4. Total successful transactions */
    if (mysql_query(conn, "SELECT COUNT(*) FROM transactions WHERE transaction_status = 'SUCCESS'") == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) stats->total_successful_txns = strtoull(row[0], NULL, 10);
            mysql_free_result(res);
        }
    }

    /* 5. Breakdown by transaction type */
    if (mysql_query(conn, "SELECT transaction_type, COUNT(*), CAST(COALESCE(SUM(amount), 0.00) AS CHAR) "
                          "FROM transactions WHERE transaction_status = 'SUCCESS' GROUP BY transaction_type") == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                if (!row[0]) continue;
                if (strcmp(row[0], "DEPOSIT") == 0) {
                    if (row[1]) stats->deposit_count = strtoull(row[1], NULL, 10);
                    if (row[2]) strncpy(stats->deposit_total, row[2], sizeof(stats->deposit_total) - 1);
                } else if (strcmp(row[0], "WITHDRAWAL") == 0) {
                    if (row[1]) stats->withdrawal_count = strtoull(row[1], NULL, 10);
                    if (row[2]) strncpy(stats->withdrawal_total, row[2], sizeof(stats->withdrawal_total) - 1);
                } else if (strcmp(row[0], "TRANSFER") == 0) {
                    if (row[1]) stats->transfer_count = strtoull(row[1], NULL, 10);
                    if (row[2]) strncpy(stats->transfer_total, row[2], sizeof(stats->transfer_total) - 1);
                }
            }
            mysql_free_result(res);
        }
    }

    /* 6. Total ATM cash */
    if (mysql_query(conn, "SELECT COALESCE(SUM(denomination * quantity), 0) FROM atm_cash WHERE atm_id = 1") == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) stats->total_atm_cash = strtoull(row[0], NULL, 10);
            mysql_free_result(res);
        }
    }

    return true;
}
