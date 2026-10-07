#include "ui.h"
#include "admin.h"
#include "account.h"
#include "database.h"
#include "validation.h"
#include "security.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void ui_print_header(const char *subtitle)
{
    printf("\n========================================\n");
    printf("       ATM SIMULATION SYSTEM\n");
    if (subtitle && subtitle[0] != '\0') {
        printf("       %s\n", subtitle);
    }
    printf("========================================\n");
}

void ui_print_customer_menu(const CustomerSession *session)
{
    if (!session || !session->is_authenticated) return;

    printf("\n========================================\n");
    printf("             ATM MAIN MENU\n");
    printf("========================================\n");
    printf("1. Balance Inquiry\n");
    printf("2. Deposit\n");
    printf("3. Withdraw\n");
    printf("4. Transfer Money\n");
    printf("5. Manage Beneficiaries\n");
    printf("6. Mini Statement\n");
    printf("7. Change PIN\n");
    printf("8. Logout\n");
    printf("========================================\n");
    printf("Enter choice [1-8]: ");
    fflush(stdout);
}

void ui_print_beneficiary_menu(void)
{
    printf("\n========================================\n");
    printf("         MANAGE BENEFICIARIES\n");
    printf("========================================\n");
    printf("1. Add Beneficiary\n");
    printf("2. Remove Beneficiary\n");
    printf("3. View Beneficiaries\n");
    printf("4. Back\n");
    printf("========================================\n");
    printf("Enter choice [1-4]: ");
    fflush(stdout);
}

void ui_display_balance_inquiry(const CustomerSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    AccountRecord acc;
    if (!account_get_by_id(session->account_id, &acc)) {
        printf("\n[ERROR] Unable to retrieve account balance. Please try again later.\n");
        return;
    }

    char masked_acc[32];
    account_mask_number(acc.account_number, masked_acc, sizeof(masked_acc));

    char formatted_bal[48];
    account_format_currency(acc.balance, formatted_bal, sizeof(formatted_bal));

    printf("\n========================================\n");
    printf("           BALANCE INQUIRY\n");
    printf("========================================\n");
    printf("Account Holder    : %s\n", session->customer_name);
    printf("Account Number    : %s\n", masked_acc);
    printf("Account Type      : %s\n", acc.account_type);
    printf("Available Balance : %s\n", formatted_bal);
    printf("========================================\n");
}

void ui_display_deposit_receipt(const DepositReceipt *receipt, const char *customer_name)
{
    if (!receipt) return;

    char fmt_dep[48], fmt_prev[48], fmt_new[48];
    account_format_currency(receipt->deposited_amount, fmt_dep, sizeof(fmt_dep));
    account_format_currency(receipt->previous_balance, fmt_prev, sizeof(fmt_prev));
    account_format_currency(receipt->new_balance, fmt_new, sizeof(fmt_new));

    printf("\n========================================\n");
    printf("             DEPOSIT RECEIPT\n");
    printf("========================================\n");
    printf("Transaction Ref   : %s\n", receipt->transaction_reference);
    if (customer_name && customer_name[0] != '\0') {
        printf("Account Holder    : %s\n", customer_name);
    }
    printf("Amount Deposited  : %s\n", fmt_dep);
    printf("Previous Balance  : %s\n", fmt_prev);
    printf("Available Balance : %s\n", fmt_new);
    printf("Status            : SUCCESS\n");
    printf("========================================\n");
}

void ui_display_withdrawal_receipt(const WithdrawalReceipt *receipt, const char *customer_name)
{
    if (!receipt) return;

    char fmt_wth[48], fmt_prev[48], fmt_new[48];
    account_format_currency(receipt->withdrawn_amount, fmt_wth, sizeof(fmt_wth));
    account_format_currency(receipt->previous_balance, fmt_prev, sizeof(fmt_prev));
    account_format_currency(receipt->new_balance, fmt_new, sizeof(fmt_new));

    printf("\n========================================\n");
    printf("           WITHDRAWAL RECEIPT\n");
    printf("========================================\n");
    printf("Transaction Ref   : %s\n", receipt->transaction_reference);
    if (customer_name && customer_name[0] != '\0') {
        printf("Account Holder    : %s\n", customer_name);
    }
    printf("Amount Dispensed  : %s\n", fmt_wth);
    printf("Previous Balance  : %s\n", fmt_prev);
    printf("Available Balance : %s\n", fmt_new);
    printf("Status            : SUCCESS\n");
    printf("----------------------------------------\n");
    printf("Notes Dispensed:\n");
    if (receipt->dispensed_notes.count_500 > 0) {
        printf("  Rs. 500  x  %u\n", receipt->dispensed_notes.count_500);
    }
    if (receipt->dispensed_notes.count_200 > 0) {
        printf("  Rs. 200  x  %u\n", receipt->dispensed_notes.count_200);
    }
    if (receipt->dispensed_notes.count_100 > 0) {
        printf("  Rs. 100  x  %u\n", receipt->dispensed_notes.count_100);
    }
    if (receipt->dispensed_notes.count_50 > 0) {
        printf("  Rs. 50   x  %u\n", receipt->dispensed_notes.count_50);
    }
    printf("========================================\n");
}

