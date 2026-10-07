#include "ui.h"
#include "account.h"
#include "database.h"
#include "validation.h"
#include <stdio.h>
#include <string.h>

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
    printf("4. Logout\n");
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

void ui_pause(void)
{
    printf("\nPress Enter to continue...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
