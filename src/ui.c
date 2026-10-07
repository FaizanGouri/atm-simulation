#include "ui.h"
#include "account.h"
#include "database.h"
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
    printf("2. Logout\n");
    printf("========================================\n");
    printf("Enter choice [1-2]: ");
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

void ui_pause(void)
{
    printf("\nPress Enter to continue...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