void ui_display_transfer_receipt(const TransferReceipt *receipt, const char *customer_name)
{
    if (!receipt) return;

    char fmt_amt[48], fmt_prev[48], fmt_new[48];
    account_format_currency(receipt->transferred_amount, fmt_amt, sizeof(fmt_amt));
    account_format_currency(receipt->source_prev_balance, fmt_prev, sizeof(fmt_prev));
    account_format_currency(receipt->source_new_balance, fmt_new, sizeof(fmt_new));

    printf("\n========================================\n");
    printf("            TRANSFER RECEIPT\n");
    printf("========================================\n");
    printf("Transaction Ref   : %s\n", receipt->transaction_reference);
    if (customer_name && customer_name[0] != '\0') {
        printf("Sender            : %s\n", customer_name);
    }
    printf("Beneficiary       : %s\n", receipt->beneficiary_name);
    printf("Destination A/C   : %s\n", receipt->dest_account_masked);
    printf("Amount Transferred: %s\n", fmt_amt);
    printf("Previous Balance  : %s\n", fmt_prev);
    printf("Available Balance : %s\n", fmt_new);
    printf("Status            : SUCCESS\n");
    printf("========================================\n");
}

void ui_display_beneficiary_list(const BeneficiaryList *list)
{
    printf("\n==========================================================================\n");
    printf("                            BENEFICIARY LIST\n");
    printf("==========================================================================\n");
    if (!list || list->count == 0) {
        printf("  No registered beneficiaries found.\n");
        printf("==========================================================================\n");
        return;
    }

    printf("%-4s  %-24s  %-16s  %-18s\n", "#", "Beneficiary Name", "Nickname", "Account Number");
    printf("--------------------------------------------------------------------------\n");

    for (size_t i = 0; i < list->count; i++) {
        const BeneficiaryRecord *b = &list->items[i];
        char masked_acc[32];
        account_mask_number(b->beneficiary_account_number, masked_acc, sizeof(masked_acc));
        const char *nick = (b->nickname[0] != '\0') ? b->nickname : "-";
        printf("%-4zu  %-24s  %-16s  %-18s\n", i + 1, b->beneficiary_name, nick, masked_acc);
    }
    printf("==========================================================================\n");
}

void ui_handle_deposit(const CustomerSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    char amount_buf[32];
    printf("\nEnter deposit amount (in Rs., or 0 to cancel): ");
    if (!fgets(amount_buf, sizeof(amount_buf), stdin)) {
        return;
    }
    validation_trim(amount_buf);

    if (strcmp(amount_buf, "0") == 0 || strcmp(amount_buf, "cancel") == 0) {
        printf("Deposit cancelled.\n");
        return;
    }

    DepositReceipt receipt;
    DepositResult res = deposit_execute(session->account_id, 1, amount_buf, &receipt);

    if (res == DEPOSIT_SUCCESS) {
        ui_display_deposit_receipt(&receipt, session->customer_name);
    } else {
        printf("\n[ERROR] %s\n", deposit_result_to_string(res));
    }
}

void ui_handle_withdrawal(const CustomerSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    char amount_buf[32];
    printf("\nEnter withdrawal amount (multiples of Rs. 50, or 0 to cancel): ");
    if (!fgets(amount_buf, sizeof(amount_buf), stdin)) {
        return;
    }
    validation_trim(amount_buf);

    if (strcmp(amount_buf, "0") == 0 || strcmp(amount_buf, "cancel") == 0) {
        printf("Withdrawal cancelled.\n");
        return;
    }

    WithdrawalReceipt receipt;
    WithdrawalResult res = withdrawal_execute(session->account_id, 1, amount_buf, &receipt);

    if (res == WITHDRAWAL_SUCCESS) {
        ui_display_withdrawal_receipt(&receipt, session->customer_name);
    } else {
        printf("\n[ERROR] %s\n", withdrawal_result_to_string(res));
    }
}

