#include <stdio.h>
#include <string.h>
#include "database.h"
#include "auth.h"
#include "card.h"
#include "security.h"
#include "validation.h"

static void run_automated_auth_tests(void)
{
    printf("\n--- RUNNING AUTOMATED AUTHENTICATION TEST SUITE ---\n");

    /* Test 1: Invalid/nonexistent card */
    printf("\n[TEST 1] Nonexistent Card Test:\n");
    CustomerSession session;
    uint8_t remaining = 0;
    AuthResult res = auth_authenticate_customer("0000000000000000", "1234", &session, &remaining);
    printf("Result: %s (Expected: Invalid card) -> %s\n",
           auth_result_to_message(res),
           (res == AUTH_ERR_INVALID_CARD) ? "PASS" : "FAIL");

    /* Test 2: Invalid card number format */
    printf("\n[TEST 2] Invalid Card Format Test (letters/short):\n");
    res = auth_authenticate_customer("abc12", "1234", &session, &remaining);
    printf("Result: %s -> %s\n", auth_result_to_message(res), (res == AUTH_ERR_INVALID_CARD) ? "PASS" : "FAIL");

    /* Test 3: Valid ACTIVE card (Card 1) + valid PIN */
    printf("\n[TEST 3] Valid Active Card + Correct PIN:\n");
    /* Seeded Card 1: '4532015012340001', hash matches PIN '1234' */
    card_reset_failed_attempts(1);
    res = auth_authenticate_customer("4532015012340001", "1234", &session, &remaining);
    printf("Result: %s -> %s\n", auth_result_to_message(res), (res == AUTH_SUCCESS && session.is_authenticated) ? "PASS" : "FAIL");
    if (res == AUTH_SUCCESS) {
        printf("Session verified for Customer: %s, Account: %s (%s)\n",
               session.customer_name, session.account_number, session.account_type);
        auth_logout(&session);
    }

    /* Test 4: Valid card + wrong PIN once -> denied, card remains ACTIVE */
    printf("\n[TEST 4] Valid Card + Incorrect PIN (Single attempt):\n");
    /* Seeded Card 1: '4532015012340001' */
    card_reset_failed_attempts(1);
    res = auth_authenticate_customer("4532015012340001", "0000", &session, &remaining);
    printf("Result: %s, Attempts remaining: %u -> %s\n",
           auth_result_to_message(res), remaining,
           (res == AUTH_ERR_WRONG_PIN && remaining == 2) ? "PASS" : "FAIL");

    /* Test 5: Wrong PIN 3 times -> Card becomes BLOCKED */
    printf("\n[TEST 5] Three Consecutive Wrong PIN Attempts -> Auto-block:\n");
    card_reset_failed_attempts(1);
    auth_authenticate_customer("4532015012340001", "0000", &session, &remaining); /* Attempt 1 */
    auth_authenticate_customer("4532015012340001", "0000", &session, &remaining); /* Attempt 2 */
    res = auth_authenticate_customer("4532015012340001", "0000", &session, &remaining); /* Attempt 3 */
    printf("Attempt 3 Result: %s -> %s\n",
           auth_result_to_message(res),
           (res == AUTH_ERR_CARD_NOW_BLOCKED) ? "PASS" : "FAIL");

    /* Test 6: Attempt login on now-BLOCKED card */
    printf("\n[TEST 6] Login Attempt on BLOCKED Card:\n");
    res = auth_authenticate_customer("4532015012340001", "1234", &session, &remaining);
    printf("Result: %s -> %s\n",
           auth_result_to_message(res),
           (res == AUTH_ERR_CARD_BLOCKED) ? "PASS" : "FAIL");

    /* Reset Card 1 back to ACTIVE for future operations */
    card_update_status(1, CARD_STATUS_ACTIVE);
    card_reset_failed_attempts(1);

    /* Test 7: EXPIRED card login denial */
    printf("\n[TEST 7] Login Attempt on EXPIRED Card:\n");
    card_update_status(1, CARD_STATUS_EXPIRED);
    res = auth_authenticate_customer("4532015012340001", "1234", &session, &remaining);
    printf("Result: %s -> %s\n",
           auth_result_to_message(res),
           (res == AUTH_ERR_CARD_EXPIRED) ? "PASS" : "FAIL");

    /* Test 8: CANCELLED card login denial */
    printf("\n[TEST 8] Login Attempt on CANCELLED Card:\n");
    card_update_status(1, CARD_STATUS_CANCELLED);
    res = auth_authenticate_customer("4532015012340001", "1234", &session, &remaining);
    printf("Result: %s -> %s\n",
           auth_result_to_message(res),
           (res == AUTH_ERR_CARD_CANCELLED) ? "PASS" : "FAIL");

    /* Restore Card 1 cleanly to original ACTIVE state */
    card_update_status(1, CARD_STATUS_ACTIVE);
    card_reset_failed_attempts(1);
    printf("\nCard 1 state restored to ACTIVE.\n");
    printf("--- ALL TESTS COMPLETED ---\n\n");
}

int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("       ATM SIMULATION SYSTEM\n");
    printf("========================================\n\n");

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

    /* Check if automated test mode requested via flag */
    if (argc > 1 && strcmp(argv[1], "--test-auth") == 0) {
        run_automated_auth_tests();
        db_disconnect();
        return 0;
    }

    /* Interactive Authentication Mode */
    printf("Phase 4: Customer Authentication System\n\n");

    char card_input[32];
    printf("Please enter your card number: ");
    if (!fgets(card_input, sizeof(card_input), stdin)) {
        db_disconnect();
        return 1;
    }
    validation_trim(card_input);

    if (!validation_is_valid_card_number(card_input)) {
        printf("\n[ERROR] Invalid card number format.\n");
        db_disconnect();
        return 1;
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
            printf("Account Number: %s (%s)\n", session.account_number, session.account_type);
            printf("Card Number   : XXXX-XXXX-XXXX-%s\n",
                   session.card_number + (strlen(session.card_number) > 4 ? strlen(session.card_number) - 4 : 0));
            printf("Session established successfully.\n");
            auth_logout(&session);
            break;
        } else if (res == AUTH_ERR_WRONG_PIN) {
            printf("\n[ERROR] %s (Attempts remaining: %u)\n\n", auth_result_to_message(res), remaining);
        } else {
            printf("\n[ERROR] %s\n", auth_result_to_message(res));
            break;
        }
    }

    db_disconnect();
    return 0;
}
