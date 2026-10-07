#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "database.h"
#include "auth.h"
#include "card.h"
#include "account.h"
#include "security.h"
#include "validation.h"
#include "ui.h"
#include "deposit.h"
#include "withdrawal.h"
#include "atm.h"
#include "utils.h"
#include "transfer.h"
#include "beneficiary.h"
#include "admin.h"

static void run_customer_menu_loop(CustomerSession *session)
{
    char choice_buf[16];
    bool in_menu = true;

    while (in_menu && session->is_authenticated) {
        ui_print_customer_menu(session);

        if (!fgets(choice_buf, sizeof(choice_buf), stdin)) {
            break;
        }
        validation_trim(choice_buf);

        if (strcmp(choice_buf, "1") == 0) {
            ui_display_balance_inquiry(session);
        } else if (strcmp(choice_buf, "2") == 0) {
            ui_handle_deposit(session);
        } else if (strcmp(choice_buf, "3") == 0) {
            ui_handle_withdrawal(session);
        } else if (strcmp(choice_buf, "4") == 0) {
            ui_handle_transfer(session);
        } else if (strcmp(choice_buf, "5") == 0) {
            ui_handle_manage_beneficiaries(session);
        } else if (strcmp(choice_buf, "6") == 0) {
            ui_handle_mini_statement(session);
        } else if (strcmp(choice_buf, "7") == 0) {
            ui_handle_pin_change(session);
        } else if (strcmp(choice_buf, "8") == 0) {
            printf("\nLogging out. Thank you for using our ATM.\n");
            auth_logout(session);
            in_menu = false;
        } else {
            printf("\n[ERROR] Invalid choice '%s'. Please enter a number between 1 and 8.\n", choice_buf);
        }
    }
}

static void run_customer_login_flow(void)
{
    char card_input[32];
    printf("\nPlease enter your card number (or 'back' to return): ");
    if (!fgets(card_input, sizeof(card_input), stdin)) {
        return;
    }
    validation_trim(card_input);

    if (strcmp(card_input, "back") == 0 || strcmp(card_input, "exit") == 0 || card_input[0] == '\0') {
        return;
    }

    if (!validation_is_valid_card_number(card_input)) {
        printf("\n[ERROR] Invalid card number format. Must be numeric (12-19 digits).\n");
        return;
    }

    CustomerSession session;
    uint8_t remaining = 3;

    while (remaining > 0) {
        char pin_input[16];
        if (!security_read_masked_input("Enter your 4-digit PIN: ", pin_input, sizeof(pin_input))) {
            printf("\nAuthentication cancelled.\n");
            security_secure_zero(pin_input, sizeof(pin_input));
            break;
        }

        AuthResult res = auth_authenticate_customer(card_input, pin_input, &session, &remaining);
        security_secure_zero(pin_input, sizeof(pin_input));

        if (res == AUTH_SUCCESS) {
            printf("\n========================================\n");
            printf("        AUTHENTICATION SUCCESSFUL\n");
            printf("========================================\n");
            printf("Welcome, %s!\n", session.customer_name);

            /* Launch Customer Banking Menu */
            run_customer_menu_loop(&session);
            break;
        } else if (res == AUTH_ERR_WRONG_PIN) {
            printf("\n[ERROR] %s (Attempts remaining: %u)\n\n", auth_result_to_message(res), remaining);
        } else {
            printf("\n[ERROR] %s\n", auth_result_to_message(res));
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    ui_print_header("Banking & ATM System");

    DBConfig cfg;
    if (!db_init_config(&cfg)) {
        printf("Failed to initialize database configuration.\n");
        return 1;
    }

    bool db_ok = db_connect(&cfg);
    security_secure_zero(cfg.password, sizeof(cfg.password));

    if (!db_ok) {
        printf("[FAILED] Database service temporarily unavailable. Please try again later.\n");
        return 1;
    }

    /* Top-Level Interactive Application Loop */
    bool running = true;
    while (running) {
        ui_print_top_menu();
        char top_choice[16];
        if (!fgets(top_choice, sizeof(top_choice), stdin)) {
            break;
        }
        validation_trim(top_choice);

        if (strcmp(top_choice, "1") == 0) {
            run_customer_login_flow();
        } else if (strcmp(top_choice, "2") == 0) {
            AdminSession admin_session;
            if (ui_handle_admin_login(&admin_session)) {
                ui_handle_admin_dashboard(&admin_session);
            }
        } else if (strcmp(top_choice, "3") == 0 || strcmp(top_choice, "exit") == 0 || strcmp(top_choice, "quit") == 0) {
            printf("\nExiting ATM system. Goodbye!\n");
            running = false;
        } else {
            printf("\n[ERROR] Invalid choice '%s'. Please enter 1, 2, or 3.\n", top_choice);
        }
    }

    db_disconnect();
    return 0;
}