void ui_handle_transfer(const CustomerSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    BeneficiaryList list;
    if (!beneficiary_get_list(session->account_id, &list) || list.count == 0) {
        printf("\n[INFO] You do not have any registered beneficiaries.\n");
        printf("Please use 'Manage Beneficiaries' from the main menu to add one first.\n");
        return;
    }

    ui_display_beneficiary_list(&list);

    char choice_buf[16];
    printf("\nSelect beneficiary [1-%zu] (or 0 to cancel): ", list.count);
    if (!fgets(choice_buf, sizeof(choice_buf), stdin)) return;
    validation_trim(choice_buf);

    int idx = atoi(choice_buf);
    if (idx <= 0 || (size_t)idx > list.count) {
        printf("Transfer cancelled.\n");
        return;
    }

    const BeneficiaryRecord *sel_b = &list.items[idx - 1];

    char amount_buf[32];
    printf("Enter transfer amount in Rs. to %s: ", sel_b->beneficiary_name);
    if (!fgets(amount_buf, sizeof(amount_buf), stdin)) return;
    validation_trim(amount_buf);

    if (strcmp(amount_buf, "0") == 0 || strcmp(amount_buf, "cancel") == 0) {
        printf("Transfer cancelled.\n");
        return;
    }

    char confirm_buf[16];
    printf("Confirm transfer of Rs. %s to %s? (y/n): ", amount_buf, sel_b->beneficiary_name);
    if (!fgets(confirm_buf, sizeof(confirm_buf), stdin)) return;
    validation_trim(confirm_buf);

    if (confirm_buf[0] != 'y' && confirm_buf[0] != 'Y') {
        printf("Transfer cancelled by user.\n");
        return;
    }

    TransferReceipt receipt;
    TransferResult res = transfer_execute(session->account_id, sel_b->beneficiary_id, amount_buf, &receipt);

    if (res == TRANSFER_SUCCESS) {
        ui_display_transfer_receipt(&receipt, session->customer_name);
    } else {
        printf("\n[ERROR] %s\n", transfer_result_to_string(res));
    }
}

void ui_handle_manage_beneficiaries(const CustomerSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    bool in_b_menu = true;
    while (in_b_menu) {
        ui_print_beneficiary_menu();

        char choice_buf[16];
        if (!fgets(choice_buf, sizeof(choice_buf), stdin)) break;
        validation_trim(choice_buf);

        if (strcmp(choice_buf, "1") == 0) {
            /* Add Beneficiary */
            char target_acc[40];
            printf("\nEnter beneficiary account number (or 0 to cancel): ");
            if (!fgets(target_acc, sizeof(target_acc), stdin)) continue;
            validation_trim(target_acc);

            if (strcmp(target_acc, "0") == 0 || strcmp(target_acc, "cancel") == 0) {
                printf("Operation cancelled.\n");
                continue;
            }

            char nickname[60];
            printf("Enter optional nickname (press Enter to skip): ");
            if (!fgets(nickname, sizeof(nickname), stdin)) nickname[0] = '\0';
            validation_trim(nickname);

            uint64_t new_id = 0;
            BeneficiaryResult res = beneficiary_add(session->account_id, target_acc, nickname, &new_id);

            if (res == BENEFICIARY_SUCCESS) {
                printf("\n[SUCCESS] Beneficiary added successfully! (ID: %llu)\n", (unsigned long long)new_id);
            } else {
                printf("\n[ERROR] %s\n", beneficiary_result_to_string(res));
            }
        } else if (strcmp(choice_buf, "2") == 0) {
            /* Remove Beneficiary */
            BeneficiaryList list;
            if (!beneficiary_get_list(session->account_id, &list) || list.count == 0) {
                printf("\nNo beneficiaries to remove.\n");
                continue;
            }

            ui_display_beneficiary_list(&list);

            char sel_buf[16];
            printf("\nSelect beneficiary # to remove [1-%zu] (or 0 to cancel): ", list.count);
            if (!fgets(sel_buf, sizeof(sel_buf), stdin)) continue;
            validation_trim(sel_buf);

            int idx = atoi(sel_buf);
            if (idx <= 0 || (size_t)idx > list.count) {
                printf("Removal cancelled.\n");
                continue;
            }

            uint64_t target_bid = list.items[idx - 1].beneficiary_id;
            BeneficiaryResult res = beneficiary_remove(session->account_id, target_bid);

            if (res == BENEFICIARY_SUCCESS) {
                printf("\n[SUCCESS] Beneficiary '%s' removed successfully.\n", list.items[idx - 1].beneficiary_name);
            } else {
                printf("\n[ERROR] %s\n", beneficiary_result_to_string(res));
            }
        } else if (strcmp(choice_buf, "3") == 0) {
            /* View Beneficiaries */
            BeneficiaryList list;
            beneficiary_get_list(session->account_id, &list);
            ui_display_beneficiary_list(&list);
        } else if (strcmp(choice_buf, "4") == 0) {
            in_b_menu = false;
        } else {
            printf("\n[ERROR] Invalid choice '%s'. Please enter 1-4.\n", choice_buf);
        }
    }
}

