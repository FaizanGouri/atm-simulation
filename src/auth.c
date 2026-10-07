#include "auth.h"
#include "card.h"
#include "security.h"
#include "validation.h"
#include "database.h"
#include "transaction.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

const char *auth_result_to_message(AuthResult result)
{
    switch (result) {
        case AUTH_SUCCESS:
            return "Authentication successful.";
        case AUTH_ERR_INVALID_CARD:
            return "Invalid card. Card number not recognized.";
        case AUTH_ERR_CARD_BLOCKED:
            return "Access denied: This card is BLOCKED. Please contact your bank.";
        case AUTH_ERR_CARD_EXPIRED:
            return "Access denied: This card has EXPIRED.";
        case AUTH_ERR_CARD_CANCELLED:
            return "Access denied: This card is CANCELLED.";
        case AUTH_ERR_WRONG_PIN:
            return "Incorrect PIN.";
        case AUTH_ERR_CARD_NOW_BLOCKED:
            return "Maximum PIN attempts exceeded. Card has now been BLOCKED.";
        case AUTH_ERR_DB_FAILURE:
            return "Database operation error during authentication.";
        default:
            return "Unknown authentication error.";
    }
}

AuthResult auth_authenticate_customer(const char *card_number,
                                      const char *pin,
                                      CustomerSession *session,
                                      uint8_t *attempts_remaining)
{
    if (session) {
        memset(session, 0, sizeof(CustomerSession));
    }
    if (attempts_remaining) {
        *attempts_remaining = 0;
    }

    if (!card_number || !pin) {
        return AUTH_ERR_INVALID_CARD;
    }

    /* 1. Validate card number format */
    char clean_card[32];
    strncpy(clean_card, card_number, sizeof(clean_card) - 1);
    clean_card[sizeof(clean_card) - 1] = '\0';
    validation_trim(clean_card);

    if (!validation_is_valid_card_number(clean_card)) {
        return AUTH_ERR_INVALID_CARD;
    }

    /* 2. Query card from database */
    CardRecord card;
    if (!card_find_by_number(clean_card, &card)) {
        return AUTH_ERR_INVALID_CARD;
    }

    /* 3. Check card status */
    if (card.status == CARD_STATUS_BLOCKED) {
        return AUTH_ERR_CARD_BLOCKED;
    }
    if (card.status == CARD_STATUS_EXPIRED) {
        return AUTH_ERR_CARD_EXPIRED;
    }
    if (card.status == CARD_STATUS_CANCELLED) {
        return AUTH_ERR_CARD_CANCELLED;
    }
    if (card.status != CARD_STATUS_ACTIVE) {
        return AUTH_ERR_INVALID_CARD;
    }

    /* 4. Validate PIN format */
    char clean_pin[16];
    strncpy(clean_pin, pin, sizeof(clean_pin) - 1);
    clean_pin[sizeof(clean_pin) - 1] = '\0';
    validation_trim(clean_pin);

    /* 5. Compute SHA-256 hash of entered PIN */
    char entered_pin_hash[65];
    if (!security_hash_sha256(clean_pin, entered_pin_hash, sizeof(entered_pin_hash))) {
        return AUTH_ERR_DB_FAILURE;
    }

    /* 6. Verify PIN hash in constant time */
    if (!security_constant_time_compare(entered_pin_hash, card.pin_hash)) {
        /* Wrong PIN: increment attempt counter */
        uint8_t updated = 0;
        bool is_blocked = false;

        if (!card_increment_failed_attempts(card.card_id, &updated, &is_blocked)) {
            return AUTH_ERR_DB_FAILURE;
        }

        if (is_blocked) {
            return AUTH_ERR_CARD_NOW_BLOCKED;
        }

        if (attempts_remaining) {
            *attempts_remaining = (3 > updated) ? (3 - updated) : 0;
        }
        return AUTH_ERR_WRONG_PIN;
    }

    /* 7. PIN Correct: reset failed attempts */
    if (!card_reset_failed_attempts(card.card_id)) {
        return AUTH_ERR_DB_FAILURE;
    }

    /* 8. Fetch customer and account details to populate session */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        return AUTH_ERR_DB_FAILURE;
    }

    const char *sess_query =
        "SELECT c.customer_id, c.customer_number, c.full_name, a.account_id, a.account_number, a.account_type "
        "FROM accounts a "
        "JOIN customers c ON a.customer_id = c.customer_id "
        "WHERE a.account_id = ? LIMIT 1";

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        return AUTH_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(stmt, sess_query, (unsigned long)strlen(sess_query)) != 0) {
        mysql_stmt_close(stmt);
        return AUTH_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long aid = card.account_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return AUTH_ERR_DB_FAILURE;
    }

    unsigned long long s_cust_id = 0, s_acc_id = 0;
    char s_cust_num[33] = {0}, s_cust_name[101] = {0}, s_acc_num[35] = {0}, s_acc_type[16] = {0};
    unsigned long l_cnum = 0, l_cname = 0, l_anum = 0, l_atype = 0;

    MYSQL_BIND b_out[6];
    memset(b_out, 0, sizeof(b_out));

    b_out[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[0].buffer = &s_cust_id;

    b_out[1].buffer_type = MYSQL_TYPE_STRING;
    b_out[1].buffer = s_cust_num;
    b_out[1].buffer_length = sizeof(s_cust_num);
    b_out[1].length = &l_cnum;

    b_out[2].buffer_type = MYSQL_TYPE_STRING;
    b_out[2].buffer = s_cust_name;
    b_out[2].buffer_length = sizeof(s_cust_name);
    b_out[2].length = &l_cname;

    b_out[3].buffer_type = MYSQL_TYPE_LONGLONG;
    b_out[3].buffer = &s_acc_id;

    b_out[4].buffer_type = MYSQL_TYPE_STRING;
    b_out[4].buffer = s_acc_num;
    b_out[4].buffer_length = sizeof(s_acc_num);
    b_out[4].length = &l_anum;

    b_out[5].buffer_type = MYSQL_TYPE_STRING;
    b_out[5].buffer = s_acc_type;
    b_out[5].buffer_length = sizeof(s_acc_type);
    b_out[5].length = &l_atype;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return AUTH_ERR_DB_FAILURE;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return AUTH_ERR_DB_FAILURE;
    }

    if (session) {
        session->customer_id = s_cust_id;
        strncpy(session->customer_number, s_cust_num, sizeof(session->customer_number) - 1);
        strncpy(session->customer_name, s_cust_name, sizeof(session->customer_name) - 1);
        session->account_id = s_acc_id;
        strncpy(session->account_number, s_acc_num, sizeof(session->account_number) - 1);
        strncpy(session->account_type, s_acc_type, sizeof(session->account_type) - 1);
        session->card_id = card.card_id;
        strncpy(session->card_number, card.card_number, sizeof(session->card_number) - 1);
        session->is_authenticated = true;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return AUTH_SUCCESS;
}

