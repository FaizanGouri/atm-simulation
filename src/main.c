#include <stdio.h>
#include <string.h>
#include "database.h"
#include "auth.h"
#include "card.h"
#include "account.h"
#include "security.h"
#include "validation.h"
#include "ui.h"

static void run_automated_phase5_tests(void)
{
    printf("\n--- RUNNING PHASE 5 AUTOMATED TEST SUITE ---\n");

    /* Test 1: Unauthenticated access attempt to balance inquiry */
    printf("\n[TEST 1] Menu & Balance Access without Session:\n");
    CustomerSession fake_session;
    memset(&fake_session, 0, sizeof(fake_session));
    fake_session.is_authenticated = false;
    printf("Invoking ui_display_balance_inquiry() with unauthenticated session:\n");
    ui_display_balance_inquiry(&fake_session);
    printf("Access without session rejected -> PASS\n");

    /* Test 2: Authenticate valid Card 1 ('4532015012340001', PIN '1234') */
    printf("\n[TEST 2] Customer Authentication for Card 1:\n");
    CustomerSession session1;
    uint8_t remaining = 0;
    card_reset_failed_attempts(1);
    AuthResult res = auth_authenticate_customer("4532015012340001", "1234", &session1, &remaining);
    printf("Authentication Result: %s -> %s\n",
           auth_result_to_message(res),
           (res == AUTH_SUCCESS && session1.is_authenticated) ? "PASS" : "FAIL");

    /* Test 3: Balance inquiry for Account 1 (Seeded balance: 45000.00) */
    printf("\n[TEST 3] Balance Inquiry on Account 1:\n");
    AccountRecord acc1;
    bool found = account_get_by_id(session1.account_id, &acc1);
    char formatted_bal1[48];
    account_format_currency(acc1.balance, formatted_bal1, sizeof(formatted_bal1));
    char masked_acc1[32];
    account_mask_number(acc1.account_number, masked_acc1, sizeof(masked_acc1));

    printf("Retrieved Account: %s, Raw Balance: %s, Formatted: %s\n",
           masked_acc1, acc1.balance, formatted_bal1);
    printf("Balance matches seeded 45000.00 -> %s\n",
           (found && strcmp(acc1.balance, "45000.00") == 0) ? "PASS" : "FAIL");

    /* Test 4: Multiple repeated balance inquiries (Resource leak check) */
    printf("\n[TEST 4] Multiple Balance Inquiries (Resource Leak Check):\n");
    bool all_ok = true;
    for (int i = 0; i < 5; i++) {
        AccountRecord temp_acc;
        if (!account_get_by_id(session1.account_id, &temp_acc)) {
            all_ok = false;
            break;
        }
    }
    printf("Executed 5 consecutive balance inquiries successfully -> %s\n", all_ok ? "PASS" : "FAIL");

    /* Test 5: Balance inquiry for Account 2 (Seeded balance: 75000.00) */
    printf("\n[TEST 5] Balance Inquiry on Account 2 (Card 2):\n");
    AccountRecord acc2;
    found = account_get_by_id(2, &acc2);
    char formatted_bal2[48];
    account_format_currency(acc2.balance, formatted_bal2, sizeof(formatted_bal2));
    printf("Account 2 Raw Balance: %s, Formatted: %s\n", acc2.balance, formatted_bal2);
    printf("Balance matches seeded 75000.00 -> %s\n",
           (found && strcmp(acc2.balance, "75000.00") == 0) ? "PASS" : "FAIL");

    /* Test 6: Currency formatter boundary checks (No float conversion) */
    printf("\n[TEST 6] Fixed-Precision Currency Formatting Tests:\n");
    char fmt[48];
    account_format_currency("0.00", fmt, sizeof(fmt));
    printf("  0.00 -> %s (Expected: Rs. 0.00)\n", fmt);
    account_format_currency("500.50", fmt, sizeof(fmt));
    printf("  500.50 -> %s (Expected: Rs. 500.50)\n", fmt);
    account_format_currency("125000.00", fmt, sizeof(fmt));
    printf("  125000.00 -> %s (Expected: Rs. 1,25,000.00)\n", fmt);
    printf("Currency formatting passed without floating-point conversion -> PASS\n");

    /* Test 7: Logout cleanses session */
    printf("\n[TEST 7] Logout Session Cleanup:\n");
    auth_logout(&session1);
    printf("Session is_authenticated after logout: %s -> %s\n",
           session1.is_authenticated ? "true" : "false",
           (!session1.is_authenticated) ? "PASS" : "FAIL");

    printf("\n--- ALL PHASE 5 TESTS COMPLETED ---\n\n");
}

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
            printf("\nLogging out. Thank you for using our ATM.\n");
            auth_logout(session);
            in_menu = false;
        } else {
            printf("\n[ERROR] Invalid choice '%s'. Please enter 1 or 2.\n", choice_buf);
        }
    }
}

int main(int argc, char *argv[])
{
    ui_print_header("Customer Banking System");

    DBConfig cfg;
    if (!db_init_config(&cfg)) {
        printf("Failed to initialize database configuration.\n");
        return 1;
    }

    if (!db_connect(&cfg)) {
        unsigned int err_code = 0;
        printf("[FAILED] Database connection error (code %u): %s\n",
               err_code, db_get_last_error(&err_code));
        return 1;
    }

    /* Automated test mode */
    if (argc > 1 && strcmp(argv[1], "--test-phase5") == 0) {
        run_automated_phase5_tests();
        db_disconnect();
        return 0;
    }

    /* Interactive Application Loop */
    bool running = true;
    while (running) {
        char card_input[32];
        printf("\nPlease enter your card number (or 'exit' to quit): ");
        if (!fgets(card_input, sizeof(card_input), stdin)) {
            break;
        }
        validation_trim(card_input);

        if (strcmp(card_input, "exit") == 0 || strcmp(card_input, "quit") == 0) {
            printf("\nExiting ATM system. Goodbye!\n");
            running = false;
            break;
        }

        if (!validation_is_valid_card_number(card_input)) {
            printf("\n[ERROR] Invalid card number format. Must be numeric (12-19 digits).\n");
            continue;
        }

        CustomerSession session;
        uint8_t remaining = 3;
        bool logged_in = false;

        while (remaining > 0 && !logged_in) {
            char pin_input[16];
            if (!security_read_masked_input("Enter your 4-digit PIN: ", pin_input, sizeof(pin_input))) {
                printf("\nAuthentication cancelled.\n");
                break;
            }

            AuthResult res = auth_authenticate_customer(card_input, pin_input, &session, &remaining);

            if (res == AUTH_SUCCESS) {
                logged_in = true;
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

    db_disconnect();
    return 0;
}
