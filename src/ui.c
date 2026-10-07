#include "ui.h"
#include "account.h"
#include "database.h"
#include "validation.h"
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
    printf("7. Logout\n");
    printf("========================================\n");
    printf("Enter choice [1-7]: ");
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
        printf("\n[ERROR] Unable to retrieve account balance: %s\n", db_get_last_error(NULL));
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
        printf("\n[ERROR] Unable to retrieve transaction history: %s\n", db_get_last_error(NULL));
        return;
    }

    ui_display_mini_statement(session, &list);
    ui_pause();
}

void ui_pause(void)
{
    printf("\nPress Enter to continue...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