void auth_logout(CustomerSession *session)
{
    if (session) {
        memset(session, 0, sizeof(CustomerSession));
    }
}

const char *auth_pin_change_result_to_message(PinChangeResult result)
{
    switch (result) {
        case PIN_CHANGE_SUCCESS:
            return "PIN changed successfully.";
        case PIN_CHANGE_ERR_UNAUTHENTICATED:
            return "User session is not authenticated.";
        case PIN_CHANGE_ERR_INCORRECT_CURRENT_PIN:
            return "Current PIN is incorrect.";
        case PIN_CHANGE_ERR_INVALID_NEW_PIN:
            return "New PIN must be exactly 4 digits.";
        case PIN_CHANGE_ERR_CONFIRMATION_MISMATCH:
            return "New PIN and confirmation do not match.";
        case PIN_CHANGE_ERR_SAME_PIN:
            return "New PIN must be different from the current PIN.";
        case PIN_CHANGE_ERR_DB_FAILURE:
            return "Unable to change PIN due to a database error.";
        default:
            return "Unknown PIN change error.";
    }
}

PinChangeResult auth_change_pin(
    CustomerSession *session,
    const char *current_pin,
    const char *new_pin,
    const char *confirm_pin
)
{
    if (!session || !session->is_authenticated || session->card_id == 0) {
        return PIN_CHANGE_ERR_UNAUTHENTICATED;
    }

    if (!current_pin || !new_pin || !confirm_pin) {
        return PIN_CHANGE_ERR_INVALID_NEW_PIN;
    }

    char clean_cur[16] = {0};
    char clean_new[16] = {0};
    char clean_conf[16] = {0};
    char cur_hash[65] = {0};
    char new_hash[65] = {0};
    char db_hash[65] = {0};

    strncpy(clean_cur, current_pin, sizeof(clean_cur) - 1);
    validation_trim(clean_cur);

    strncpy(clean_new, new_pin, sizeof(clean_new) - 1);
    validation_trim(clean_new);

    strncpy(clean_conf, confirm_pin, sizeof(clean_conf) - 1);
    validation_trim(clean_conf);

    /* Validate new PIN format: strictly 4 numeric digits */
    if (strlen(clean_new) != 4 || !validation_is_valid_pin(clean_new)) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_INVALID_NEW_PIN;
    }

    /* Validate new PIN confirmation match */
    if (strcmp(clean_new, clean_conf) != 0) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_CONFIRMATION_MISMATCH;
    }

    /* Validate that new PIN is different from current PIN */
    if (strcmp(clean_new, clean_cur) == 0) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_SAME_PIN;
    }

    /* Retrieve current PIN hash from cards table */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    const char *fetch_query = "SELECT pin_hash FROM cards WHERE card_id = ? LIMIT 1";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    if (mysql_stmt_prepare(stmt, fetch_query, (unsigned long)strlen(fetch_query)) != 0) {
        mysql_stmt_close(stmt);
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long cid = session->card_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &cid;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    unsigned long l_db_hash = 0;
    MYSQL_BIND b_out[1];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_STRING;
    b_out[0].buffer = db_hash;
    b_out[0].buffer_length = sizeof(db_hash);
    b_out[0].length = &l_db_hash;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0 || mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);

    /* Hash current entered PIN */
    if (!security_hash_sha256(clean_cur, cur_hash, sizeof(cur_hash))) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        memset(db_hash, 0, sizeof(db_hash));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    /* Compare current PIN hash in constant time */
    if (!security_constant_time_compare(cur_hash, db_hash)) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        memset(cur_hash, 0, sizeof(cur_hash));
        memset(db_hash, 0, sizeof(db_hash));
        /* Do NOT alter login failure count */
        return PIN_CHANGE_ERR_INCORRECT_CURRENT_PIN;
    }

    /* Hash new PIN */
    if (!security_hash_sha256(clean_new, new_hash, sizeof(new_hash))) {
        memset(clean_cur, 0, sizeof(clean_cur));
        memset(clean_new, 0, sizeof(clean_new));
        memset(clean_conf, 0, sizeof(clean_conf));
        memset(cur_hash, 0, sizeof(cur_hash));
        memset(db_hash, 0, sizeof(db_hash));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    /* Wipe plaintext PINs and temporary current hashes */
    memset(clean_cur, 0, sizeof(clean_cur));
    memset(clean_new, 0, sizeof(clean_new));
    memset(clean_conf, 0, sizeof(clean_conf));
    memset(cur_hash, 0, sizeof(cur_hash));
    memset(db_hash, 0, sizeof(db_hash));

    /* Begin atomic database transaction */
    if (!db_transaction_begin()) {
        memset(new_hash, 0, sizeof(new_hash));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    /* Update card PIN hash and reset failed attempts to 0 */
    if (!card_update_pin(session->card_id, new_hash)) {
        db_transaction_rollback();
        memset(new_hash, 0, sizeof(new_hash));
        return PIN_CHANGE_ERR_DB_FAILURE;
    }
    memset(new_hash, 0, sizeof(new_hash));

    /* Insert PIN_CHANGE transaction audit record */
    char txn_ref[36];
    utils_generate_txn_reference(txn_ref, sizeof(txn_ref));

    TransactionRecordInput txn_in;
    memset(&txn_in, 0, sizeof(txn_in));
    strncpy(txn_in.transaction_reference, txn_ref, sizeof(txn_in.transaction_reference) - 1);
    txn_in.account_id = session->account_id;
    txn_in.type = TXN_TYPE_PIN_CHANGE;
    strncpy(txn_in.amount, "0.00", sizeof(txn_in.amount) - 1);
    strncpy(txn_in.balance_before, "0.00", sizeof(txn_in.balance_before) - 1);
    strncpy(txn_in.balance_after, "0.00", sizeof(txn_in.balance_after) - 1);
    txn_in.related_account_id = NULL;
    txn_in.atm_id = NULL;
    txn_in.status = TXN_STATUS_SUCCESS;
    txn_in.description = "ATM Card PIN Change";

    if (!transaction_record_insert(&txn_in)) {
        db_transaction_rollback();
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    /* Commit transaction */
    if (!db_transaction_commit()) {
        db_transaction_rollback();
        return PIN_CHANGE_ERR_DB_FAILURE;
    }

    return PIN_CHANGE_SUCCESS;
}

