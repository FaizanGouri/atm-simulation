#ifndef ADMIN_H
#define ADMIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ADMIN_MAX_CUSTOMERS 50
#define ADMIN_MAX_ACCOUNTS 20
#define ADMIN_MAX_CARDS 20
#define ADMIN_MAX_TRANSACTIONS 50

typedef enum {
    ADMIN_ROLE_SUPER_ADMIN,
    ADMIN_ROLE_ADMIN,
    ADMIN_ROLE_OPERATOR,
    ADMIN_ROLE_UNKNOWN
} AdminRole;

typedef enum {
    ADMIN_STATUS_ACTIVE,
    ADMIN_STATUS_INACTIVE,
    ADMIN_STATUS_SUSPENDED,
    ADMIN_STATUS_UNKNOWN
} AdminStatus;

typedef struct {
    uint64_t admin_id;
    char username[51];
    char full_name[101];
    AdminRole role;
    AdminStatus status;
    bool is_authenticated;
} AdminSession;

typedef enum {
    ADMIN_AUTH_SUCCESS = 0,
    ADMIN_AUTH_ERR_INVALID_CREDENTIALS,
    ADMIN_AUTH_ERR_ACCOUNT_INACTIVE,
    ADMIN_AUTH_ERR_ACCOUNT_SUSPENDED,
    ADMIN_AUTH_ERR_DB_FAILURE
} AdminAuthResult;

typedef struct {
    uint64_t customer_id;
    char customer_number[33];
    char full_name[101];
    char email[101];
    char phone[21];
    char status[16];
    char created_at[32];
} AdminCustomerItem;

typedef struct {
    AdminCustomerItem items[ADMIN_MAX_CUSTOMERS];
    size_t count;
} AdminCustomerList;

typedef struct {
    uint64_t account_id;
    uint64_t customer_id;
    char account_number[35];
    char account_type[16];
    char balance[32];
    char status[16];
} AdminAccountItem;

typedef struct {
    AdminAccountItem items[ADMIN_MAX_ACCOUNTS];
    size_t count;
} AdminAccountList;

typedef struct {
    uint64_t card_id;
    uint64_t account_id;
    char card_number[20];
    char status[16];
    char expiry_date[12];
    uint8_t failed_pin_attempts;
} AdminCardItem;

typedef struct {
    AdminCardItem items[ADMIN_MAX_CARDS];
    size_t count;
} AdminCardList;

typedef enum {
    ADMIN_CARD_OP_SUCCESS = 0,
    ADMIN_CARD_OP_ERR_UNAUTHENTICATED,
    ADMIN_CARD_OP_ERR_NOT_FOUND,
    ADMIN_CARD_OP_ERR_ALREADY_BLOCKED,
    ADMIN_CARD_OP_ERR_ALREADY_ACTIVE,
    ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_EXPIRED,
    ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_CANCELLED,
    ADMIN_CARD_OP_ERR_DB_FAILURE
} AdminCardOpResult;

typedef struct {
    uint64_t transaction_id;
    char transaction_reference[36];
    uint64_t account_id;
    char account_number[35];
    char transaction_type[20];
    char amount[32];
    char balance_before[32];
    char balance_after[32];
    bool has_related_account;
    uint64_t related_account_id;
    bool has_atm_id;
    uint64_t atm_id;
    char status[16];
    char description[255];
    char created_at[32];
} AdminTransactionItem;

typedef struct {
    AdminTransactionItem items[ADMIN_MAX_TRANSACTIONS];
    size_t count;
} AdminTransactionList;

typedef struct {
    uint32_t qty_500;
    uint32_t qty_200;
    uint32_t qty_100;
    uint32_t qty_50;
    uint64_t total_cash;
} AdminAtmCashStatus;

typedef struct {
    uint32_t add_500;
    uint32_t add_200;
    uint32_t add_100;
    uint32_t add_50;
} AdminAtmRefillInput;

typedef enum {
    ADMIN_REFILL_SUCCESS = 0,
    ADMIN_REFILL_ERR_UNAUTHENTICATED,
    ADMIN_REFILL_ERR_ZERO_TOTAL,
    ADMIN_REFILL_ERR_OVERFLOW,
    ADMIN_REFILL_ERR_DB_FAILURE
} AdminRefillResult;

typedef struct {
    uint64_t total_customers;
    uint64_t total_accounts;
    uint64_t active_accounts;
    uint64_t total_cards;
    uint64_t active_cards;
    uint64_t blocked_cards;
    uint64_t total_successful_txns;
    uint64_t deposit_count;
    char deposit_total[32];
    uint64_t withdrawal_count;
    char withdrawal_total[32];
    uint64_t transfer_count;
    char transfer_total[32];
    uint64_t total_atm_cash;
} AdminStatistics;

/* Enum/Status helpers */
const char *admin_role_to_string(AdminRole role);
AdminRole admin_role_from_string(const char *role_str);
const char *admin_status_to_string(AdminStatus status);
AdminStatus admin_status_from_string(const char *status_str);
const char *admin_auth_result_to_message(AdminAuthResult result);
const char *admin_card_op_result_to_message(AdminCardOpResult result);
const char *admin_refill_result_to_message(AdminRefillResult result);

/* Authentication */
AdminAuthResult admin_authenticate(const char *username, const char *password, AdminSession *session);
void admin_logout(AdminSession *session);

/* Customer / Account / Card Viewing */
bool admin_get_customers(const AdminSession *session, AdminCustomerList *list);
bool admin_get_customer_accounts(const AdminSession *session, uint64_t customer_id, AdminAccountList *list);
bool admin_get_account_cards(const AdminSession *session, uint64_t account_id, AdminCardList *list);

/* Card Block / Unblock */
AdminCardOpResult admin_block_card(const AdminSession *session, uint64_t card_id);
AdminCardOpResult admin_unblock_card(const AdminSession *session, uint64_t card_id);

/* Transaction View */
bool admin_get_transactions(const AdminSession *session,
                            uint64_t account_id_filter,
                            const char *type_filter,
                            const char *status_filter,
                            unsigned int limit,
                            AdminTransactionList *list);

/* ATM Cash Management */
bool admin_get_cash_status(const AdminSession *session, uint64_t atm_id, AdminAtmCashStatus *status);
AdminRefillResult admin_refill_cash(const AdminSession *session, uint64_t atm_id, const AdminAtmRefillInput *input);

/* Statistics */
bool admin_get_statistics(const AdminSession *session, AdminStatistics *stats);

#endif /* ADMIN_H */