void ui_display_mini_statement(const CustomerSession *session, const StatementList *list)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    AccountRecord acc;
    if (!account_get_by_id(session->account_id, &acc)) {
        printf("\n[ERROR] Unable to fetch account information.\n");
        return;
    }

    char masked_acc[32];
    account_mask_number(acc.account_number, masked_acc, sizeof(masked_acc));

    char formatted_bal[48];
    account_format_currency(acc.balance, formatted_bal, sizeof(formatted_bal));

    printf("\n========================================================================================\n");
    printf("                                    MINI STATEMENT\n");
    printf("========================================================================================\n");
    printf("Account Number  : %s\n", masked_acc);
    printf("Account Holder  : %s\n", session->customer_name);
    printf("Account Type    : %s\n", acc.account_type);
    printf("----------------------------------------------------------------------------------------\n");
    printf("%-18s  %-12s  %-16s  %-9s  %-24s\n", "Date & Time", "Type", "Amount", "Status", "Details");
    printf("----------------------------------------------------------------------------------------\n");

    if (!list || list->count == 0) {
        printf("  No transactions found for this account.\n");
    } else {
        for (size_t i = 0; i < list->count; i++) {
            const StatementItem *item = &list->items[i];

            char fmt_amt[48];
            account_format_currency(item->amount, fmt_amt, sizeof(fmt_amt));

            char signed_amt[56];
            snprintf(signed_amt, sizeof(signed_amt), "%s%s", item->is_credit ? "+" : "-", fmt_amt);

            const char *type_str = transaction_type_to_string(item->type);
            const char *status_str = transaction_status_to_string(item->status);

            char details[64] = {0};
            if (item->type == TXN_TYPE_TRANSFER) {
                if (item->has_related_account && item->related_account_number[0] != '\0') {
                    char masked_rel[32];
                    account_mask_number(item->related_account_number, masked_rel, sizeof(masked_rel));
                    snprintf(details, sizeof(details), "%s %s", item->is_credit ? "From" : "To", masked_rel);
                } else {
                    snprintf(details, sizeof(details), "Fund Transfer");
                }
            } else if (item->type == TXN_TYPE_DEPOSIT) {
                snprintf(details, sizeof(details), "Cash Deposit");
            } else if (item->type == TXN_TYPE_WITHDRAWAL) {
                snprintf(details, sizeof(details), "ATM Withdrawal");
            } else {
                strncpy(details, item->description, sizeof(details) - 1);
            }

            printf("%-18s  %-12s  %-16s  %-9s  %-24s\n",
                   item->created_at_formatted,
                   type_str,
                   signed_amt,
                   status_str,
                   details);
        }
    }

    printf("----------------------------------------------------------------------------------------\n");
    printf("Current Available Balance : %s\n", formatted_bal);
    printf("========================================================================================\n");
}

void ui_handle_mini_statement(const CustomerSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    printf("\nMini Statement Options:\n");
    printf("1. Last 5 transactions\n");
    printf("2. Last 10 transactions (Default)\n");
    printf("3. Last 20 transactions\n");
    printf("Enter choice [1-3, or Enter for default]: ");
    fflush(stdout);

    char choice_buf[16];
    unsigned int limit = 10;
    if (fgets(choice_buf, sizeof(choice_buf), stdin)) {
        validation_trim(choice_buf);
        if (strcmp(choice_buf, "1") == 0) limit = 5;
        else if (strcmp(choice_buf, "2") == 0) limit = 10;
        else if (strcmp(choice_buf, "3") == 0) limit = 20;
    }

    StatementList list;
    if (!transaction_get_statement(session->account_id, limit, &list)) {
        printf("\n[ERROR] Unable to retrieve transaction history. Please try again later.\n");
        return;
    }

    ui_display_mini_statement(session, &list);
    ui_pause();
}

void ui_handle_pin_change(CustomerSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active customer session.\n");
        return;
    }

    printf("\n========================================\n");
    printf("              CHANGE PIN\n");
    printf("========================================\n");

    char current_pin[16] = {0};
    char new_pin[16] = {0};
    char confirm_pin[16] = {0};

    if (!security_read_masked_input("Current PIN: ", current_pin, sizeof(current_pin))) {
        printf("\nPIN change cancelled.\n");
        security_secure_zero(current_pin, sizeof(current_pin));
        return;
    }

    if (!security_read_masked_input("New PIN: ", new_pin, sizeof(new_pin))) {
        printf("\nPIN change cancelled.\n");
        security_secure_zero(current_pin, sizeof(current_pin));
        security_secure_zero(new_pin, sizeof(new_pin));
        return;
    }

    if (!security_read_masked_input("Confirm New PIN: ", confirm_pin, sizeof(confirm_pin))) {
        printf("\nPIN change cancelled.\n");
        security_secure_zero(current_pin, sizeof(current_pin));
        security_secure_zero(new_pin, sizeof(new_pin));
        security_secure_zero(confirm_pin, sizeof(confirm_pin));
        return;
    }

    PinChangeResult result = auth_change_pin(session, current_pin, new_pin, confirm_pin);

    /* Zero all PIN buffers immediately */
    security_secure_zero(current_pin, sizeof(current_pin));
    security_secure_zero(new_pin, sizeof(new_pin));
    security_secure_zero(confirm_pin, sizeof(confirm_pin));

    if (result == PIN_CHANGE_SUCCESS) {
        printf("\nPIN changed successfully.\n");
    } else {
        printf("\n[ERROR] %s\n", auth_pin_change_result_to_message(result));
    }

    ui_pause();
}

void ui_pause(void)
{
    printf("\nPress Enter to continue...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void ui_print_top_menu(void)
{
    printf("\n========================================\n");
    printf("              ATM SYSTEM\n");
    printf("========================================\n");
    printf("1. Customer Login\n");
    printf("2. Admin Login\n");
    printf("3. Exit\n");
    printf("========================================\n");
    printf("Enter choice [1-3]: ");
    fflush(stdout);
}

void ui_print_admin_menu(const AdminSession *session)
{
    if (!session || !session->is_authenticated) return;

    printf("\n========================================\n");
    printf("             ADMIN DASHBOARD\n");
    printf("========================================\n");
    printf("1. View Customers\n");
    printf("2. View Customer Accounts\n");
    printf("3. View Cards\n");
    printf("4. Block Card\n");
    printf("5. Unblock Card\n");
    printf("6. View Transactions\n");
    printf("7. ATM Cash Status\n");
    printf("8. ATM Cash Refill\n");
    printf("9. Statistics\n");
    printf("10. Logout\n");
    printf("========================================\n");
    printf("Enter choice [1-10]: ");
    fflush(stdout);
}

bool ui_handle_admin_login(AdminSession *session)
{
    if (!session) return false;

    printf("\n========================================\n");
    printf("          ADMINISTRATOR LOGIN\n");
    printf("========================================\n");

    char username_buf[64] = {0};
    printf("Username: ");
    fflush(stdout);
    if (!fgets(username_buf, sizeof(username_buf), stdin)) {
        return false;
    }
    validation_trim(username_buf);

    char password_buf[64] = {0};
    if (!security_read_masked_input("Password: ", password_buf, sizeof(password_buf))) {
        printf("\nLogin cancelled.\n");
        security_secure_zero(password_buf, sizeof(password_buf));
        return false;
    }

    AdminAuthResult res = admin_authenticate(username_buf, password_buf, session);
    security_secure_zero(password_buf, sizeof(password_buf));

    if (res == ADMIN_AUTH_SUCCESS) {
        printf("\n========================================\n");
        printf("    ADMINISTRATOR LOGIN SUCCESSFUL\n");
        printf("========================================\n");
        printf("Welcome, %s (%s)!\n", session->full_name, admin_role_to_string(session->role));
        return true;
    } else {
        printf("\n[ERROR] %s\n", admin_auth_result_to_message(res));
        return false;
    }
}

void ui_display_admin_customers(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    AdminCustomerList list;
    if (!admin_get_customers(session, &list)) {
        printf("\n[ERROR] Unable to retrieve customer directory.\n");
        return;
    }

    printf("\n========================================================================================\n");
    printf("                                CUSTOMER DIRECTORY\n");
    printf("========================================================================================\n");
    printf("%-5s | %-15s | %-20s | %-22s | %-12s | %-8s\n",
           "ID", "Cust Number", "Full Name", "Email", "Phone", "Status");
    printf("------+-----------------+----------------------+------------------------+--------------+---------\n");

    for (size_t i = 0; i < list.count; i++) {
        printf("%-5llu | %-15s | %-20s | %-22s | %-12s | %-8s\n",
               (unsigned long long)list.items[i].customer_id,
               list.items[i].customer_number,
               list.items[i].full_name,
               list.items[i].email,
               list.items[i].phone,
               list.items[i].status);
    }
    printf("========================================================================================\n");
    printf("Total records displayed: %zu\n", list.count);
    ui_pause();
}

void ui_display_admin_accounts(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    char in_buf[32];
    printf("\nEnter Customer ID: ");
    fflush(stdout);
    if (!fgets(in_buf, sizeof(in_buf), stdin)) return;
    validation_trim(in_buf);
    uint64_t cid = strtoull(in_buf, NULL, 10);
    if (cid == 0) {
        printf("[ERROR] Invalid Customer ID.\n");
        return;
    }

    AdminAccountList list;
    if (!admin_get_customer_accounts(session, cid, &list)) {
        printf("\n[ERROR] Unable to retrieve customer accounts.\n");
        return;
    }

    if (list.count == 0) {
        printf("\nNo accounts found for Customer ID %llu.\n", (unsigned long long)cid);
        ui_pause();
        return;
    }

    printf("\n================================================================================\n");
    printf("                        ACCOUNTS FOR CUSTOMER ID %llu\n", (unsigned long long)cid);
    printf("================================================================================\n");
    printf("%-8s | %-18s | %-12s | %-18s | %-10s\n",
           "Acc ID", "Account Number", "Type", "Balance", "Status");
    printf("---------+--------------------+--------------+--------------------+-----------\n");

    for (size_t i = 0; i < list.count; i++) {
        char masked[32];
        account_mask_number(list.items[i].account_number, masked, sizeof(masked));
        char formatted_bal[48];
        account_format_currency(list.items[i].balance, formatted_bal, sizeof(formatted_bal));

        printf("%-8llu | %-18s | %-12s | %-18s | %-10s\n",
               (unsigned long long)list.items[i].account_id,
               masked,
               list.items[i].account_type,
               formatted_bal,
               list.items[i].status);
    }
    printf("================================================================================\n");
    ui_pause();
}

void ui_display_admin_cards(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    char in_buf[32];
    printf("\nEnter Account ID: ");
    fflush(stdout);
    if (!fgets(in_buf, sizeof(in_buf), stdin)) return;
    validation_trim(in_buf);
    uint64_t aid = strtoull(in_buf, NULL, 10);
    if (aid == 0) {
        printf("[ERROR] Invalid Account ID.\n");
        return;
    }

    AdminCardList list;
    if (!admin_get_account_cards(session, aid, &list)) {
        printf("\n[ERROR] Unable to retrieve account cards.\n");
        return;
    }

    if (list.count == 0) {
        printf("\nNo cards found for Account ID %llu.\n", (unsigned long long)aid);
        ui_pause();
        return;
    }

    printf("\n================================================================================\n");
    printf("                          CARDS FOR ACCOUNT ID %llu\n", (unsigned long long)aid);
    printf("================================================================================\n");
    printf("%-8s | %-20s | %-12s | %-12s | %-15s\n",
           "Card ID", "Card Number", "Status", "Expiry", "Failed Attempts");
    printf("---------+----------------------+--------------+--------------+----------------\n");

    for (size_t i = 0; i < list.count; i++) {
        char masked_card[24];
        size_t len = strlen(list.items[i].card_number);
        if (len >= 4) {
            snprintf(masked_card, sizeof(masked_card), "****-****-****-%s", list.items[i].card_number + (len - 4));
        } else {
            strncpy(masked_card, list.items[i].card_number, sizeof(masked_card) - 1);
            masked_card[sizeof(masked_card) - 1] = '\0';
        }

        printf("%-8llu | %-20s | %-12s | %-12s | %-15u\n",
               (unsigned long long)list.items[i].card_id,
               masked_card,
               list.items[i].status,
               list.items[i].expiry_date,
               list.items[i].failed_pin_attempts);
    }
    printf("================================================================================\n");
    ui_pause();
}

void ui_handle_admin_block_card(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    char in_buf[32];
    printf("\nEnter Card ID to BLOCK: ");
    fflush(stdout);
    if (!fgets(in_buf, sizeof(in_buf), stdin)) return;
    validation_trim(in_buf);
    uint64_t cid = strtoull(in_buf, NULL, 10);
    if (cid == 0) {
        printf("[ERROR] Invalid Card ID.\n");
        return;
    }

    AdminCardOpResult res = admin_block_card(session, cid);
    if (res == ADMIN_CARD_OP_SUCCESS) {
        printf("\n[SUCCESS] Card ID %llu has been BLOCKED.\n", (unsigned long long)cid);
    } else {
        printf("\n[ERROR] %s\n", admin_card_op_result_to_message(res));
    }
    ui_pause();
}

void ui_handle_admin_unblock_card(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    char in_buf[32];
    printf("\nEnter Card ID to UNBLOCK: ");
    fflush(stdout);
    if (!fgets(in_buf, sizeof(in_buf), stdin)) return;
    validation_trim(in_buf);
    uint64_t cid = strtoull(in_buf, NULL, 10);
    if (cid == 0) {
        printf("[ERROR] Invalid Card ID.\n");
        return;
    }

    AdminCardOpResult res = admin_unblock_card(session, cid);
    if (res == ADMIN_CARD_OP_SUCCESS) {
        printf("\n[SUCCESS] Card ID %llu has been UNBLOCKED and reset to ACTIVE.\n", (unsigned long long)cid);
    } else {
        printf("\n[ERROR] %s\n", admin_card_op_result_to_message(res));
    }
    ui_pause();
}

void ui_display_admin_transactions(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    char buf[32];
    printf("\nFilter by Account ID (0 for all): ");
    fflush(stdout);
    uint64_t aid_filt = 0;
    if (fgets(buf, sizeof(buf), stdin)) {
        validation_trim(buf);
        aid_filt = strtoull(buf, NULL, 10);
    }

    printf("Filter by Type (DEPOSIT, WITHDRAWAL, TRANSFER, PIN_CHANGE, or Enter for all): ");
    fflush(stdout);
    char type_filt[32] = {0};
    if (fgets(type_filt, sizeof(type_filt), stdin)) {
        validation_trim(type_filt);
    }

    printf("Filter by Status (SUCCESS, FAILED, REVERSED, or Enter for all): ");
    fflush(stdout);
    char stat_filt[32] = {0};
    if (fgets(stat_filt, sizeof(stat_filt), stdin)) {
        validation_trim(stat_filt);
    }

    printf("Limit (default 15, max 50): ");
    fflush(stdout);
    unsigned int lim = 15;
    if (fgets(buf, sizeof(buf), stdin)) {
        validation_trim(buf);
        if (buf[0] != '\0') lim = (unsigned int)strtoul(buf, NULL, 10);
    }

    AdminTransactionList list;
    if (!admin_get_transactions(session, aid_filt, type_filt, stat_filt, lim, &list)) {
        printf("\n[ERROR] Unable to retrieve transactions.\n");
        return;
    }

    printf("\n========================================================================================================\n");
    printf("                                         TRANSACTION LOGS\n");
    printf("========================================================================================================\n");
    printf("%-5s | %-19s | %-16s | %-12s | %-12s | %-8s | %-19s\n",
           "ID", "Reference", "Account", "Type", "Amount", "Status", "Timestamp");
    printf("------+---------------------+------------------+--------------+--------------+----------+--------------------\n");

    for (size_t i = 0; i < list.count; i++) {
        char masked[32];
        account_mask_number(list.items[i].account_number, masked, sizeof(masked));
        char formatted_amt[48];
        account_format_currency(list.items[i].amount, formatted_amt, sizeof(formatted_amt));

        printf("%-5llu | %-19s | %-16s | %-12s | %-12s | %-8s | %-19s\n",
               (unsigned long long)list.items[i].transaction_id,
               list.items[i].transaction_reference,
               masked,
               list.items[i].transaction_type,
               formatted_amt,
               list.items[i].status,
               list.items[i].created_at);
    }
    printf("========================================================================================================\n");
    printf("Total records retrieved: %zu\n", list.count);
    ui_pause();
}

void ui_display_admin_cash_status(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    AdminAtmCashStatus st;
    if (!admin_get_cash_status(session, 1, &st)) {
        printf("\n[ERROR] Unable to retrieve ATM cash status.\n");
        return;
    }

    char tot_str[32];
    snprintf(tot_str, sizeof(tot_str), "%llu.00", (unsigned long long)st.total_cash);
    char fmt_tot[48];
    account_format_currency(tot_str, fmt_tot, sizeof(fmt_tot));

    char sub_500[32], sub_200[32], sub_100[32], sub_50[32];
    snprintf(sub_500, sizeof(sub_500), "%llu.00", (unsigned long long)st.qty_500 * 500);
    snprintf(sub_200, sizeof(sub_200), "%llu.00", (unsigned long long)st.qty_200 * 200);
    snprintf(sub_100, sizeof(sub_100), "%llu.00", (unsigned long long)st.qty_100 * 100);
    snprintf(sub_50,  sizeof(sub_50),  "%llu.00", (unsigned long long)st.qty_50  * 50);

    char f_500[48], f_200[48], f_100[48], f_50[48];
    account_format_currency(sub_500, f_500, sizeof(f_500));
    account_format_currency(sub_200, f_200, sizeof(f_200));
    account_format_currency(sub_100, f_100, sizeof(f_100));
    account_format_currency(sub_50,  f_50,  sizeof(f_50));

    printf("\n=======================================================\n");
    printf("                ATM CASH INVENTORY STATUS              \n");
    printf("=======================================================\n");
    printf("Denomination | Quantity | Subtotal Value\n");
    printf("-------------+----------+------------------------------\n");
    printf("  Rs. 500    | %-8u | %s\n", st.qty_500, f_500);
    printf("  Rs. 200    | %-8u | %s\n", st.qty_200, f_200);
    printf("  Rs. 100    | %-8u | %s\n", st.qty_100, f_100);
    printf("  Rs. 50     | %-8u | %s\n", st.qty_50,  f_50);
    printf("=======================================================\n");
    printf("Total Cash in ATM Vault: %s\n", fmt_tot);
    printf("=======================================================\n");
    ui_pause();
}

void ui_handle_admin_refill_cash(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    printf("\n========================================\n");
    printf("           ATM CASH REFILL\n");
    printf("========================================\n");

    AdminAtmRefillInput in;
    memset(&in, 0, sizeof(in));

    char buf[32];
    printf("Enter additional Rs. 500 notes to add: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        validation_trim(buf);
        if (buf[0] != '\0') in.add_500 = (uint32_t)strtoul(buf, NULL, 10);
    }

    printf("Enter additional Rs. 200 notes to add: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        validation_trim(buf);
        if (buf[0] != '\0') in.add_200 = (uint32_t)strtoul(buf, NULL, 10);
    }

    printf("Enter additional Rs. 100 notes to add: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        validation_trim(buf);
        if (buf[0] != '\0') in.add_100 = (uint32_t)strtoul(buf, NULL, 10);
    }

    printf("Enter additional Rs. 50 notes to add: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        validation_trim(buf);
        if (buf[0] != '\0') in.add_50 = (uint32_t)strtoul(buf, NULL, 10);
    }

    AdminRefillResult res = admin_refill_cash(session, 1, &in);
    if (res == ADMIN_REFILL_SUCCESS) {
        printf("\n[SUCCESS] %s\n", admin_refill_result_to_message(res));
    } else {
        printf("\n[ERROR] %s\n", admin_refill_result_to_message(res));
    }
    ui_pause();
}

void ui_display_admin_statistics(const AdminSession *session)
{
    if (!session || !session->is_authenticated) {
        printf("\n[ERROR] Access denied: No active admin session.\n");
        return;
    }

    AdminStatistics stats;
    if (!admin_get_statistics(session, &stats)) {
        printf("\n[ERROR] Unable to calculate system statistics.\n");
        return;
    }

    char f_dep[48], f_wth[48], f_trf[48];
    account_format_currency(stats.deposit_total, f_dep, sizeof(f_dep));
    account_format_currency(stats.withdrawal_total, f_wth, sizeof(f_wth));
    account_format_currency(stats.transfer_total, f_trf, sizeof(f_trf));

    char atm_tot_str[32];
    snprintf(atm_tot_str, sizeof(atm_tot_str), "%llu.00", (unsigned long long)stats.total_atm_cash);
    char f_atm[48];
    account_format_currency(atm_tot_str, f_atm, sizeof(f_atm));

    printf("\n=======================================================\n");
    printf("              SYSTEM OPERATIONAL STATISTICS            \n");
    printf("=======================================================\n");
    printf("  Customers Registered   : %llu\n", (unsigned long long)stats.total_customers);
    printf("  Bank Accounts          : %llu  (Active: %llu)\n",
           (unsigned long long)stats.total_accounts, (unsigned long long)stats.active_accounts);
    printf("  Debit Cards            : %llu  (Active: %llu, Blocked: %llu)\n",
           (unsigned long long)stats.total_cards, (unsigned long long)stats.active_cards, (unsigned long long)stats.blocked_cards);
    printf("-------------------------------------------------------\n");
    printf("  Total Successful Txns  : %llu\n", (unsigned long long)stats.total_successful_txns);
    printf("    - Deposits           : %llu txns  (%s)\n", (unsigned long long)stats.deposit_count, f_dep);
    printf("    - Withdrawals        : %llu txns  (%s)\n", (unsigned long long)stats.withdrawal_count, f_wth);
    printf("    - Fund Transfers     : %llu txns  (%s)\n", (unsigned long long)stats.transfer_count, f_trf);
    printf("-------------------------------------------------------\n");
    printf("  Total Cash in ATM Vault: %s\n", f_atm);
    printf("=======================================================\n");
    ui_pause();
}

void ui_handle_admin_dashboard(AdminSession *session)
{
    if (!session || !session->is_authenticated) return;

    char choice_buf[16];
    bool in_admin = true;

    while (in_admin && session->is_authenticated) {
        ui_print_admin_menu(session);

        if (!fgets(choice_buf, sizeof(choice_buf), stdin)) {
            break;
        }
        validation_trim(choice_buf);

        if (strcmp(choice_buf, "1") == 0) {
            ui_display_admin_customers(session);
        } else if (strcmp(choice_buf, "2") == 0) {
            ui_display_admin_accounts(session);
        } else if (strcmp(choice_buf, "3") == 0) {
            ui_display_admin_cards(session);
        } else if (strcmp(choice_buf, "4") == 0) {
            ui_handle_admin_block_card(session);
        } else if (strcmp(choice_buf, "5") == 0) {
            ui_handle_admin_unblock_card(session);
        } else if (strcmp(choice_buf, "6") == 0) {
            ui_display_admin_transactions(session);
        } else if (strcmp(choice_buf, "7") == 0) {
            ui_display_admin_cash_status(session);
        } else if (strcmp(choice_buf, "8") == 0) {
            ui_handle_admin_refill_cash(session);
        } else if (strcmp(choice_buf, "9") == 0) {
            ui_display_admin_statistics(session);
        } else if (strcmp(choice_buf, "10") == 0) {
            printf("\nAdministrator logged out.\n");
            admin_logout(session);
            in_admin = false;
        } else {
            printf("\n[ERROR] Invalid choice '%s'. Please enter a number between 1 and 10.\n", choice_buf);
        }
    }
}

