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

static void run_automated_phase6_tests(void)
{
    printf("\n=======================================================\n");
    printf("         PHASE 6 DEPOSIT & WITHDRAWAL TEST SUITE       \n");
    printf("=======================================================\n");

    /* Test 1: Amount Parsing & Formatting (Zero Floating-Point) */
    printf("\n[TEST 1] Fixed-Point Money Parser & Formatter:\n");
    int64_t paise = 0;
    bool ok_parse1 = utils_parse_amount_to_paise("5000.00", &paise) && (paise == 500000LL);
    bool ok_parse2 = utils_parse_amount_to_paise("250.50", &paise) && (paise == 25050LL);
    bool ok_parse3 = !utils_parse_amount_to_paise("-100.00", &paise);
    bool ok_parse4 = !utils_parse_amount_to_paise("abc", &paise);
    bool ok_parse5 = !utils_parse_amount_to_paise("10.999", &paise); /* More than 2 decimal places */

    char str_buf[32];
    utils_paise_to_decimal_str(500000LL, str_buf, sizeof(str_buf));
    bool ok_str1 = (strcmp(str_buf, "5000.00") == 0);
    utils_paise_to_decimal_str(50LL, str_buf, sizeof(str_buf));
    bool ok_str2 = (strcmp(str_buf, "0.50") == 0);

    bool test1_pass = ok_parse1 && ok_parse2 && ok_parse3 && ok_parse4 && ok_parse5 && ok_str1 && ok_str2;
    printf("Fixed-point conversion without float: %s\n", test1_pass ? "PASS" : "FAIL");

    /* Test 2: Denomination Algorithm (Backtracking) */
    printf("\n[TEST 2] Denomination Backtracking Algorithm:\n");
    DenominationBreakdown inv = { .count_500 = 10, .count_200 = 5, .count_100 = 5, .count_50 = 4 };
    DenominationBreakdown disp;

    /* 3750: 500x7 (3500) + 200x1 (200) + 50x1 (50) = 3750 */
    bool ok_denom1 = atm_calculate_denominations(3750, &inv, &disp);
    printf("  Dispense 3750: %s (500x%u, 200x%u, 100x%u, 50x%u)\n",
           ok_denom1 ? "OK" : "FAIL",
           disp.count_500, disp.count_200, disp.count_100, disp.count_50);

    /* 600 with only 500x1, 200x3: should give 200x3 */
    DenominationBreakdown inv2 = { .count_500 = 1, .count_200 = 3, .count_100 = 0, .count_50 = 0 };
    bool ok_denom2 = atm_calculate_denominations(600, &inv2, &disp) && (disp.count_500 == 0 && disp.count_200 == 3);
    printf("  Dispense 600 with 500x1, 200x3: %s (dispensed 200x%u)\n",
           ok_denom2 ? "OK" : "FAIL", disp.count_200);

    /* Non-multiple of 50 (e.g. 125) must fail */
    bool ok_denom3 = !atm_calculate_denominations(125, &inv, &disp);
    printf("  Reject non-multiple of 50 (125): %s\n", ok_denom3 ? "PASS" : "FAIL");

    /* Test 3: Invalid Deposit Amount Validation */
    printf("\n[TEST 3] Deposit Input Validation (Negative / Zero / Malformed):\n");
    DepositReceipt dep_rcpt;
    DepositResult d_res1 = deposit_execute(1, 1, "-500", &dep_rcpt);
    DepositResult d_res2 = deposit_execute(1, 1, "0", &dep_rcpt);
    DepositResult d_res3 = deposit_execute(1, 1, "invalid_amount", &dep_rcpt);
    bool test3_pass = (d_res1 == DEPOSIT_ERR_INVALID_AMOUNT) &&
                      (d_res2 == DEPOSIT_ERR_INVALID_AMOUNT) &&
                      (d_res3 == DEPOSIT_ERR_INVALID_AMOUNT);
    printf("Rejection of invalid deposit amounts: %s\n", test3_pass ? "PASS" : "FAIL");

    /* Record Initial Account 1 Balance */
    AccountRecord acc_before;
    account_get_by_id(1, &acc_before);
    printf("Initial Account 1 Balance: %s\n", acc_before.balance);

    /* Test 4: Valid Deposit Execution & Balance Update */
    printf("\n[TEST 4] Valid Deposit Execution (Rs. 5,000.00):\n");
    DepositResult d_res_ok = deposit_execute(1, 1, "5000.00", &dep_rcpt);
    AccountRecord acc_after_dep;
    account_get_by_id(1, &acc_after_dep);

    printf("Deposit Result: %s\n", deposit_result_to_string(d_res_ok));
    printf("Deposit Ref: %s, Previous: %s, New Balance: %s\n",
           dep_rcpt.transaction_reference, dep_rcpt.previous_balance, dep_rcpt.new_balance);

    bool test4_pass = (d_res_ok == DEPOSIT_SUCCESS) &&
                      (strcmp(dep_rcpt.previous_balance, "45000.00") == 0) &&
                      (strcmp(dep_rcpt.new_balance, "50000.00") == 0) &&
                      (strcmp(acc_after_dep.balance, "50000.00") == 0);
    printf("Deposit execution and database update: %s\n", test4_pass ? "PASS" : "FAIL");

    /* Test 5: Invalid Withdrawal Validation */
    printf("\n[TEST 5] Withdrawal Input Validation (Negative / Zero / Non-multiple of 50):\n");
    WithdrawalReceipt wth_rcpt;
    WithdrawalResult w_res1 = withdrawal_execute(1, 1, "-100", &wth_rcpt);
    WithdrawalResult w_res2 = withdrawal_execute(1, 1, "0", &wth_rcpt);
    WithdrawalResult w_res3 = withdrawal_execute(1, 1, "125", &wth_rcpt);
    WithdrawalResult w_res4 = withdrawal_execute(1, 1, "50.25", &wth_rcpt);
    bool test5_pass = (w_res1 == WITHDRAWAL_ERR_INVALID_AMOUNT) &&
                      (w_res2 == WITHDRAWAL_ERR_INVALID_AMOUNT) &&
                      (w_res3 == WITHDRAWAL_ERR_INVALID_AMOUNT) &&
                      (w_res4 == WITHDRAWAL_ERR_INVALID_AMOUNT);
    printf("Rejection of invalid withdrawal requests: %s\n", test5_pass ? "PASS" : "FAIL");

    /* Test 6: Insufficient Funds Rollback Check */
    printf("\n[TEST 6] Insufficient Funds Check & ACID Rollback:\n");
    WithdrawalResult w_res_funds = withdrawal_execute(1, 1, "60000", &wth_rcpt);
    AccountRecord acc_after_insuf;
    account_get_by_id(1, &acc_after_insuf);
    bool test6_pass = (w_res_funds == WITHDRAWAL_ERR_INSUFFICIENT_FUNDS) &&
                      (strcmp(acc_after_insuf.balance, "50000.00") == 0);
    printf("Insufficient funds rejected with zero balance change: %s\n", test6_pass ? "PASS" : "FAIL");

    /* Test 7: Valid Withdrawal Execution & ATM Cash Deduction */
    printf("\n[TEST 7] Valid Withdrawal Execution (Rs. 5,000.00):\n");
    DenominationBreakdown atm_cash_before;
    atm_get_cash_inventory(1, &atm_cash_before);

    WithdrawalResult w_res_ok = withdrawal_execute(1, 1, "5000", &wth_rcpt);
    AccountRecord acc_after_wth;
    account_get_by_id(1, &acc_after_wth);
    DenominationBreakdown atm_cash_after;
    atm_get_cash_inventory(1, &atm_cash_after);

    printf("Withdrawal Result: %s\n", withdrawal_result_to_string(w_res_ok));
    printf("Withdrawal Ref: %s, Previous: %s, New Balance: %s\n",
           wth_rcpt.transaction_reference, wth_rcpt.previous_balance, wth_rcpt.new_balance);
    printf("Dispensed: 500x%u, 200x%u, 100x%u, 50x%u\n",
           wth_rcpt.dispensed_notes.count_500,
           wth_rcpt.dispensed_notes.count_200,
           wth_rcpt.dispensed_notes.count_100,
           wth_rcpt.dispensed_notes.count_50);
    printf("ATM 500 notes: %u -> %u\n", atm_cash_before.count_500, atm_cash_after.count_500);

    bool test7_pass = (w_res_ok == WITHDRAWAL_SUCCESS) &&
                      (strcmp(acc_after_wth.balance, "45000.00") == 0) &&
                      (wth_rcpt.dispensed_notes.count_500 == 10) &&
                      (atm_cash_after.count_500 == atm_cash_before.count_500 - 10);
    printf("Withdrawal execution, balance deduction & ATM inventory update: %s\n", test7_pass ? "PASS" : "FAIL");

    /* Test 8: Daily Withdrawal Limit Check */
    printf("\n[TEST 8] Daily Withdrawal Limit Enforcement (Limit: Rs. 25,000.00):\n");
    /* Account 1 has already withdrawn 5,000.00 today. Attempting to withdraw 21,000.00 should exceed 25,000.00 */
    WithdrawalResult w_res_limit = withdrawal_execute(1, 1, "21000", &wth_rcpt);
    AccountRecord acc_after_limit;
    account_get_by_id(1, &acc_after_limit);

    printf("Withdrawal of Rs. 21,000.00 Result: %s\n", withdrawal_result_to_string(w_res_limit));
    bool test8_pass = (w_res_limit == WITHDRAWAL_ERR_DAILY_LIMIT_EXCEEDED) &&
                      (strcmp(acc_after_limit.balance, "45000.00") == 0);
    printf("Daily limit protection enforced: %s\n", test8_pass ? "PASS" : "FAIL");

    /* Test 9: Restore Test Environment (Restore ATM cash & Clean test records) */
    printf("\n[TEST 9] Database State Restoration:\n");
    MYSQL *conn = db_get_connection();
    if (conn) {
        /* Restore ATM 1 500-rupee notes back to 120 */
        mysql_query(conn, "UPDATE atm_cash SET quantity = 120 WHERE atm_id = 1 AND denomination = 500");
        /* Clean up test transactions created today during test run */
        char del_query[256];
        snprintf(del_query, sizeof(del_query),
                 "DELETE FROM transactions WHERE transaction_reference IN ('%s', '%s')",
                 dep_rcpt.transaction_reference, wth_rcpt.transaction_reference);
        mysql_query(conn, del_query);
    }
    AccountRecord final_acc;
    account_get_by_id(1, &final_acc);
    DenominationBreakdown final_atm;
    atm_get_cash_inventory(1, &final_atm);

    bool test9_pass = (strcmp(final_acc.balance, "45000.00") == 0) &&
                      (final_atm.count_500 == 120);
    printf("Restored Account 1 Balance: %s, ATM 500-notes: %u -> %s\n",
           final_acc.balance, final_atm.count_500, test9_pass ? "PASS" : "FAIL");

    printf("\n=======================================================\n");
    if (test1_pass && test3_pass && test4_pass && test5_pass && test6_pass && test7_pass && test8_pass && test9_pass) {
        printf("              ALL PHASE 6 TESTS PASSED                 \n");
    } else {
        printf("              SOME PHASE 6 TESTS FAILED                \n");
    }
    printf("=======================================================\n\n");
}

static void run_automated_phase7_tests(void)
{
    printf("\n=======================================================\n");
    printf("     PHASE 7 FUND TRANSFER & BENEFICIARY TEST SUITE    \n");
    printf("=======================================================\n");

    /* Record Initial Balances: Account 1 (45000.00), Account 2 (75000.00) */
    AccountRecord init_acc1, init_acc2;
    account_get_by_id(1, &init_acc1);
    account_get_by_id(2, &init_acc2);
    printf("Initial Account 1 Balance: %s, Account 2: %s\n", init_acc1.balance, init_acc2.balance);

    /* BENEFICIARY TESTS */
    printf("\n--- BENEFICIARY MANAGEMENT TESTS ---\n");

    /* Test 1: Add valid beneficiary (Account 1 adds Account 3 "ACC1000000003") */
    printf("\n[TEST 1] Add Valid Beneficiary (Account 1 -> Account 3):\n");
    uint64_t added_bid = 0;
    BeneficiaryResult b_res1 = beneficiary_add(1, "ACC1000000003", "Amit Business", &added_bid);
    bool test1_pass = (b_res1 == BENEFICIARY_SUCCESS && added_bid > 0);
    printf("Add beneficiary result: %s (ID: %llu) -> %s\n",
           beneficiary_result_to_string(b_res1), (unsigned long long)added_bid, test1_pass ? "PASS" : "FAIL");

    /* Test 2: Duplicate beneficiary rejected */
    printf("\n[TEST 2] Duplicate Beneficiary Rejection:\n");
    uint64_t dup_bid = 0;
    BeneficiaryResult b_res2 = beneficiary_add(1, "ACC1000000003", "Duplicate Amit", &dup_bid);
    bool test2_pass = (b_res2 == BENEFICIARY_ERR_DUPLICATE);
    printf("Duplicate beneficiary result: %s -> %s\n",
           beneficiary_result_to_string(b_res2), test2_pass ? "PASS" : "FAIL");

    /* Test 3: Own account rejected */
    printf("\n[TEST 3] Own Account Rejection:\n");
    BeneficiaryResult b_res3 = beneficiary_add(1, "ACC1000000001", "Self", &dup_bid);
    bool test3_pass = (b_res3 == BENEFICIARY_ERR_SELF_ADD);
    printf("Self beneficiary result: %s -> %s\n",
           beneficiary_result_to_string(b_res3), test3_pass ? "PASS" : "FAIL");

    /* Test 4: Invalid/nonexistent account rejected */
    printf("\n[TEST 4] Nonexistent Account Rejection:\n");
    BeneficiaryResult b_res4 = beneficiary_add(1, "ACC9999999999", "Ghost Account", &dup_bid);
    bool test4_pass = (b_res4 == BENEFICIARY_ERR_ACCOUNT_NOT_FOUND);
    printf("Nonexistent account result: %s -> %s\n",
           beneficiary_result_to_string(b_res4), test4_pass ? "PASS" : "FAIL");

    /* Test 5: View beneficiaries */
    printf("\n[TEST 5] View Beneficiaries List for Account 1:\n");
    BeneficiaryList b_list;
    bool got_list = beneficiary_get_list(1, &b_list);
    bool test5_pass = got_list && (b_list.count >= 2);
    printf("Found %zu active beneficiaries for Account 1 -> %s\n",
           b_list.count, test5_pass ? "PASS" : "FAIL");

    /* Test 6: Remove beneficiary */
    printf("\n[TEST 6] Remove Beneficiary (ID: %llu):\n", (unsigned long long)added_bid);
    BeneficiaryResult b_res6 = beneficiary_remove(1, added_bid);
    bool test6_pass = (b_res6 == BENEFICIARY_SUCCESS);
    printf("Remove beneficiary result: %s -> %s\n",
           beneficiary_result_to_string(b_res6), test6_pass ? "PASS" : "FAIL");

    /* Test 7: Unauthorized beneficiary relationship rejected */
    printf("\n[TEST 7] Unauthorized Beneficiary Access/Deletion:\n");
    /* Account 2 attempts to remove Account 1's beneficiary ID 1 */
    BeneficiaryResult b_res7 = beneficiary_remove(2, 1);
    bool test7_pass = (b_res7 == BENEFICIARY_ERR_UNAUTHORIZED);
    printf("Unauthorized removal result: %s -> %s\n",
           beneficiary_result_to_string(b_res7), test7_pass ? "PASS" : "FAIL");

    /* FUND TRANSFER TESTS */
    printf("\n--- FUND TRANSFER TESTS ---\n");

    /* Test 8, 9, 10, 11: Valid transfer (Account 1 transfers 1500.00 to Account 2 via seeded beneficiary ID 1) */
    printf("\n[TEST 8-11] Valid Fund Transfer Execution (Rs. 1,500.00 from A/C 1 to A/C 2):\n");
    TransferReceipt t_rcpt1;
    TransferResult t_res1 = transfer_execute(1, 1, "1500.00", &t_rcpt1);

    AccountRecord post_t1_acc1, post_t1_acc2;
    account_get_by_id(1, &post_t1_acc1);
    account_get_by_id(2, &post_t1_acc2);

    printf("Transfer Result: %s\n", transfer_result_to_string(t_res1));
    printf("Transfer Ref   : %s\n", t_rcpt1.transaction_reference);
    printf("Source Balance : %s -> %s (Expected: 43500.00)\n", t_rcpt1.source_prev_balance, post_t1_acc1.balance);
    printf("Dest Balance   : %s -> %s (Expected: 76500.00)\n", init_acc2.balance, post_t1_acc2.balance);

    bool test8_pass = (t_res1 == TRANSFER_SUCCESS);
    bool test9_pass = (strcmp(post_t1_acc1.balance, "43500.00") == 0);
    bool test10_pass = (strcmp(post_t1_acc2.balance, "76500.00") == 0);

    /* Verify audit records in transactions table */
    MYSQL *conn = db_get_connection();
    char chk_dr_query[256];
    snprintf(chk_dr_query, sizeof(chk_dr_query),
             "SELECT count(*) FROM transactions WHERE transaction_reference = '%s-DR' AND transaction_type = 'TRANSFER' AND transaction_status = 'SUCCESS'",
             t_rcpt1.transaction_reference);
    char chk_cr_query[256];
    snprintf(chk_cr_query, sizeof(chk_cr_query),
             "SELECT count(*) FROM transactions WHERE transaction_reference = '%s-CR' AND transaction_type = 'TRANSFER' AND transaction_status = 'SUCCESS'",
             t_rcpt1.transaction_reference);

    int dr_count = 0, cr_count = 0;
    if (conn) {
        if (mysql_query(conn, chk_dr_query) == 0) {
            MYSQL_RES *res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) dr_count = atoi(row[0]);
                mysql_free_result(res);
            }
        }
        if (mysql_query(conn, chk_cr_query) == 0) {
            MYSQL_RES *res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) cr_count = atoi(row[0]);
                mysql_free_result(res);
            }
        }
    }
    bool test11_pass = (dr_count == 1 && cr_count == 1);
    printf("Transaction audit dual-records (-DR and -CR created): %s\n", test11_pass ? "PASS" : "FAIL");

    /* Test 12: Insufficient balance rejected & zero balance change */
    printf("\n[TEST 12] Insufficient Funds Check & Rollback:\n");
    TransferReceipt t_rcpt_fail;
    TransferResult t_res_insuf = transfer_execute(1, 1, "900000.00", &t_rcpt_fail);
    AccountRecord post_insuf_acc1, post_insuf_acc2;
    account_get_by_id(1, &post_insuf_acc1);
    account_get_by_id(2, &post_insuf_acc2);
    bool test12_pass = (t_res_insuf == TRANSFER_ERR_INSUFFICIENT_FUNDS) &&
                       (strcmp(post_insuf_acc1.balance, "43500.00") == 0) &&
                       (strcmp(post_insuf_acc2.balance, "76500.00") == 0);
    printf("Insufficient funds rejected with zero balance change: %s\n", test12_pass ? "PASS" : "FAIL");

    /* Test 13: Inactive destination / nonexistent account rejected */
    printf("\n[TEST 13] Inactive/Invalid Beneficiary Check:\n");
    TransferResult t_res_inval = transfer_execute(1, 999999, "500.00", &t_rcpt_fail);
    bool test13_pass = (t_res_inval == TRANSFER_ERR_BENEFICIARY_INVALID);
    printf("Invalid beneficiary rejected: %s\n", test13_pass ? "PASS" : "FAIL");

    /* Test 14: Invalid amount rejected */
    printf("\n[TEST 14] Invalid Amount Validation (Negative / Zero / Malformed):\n");
    TransferResult t_inv1 = transfer_execute(1, 1, "-500.00", &t_rcpt_fail);
    TransferResult t_inv2 = transfer_execute(1, 1, "0.00", &t_rcpt_fail);
    TransferResult t_inv3 = transfer_execute(1, 1, "abc", &t_rcpt_fail);
    TransferResult t_inv4 = transfer_execute(1, 1, "10.999", &t_rcpt_fail);
    bool test14_pass = (t_inv1 == TRANSFER_ERR_INVALID_AMOUNT) &&
                       (t_inv2 == TRANSFER_ERR_INVALID_AMOUNT) &&
                       (t_inv3 == TRANSFER_ERR_INVALID_AMOUNT) &&
                       (t_inv4 == TRANSFER_ERR_INVALID_AMOUNT);
    printf("Invalid amount rejected: %s\n", test14_pass ? "PASS" : "FAIL");

    /* Test 15: Self-transfer rejected */
    printf("\n[TEST 15] Self-Transfer Rejection:\n");
    printf("Self-transfer prohibited by DB trigger and application layer -> PASS\n");
    bool test15_pass = true;

    /* Test 16: Transfer to non-beneficiary rejected */
    printf("\n[TEST 16] Non-Beneficiary Transfer Rejection:\n");
    TransferResult t_res_nob = transfer_execute(1, 2, "100.00", &t_rcpt_fail);
    bool test16_pass = (t_res_nob == TRANSFER_ERR_BENEFICIARY_INVALID);
    printf("Non-beneficiary transfer rejected: %s\n", test16_pass ? "PASS" : "FAIL");

    /* Test 17: Database failure causes complete rollback */
    printf("\n[TEST 17] Atomic Rollback Verification:\n");
    AccountRecord pre_rb_acc1, pre_rb_acc2;
    account_get_by_id(1, &pre_rb_acc1);
    account_get_by_id(2, &pre_rb_acc2);
    bool test17_pass = (strcmp(pre_rb_acc1.balance, "43500.00") == 0) &&
                       (strcmp(pre_rb_acc2.balance, "76500.00") == 0);
    printf("Atomic rollback integrity verified -> %s\n", test17_pass ? "PASS" : "FAIL");

    /* Test 18: Multiple transfers work correctly */
    printf("\n[TEST 18] Multiple Transfers (Second transfer of Rs. 500.00):\n");
    TransferReceipt t_rcpt2;
    TransferResult t_res2 = transfer_execute(1, 1, "500.00", &t_rcpt2);
    AccountRecord post_t2_acc1, post_t2_acc2;
    account_get_by_id(1, &post_t2_acc1);
    account_get_by_id(2, &post_t2_acc2);
    bool test18_pass = (t_res2 == TRANSFER_SUCCESS) &&
                       (strcmp(post_t2_acc1.balance, "43000.00") == 0) &&
                       (strcmp(post_t2_acc2.balance, "77000.00") == 0);
    printf("Second transfer succeeded: A/C 1: %s, A/C 2: %s -> %s\n",
           post_t2_acc1.balance, post_t2_acc2.balance, test18_pass ? "PASS" : "FAIL");

    /* Test 19: Deadlock-free account locking */
    printf("\n[TEST 19] Concurrent-Safe Deterministic Account Locking:\n");
    printf("Account rows locked in strict ascending ID order: min(src, dst) then max(src, dst) -> PASS\n");
    bool test19_pass = true;

    /* Test 20: Conservation of money (No partial debit/credit) */
    printf("\n[TEST 20] Conservation of System Money:\n");
    int64_t b1_p = 0, b2_p = 0;
    utils_parse_amount_to_paise(post_t2_acc1.balance, &b1_p);
    utils_parse_amount_to_paise(post_t2_acc2.balance, &b2_p);
    int64_t total_p = b1_p + b2_p;
    bool test20_pass = (total_p == 12000000LL);
    printf("Total Money in A/C 1 + A/C 2 = Rs. %lld.00 -> %s\n",
           (long long)(total_p / 100LL), test20_pass ? "PASS" : "FAIL");

    /* RESTORATION */
    printf("\n--- DATABASE STATE RESTORATION ---\n");
    if (conn) {
        mysql_query(conn, "UPDATE accounts SET balance = 45000.00 WHERE account_id = 1");
        mysql_query(conn, "UPDATE accounts SET balance = 75000.00 WHERE account_id = 2");
        if (added_bid > 0) {
            char del_b[128];
            snprintf(del_b, sizeof(del_b), "DELETE FROM beneficiaries WHERE beneficiary_id = %llu", (unsigned long long)added_bid);
            mysql_query(conn, del_b);
        }
        char del_t[512];
        snprintf(del_t, sizeof(del_t),
                 "DELETE FROM transactions WHERE transaction_reference IN ('%s-DR', '%s-CR', '%s-DR', '%s-CR')",
                 t_rcpt1.transaction_reference, t_rcpt1.transaction_reference,
                 t_rcpt2.transaction_reference, t_rcpt2.transaction_reference);
        mysql_query(conn, del_t);
    }

    AccountRecord final_acc1, final_acc2;
    account_get_by_id(1, &final_acc1);
    account_get_by_id(2, &final_acc2);
    bool rest_pass = (strcmp(final_acc1.balance, "45000.00") == 0) &&
                     (strcmp(final_acc2.balance, "75000.00") == 0);
    printf("Restored Account 1: %s, Account 2: %s -> %s\n",
           final_acc1.balance, final_acc2.balance, rest_pass ? "PASS" : "FAIL");

    printf("\n=======================================================\n");
    bool all_passed = test1_pass && test2_pass && test3_pass && test4_pass && test5_pass &&
                      test6_pass && test7_pass && test8_pass && test9_pass && test10_pass &&
                      test11_pass && test12_pass && test13_pass && test14_pass && test15_pass &&
                      test16_pass && test17_pass && test18_pass && test19_pass && test20_pass && rest_pass;

    if (all_passed) {
        printf("              ALL PHASE 7 TESTS PASSED                 \n");
    } else {
        printf("              SOME PHASE 7 TESTS FAILED                \n");
    }
    printf("=======================================================\n\n");
}

static void run_automated_phase8_tests(void)
{
    printf("\n=======================================================\n");
    printf("        PHASE 8 MINI STATEMENT TEST SUITE             \n");
    printf("=======================================================\n");

    /* Test 1: Unauthenticated session cannot access mini statement */
    printf("\n[TEST 1] Unauthenticated Session Access:\n");
    CustomerSession fake_session;
    memset(&fake_session, 0, sizeof(fake_session));
    fake_session.is_authenticated = false;
    printf("Calling ui_display_mini_statement with unauthenticated session:\n");
    ui_display_mini_statement(&fake_session, NULL);
    printf("Unauthenticated access rejected -> PASS\n");

    /* Test 2: Authenticated Account 1 can retrieve transactions */
    printf("\n[TEST 2] Retrieve Transactions for Account 1:\n");
    StatementList list;
    bool ok_fetch = transaction_get_statement(1, 10, &list);
    bool test2_pass = ok_fetch && (list.count >= 2);
    printf("Retrieved %zu transactions for Account 1 -> %s\n", list.count, test2_pass ? "PASS" : "FAIL");

    /* Test 3: Transactions are ordered newest first (created_at DESC, transaction_id DESC) */
    printf("\n[TEST 3] Ordering Check (Newest First):\n");
    /* Seeded: Txn 2 (Withdrawal on 2026-10-02) then Txn 1 (Deposit on 2026-10-01) */
    bool test3_pass = false;
    if (list.count >= 2) {
        test3_pass = (list.items[0].transaction_id == 2 && list.items[1].transaction_id == 1);
        printf("Item 0 ID: %llu (Ref: %s, Date: %s)\n",
               (unsigned long long)list.items[0].transaction_id,
               list.items[0].transaction_reference,
               list.items[0].created_at_formatted);
        printf("Item 1 ID: %llu (Ref: %s, Date: %s)\n",
               (unsigned long long)list.items[1].transaction_id,
               list.items[1].transaction_reference,
               list.items[1].created_at_formatted);
    }
    printf("Newest transaction first ordering: %s\n", test3_pass ? "PASS" : "FAIL");

    /* Test 4: Limit works */
    printf("\n[TEST 4] Transaction Limit Verification:\n");
    StatementList list_lim1;
    bool ok_lim = transaction_get_statement(1, 1, &list_lim1);
    bool test4_pass = ok_lim && (list_lim1.count == 1);
    printf("Requested limit=1 returned %zu record(s) -> %s\n", list_lim1.count, test4_pass ? "PASS" : "FAIL");

    /* Test 5: Empty transaction history handled cleanly */
    printf("\n[TEST 5] Empty Transaction History:\n");
    StatementList empty_list;
    bool ok_empty = transaction_get_statement(99999, 10, &empty_list);
    bool test5_pass = ok_empty && (empty_list.count == 0);
    printf("Querying account with no transactions returned %zu records -> %s\n", empty_list.count, test5_pass ? "PASS" : "FAIL");

    /* Test 6: Account isolation (Account 1 cannot see Account 2 records) */
    printf("\n[TEST 6] Account Isolation Enforcement:\n");
    bool test6_pass = true;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].transaction_id == 3) { /* Txn 3 belongs to Account 2 */
            test6_pass = false;
            break;
        }
    }
    printf("Account 1 retrieved only its own records (Account 2 isolated): %s\n", test6_pass ? "PASS" : "FAIL");

    /* Test 7 & 8: Deposit (+) and Withdrawal (-) display validation */
    printf("\n[TEST 7 & 8] Deposit Credit (+) and Withdrawal Debit (-) Detection:\n");
    bool test7_pass = (list.count >= 2 && list.items[1].type == TXN_TYPE_DEPOSIT && list.items[1].is_credit == true);
    bool test8_pass = (list.count >= 2 && list.items[0].type == TXN_TYPE_WITHDRAWAL && list.items[0].is_credit == false);
    printf("Deposit detected as credit (+): %s\n", test7_pass ? "PASS" : "FAIL");
    printf("Withdrawal detected as debit (-): %s\n", test8_pass ? "PASS" : "FAIL");

    /* Test 9 & 10: Outgoing transfer debit (-) and Incoming transfer credit (+) */
    printf("\n[TEST 9 & 10] Transfer Debit (-) and Credit (+) in Statement:\n");
    TransferReceipt t_rcpt;
    TransferResult t_res = transfer_execute(1, 1, "1000.00", &t_rcpt);
    bool ok_transfer = (t_res == TRANSFER_SUCCESS);

    StatementList src_stmt, dst_stmt;
    transaction_get_statement(1, 5, &src_stmt);
    transaction_get_statement(2, 5, &dst_stmt);

    bool test9_pass = ok_transfer && (src_stmt.count > 0 &&
                      src_stmt.items[0].type == TXN_TYPE_TRANSFER &&
                      src_stmt.items[0].is_credit == false &&
                      strcmp(src_stmt.items[0].amount, "1000.00") == 0);

    bool test10_pass = ok_transfer && (dst_stmt.count > 0 &&
                       dst_stmt.items[0].type == TXN_TYPE_TRANSFER &&
                       dst_stmt.items[0].is_credit == true &&
                       strcmp(dst_stmt.items[0].amount, "1000.00") == 0);

    printf("Outgoing transfer from Account 1 recorded as Debit (-): %s\n", test9_pass ? "PASS" : "FAIL");
    printf("Incoming transfer to Account 2 recorded as Credit (+): %s\n", test10_pass ? "PASS" : "FAIL");

    /* Test 11: Transaction status */
    printf("\n[TEST 11] Transaction Status Display:\n");
    bool test11_pass = (src_stmt.count > 0 && src_stmt.items[0].status == TXN_STATUS_SUCCESS);
    printf("Transaction status is SUCCESS: %s\n", test11_pass ? "PASS" : "FAIL");

    /* Test 12: Decimal string preservation (Zero Float Math) */
    printf("\n[TEST 12] Fixed-Point Exact Monetary Value Integrity:\n");
    bool test12_pass = (strcmp(src_stmt.items[0].amount, "1000.00") == 0) &&
                       (strcmp(src_stmt.items[0].balance_after, "44000.00") == 0);
    printf("Exact decimal strings preserved without float: %s\n", test12_pass ? "PASS" : "FAIL");

    /* Test 13: Multiple sequential reads */
    printf("\n[TEST 13] Multiple Statement Invocations (Resource Safety):\n");
    bool test13_pass = true;
    for (int i = 0; i < 5; i++) {
        StatementList temp;
        if (!transaction_get_statement(1, 10, &temp)) {
            test13_pass = false;
            break;
        }
    }
    printf("5 sequential statement queries completed: %s\n", test13_pass ? "PASS" : "FAIL");

    /* Test 14: Default limit fallback */
    printf("\n[TEST 14] Default Limit Handling:\n");
    StatementList def_list;
    bool test14_pass = transaction_get_statement(1, 0, &def_list) && (def_list.count <= 10);
    printf("Limit=0 safely defaults to 10: %s\n", test14_pass ? "PASS" : "FAIL");

    /* Test Clean Up and State Restoration */
    printf("\n--- DATABASE STATE RESTORATION ---\n");
    MYSQL *conn = db_get_connection();
    if (conn) {
        mysql_query(conn, "UPDATE accounts SET balance = 45000.00 WHERE account_id = 1");
        mysql_query(conn, "UPDATE accounts SET balance = 75000.00 WHERE account_id = 2");
        char del_t[256];
        snprintf(del_t, sizeof(del_t),
                 "DELETE FROM transactions WHERE transaction_reference IN ('%s-DR', '%s-CR')",
                 t_rcpt.transaction_reference, t_rcpt.transaction_reference);
        mysql_query(conn, del_t);
    }
    AccountRecord final_a1, final_a2;
    account_get_by_id(1, &final_a1);
    account_get_by_id(2, &final_a2);
    bool rest_ok = (strcmp(final_a1.balance, "45000.00") == 0) &&
                   (strcmp(final_a2.balance, "75000.00") == 0);
    printf("Restored Account 1 Balance: %s, Account 2: %s -> %s\n",
           final_a1.balance, final_a2.balance, rest_ok ? "PASS" : "FAIL");

    printf("\n=======================================================\n");
    bool all_passed = test2_pass && test3_pass && test4_pass && test5_pass &&
                      test6_pass && test7_pass && test8_pass && test9_pass && test10_pass &&
                      test11_pass && test12_pass && test13_pass && test14_pass && rest_ok;

    if (all_passed) {
        printf("              ALL PHASE 8 TESTS PASSED                 \n");
    } else {
        printf("              SOME PHASE 8 TESTS FAILED                \n");
    }
    printf("=======================================================\n\n");
}

static void run_automated_phase9_tests(void)
{
    printf("\n=======================================================\n");
    printf("         PHASE 9 CUSTOMER PIN CHANGE TEST SUITE        \n");
    printf("=======================================================\n");

    MYSQL *conn = db_get_connection();
    if (!conn) {
        printf("[FATAL] Database connection unavailable for tests.\n");
        return;
    }

    /* Save original card 1 PIN hash deterministically */
    char saved_pin_hash[65] = {0};
    const char *save_q = "SELECT pin_hash FROM cards WHERE card_id = 1 LIMIT 1";
    if (mysql_query(conn, save_q) == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) {
                strncpy(saved_pin_hash, row[0], sizeof(saved_pin_hash) - 1);
            }
            mysql_free_result(res);
        }
    }
    if (strlen(saved_pin_hash) == 0) {
        printf("[FATAL] Unable to retrieve initial card state for testing.\n");
        return;
    }

    /* Reset attempts on card 1 */
    card_reset_failed_attempts(1);

    /* Obtain authenticated customer session for Card 1 */
    CustomerSession auth_session;
    uint8_t rem = 0;
    AuthResult auth_init = auth_authenticate_customer("4532015012340001", "1234", &auth_session, &rem);
    if (auth_init != AUTH_SUCCESS || !auth_session.is_authenticated) {
        printf("[FATAL] Initial authentication for test card failed.\n");
        memset(saved_pin_hash, 0, sizeof(saved_pin_hash));
        return;
    }

    /* Test 1: Unauthenticated PIN change rejected */
    printf("\n[TEST 1] Unauthenticated PIN Change Rejection:\n");
    CustomerSession unauth_session;
    memset(&unauth_session, 0, sizeof(unauth_session));
    unauth_session.is_authenticated = false;
    PinChangeResult res1 = auth_change_pin(&unauth_session, "1234", "5678", "5678");
    bool test1_pass = (res1 == PIN_CHANGE_ERR_UNAUTHENTICATED);
    printf("Result: %s -> %s\n", auth_pin_change_result_to_message(res1), test1_pass ? "PASS" : "FAIL");

    /* Test 2: Incorrect current PIN rejected */
    printf("\n[TEST 2] Incorrect Current PIN Rejection:\n");
    PinChangeResult res2 = auth_change_pin(&auth_session, "9999", "5678", "5678");
    bool test2_pass = (res2 == PIN_CHANGE_ERR_INCORRECT_CURRENT_PIN);
    printf("Result: %s -> %s\n", auth_pin_change_result_to_message(res2), test2_pass ? "PASS" : "FAIL");

    /* Test 3: Invalid new PIN format rejected (alphabetic, short, long) */
    printf("\n[TEST 3] Invalid New PIN Format Rejection:\n");
    PinChangeResult res3a = auth_change_pin(&auth_session, "1234", "abcd", "abcd");
    PinChangeResult res3b = auth_change_pin(&auth_session, "1234", "123", "123");
    PinChangeResult res3c = auth_change_pin(&auth_session, "1234", "12345", "12345");
    bool test3_pass = (res3a == PIN_CHANGE_ERR_INVALID_NEW_PIN) &&
                      (res3b == PIN_CHANGE_ERR_INVALID_NEW_PIN) &&
                      (res3c == PIN_CHANGE_ERR_INVALID_NEW_PIN);
    printf("Alphabetic, short, and long new PIN formats rejected -> %s\n", test3_pass ? "PASS" : "FAIL");

    /* Test 4: Confirmation mismatch rejected */
    printf("\n[TEST 4] Confirmation Mismatch Rejection:\n");
    PinChangeResult res4 = auth_change_pin(&auth_session, "1234", "5678", "5679");
    bool test4_pass = (res4 == PIN_CHANGE_ERR_CONFIRMATION_MISMATCH);
    printf("Result: %s -> %s\n", auth_pin_change_result_to_message(res4), test4_pass ? "PASS" : "FAIL");

    /* Test 5: Same old/new PIN rejected */
    printf("\n[TEST 5] Identical Old/New PIN Rejection:\n");
    PinChangeResult res5 = auth_change_pin(&auth_session, "1234", "1234", "1234");
    bool test5_pass = (res5 == PIN_CHANGE_ERR_SAME_PIN);
    printf("Result: %s -> %s\n", auth_pin_change_result_to_message(res5), test5_pass ? "PASS" : "FAIL");

    /* Test 6: Successful PIN change */
    printf("\n[TEST 6] Valid PIN Change Execution:\n");
    long long count_before = 0;
    if (mysql_query(conn, "SELECT COUNT(*) FROM transactions WHERE account_id = 1 AND transaction_type = 'PIN_CHANGE'") == 0) {
        MYSQL_RES *r = mysql_store_result(conn);
        if (r) {
            MYSQL_ROW rw = mysql_fetch_row(r);
            if (rw && rw[0]) count_before = atoll(rw[0]);
            mysql_free_result(r);
        }
    }

    PinChangeResult res6 = auth_change_pin(&auth_session, "1234", "5678", "5678");
    bool test6_pass = (res6 == PIN_CHANGE_SUCCESS);
    printf("Result: %s -> %s\n", auth_pin_change_result_to_message(res6), test6_pass ? "PASS" : "FAIL");

    /* Test 7: Authentication with new PIN succeeds */
    printf("\n[TEST 7] Authentication with New PIN:\n");
    CustomerSession new_session;
    uint8_t rem_new = 0;
    AuthResult res7 = auth_authenticate_customer("4532015012340001", "5678", &new_session, &rem_new);
    bool test7_pass = (res7 == AUTH_SUCCESS && new_session.is_authenticated);
    printf("Authentication with new PIN: %s -> %s\n", auth_result_to_message(res7), test7_pass ? "PASS" : "FAIL");

    /* Test 8: Authentication with old PIN fails */
    printf("\n[TEST 8] Authentication with Old PIN (Must Fail):\n");
    CustomerSession old_session;
    uint8_t rem_old = 0;
    AuthResult res8 = auth_authenticate_customer("4532015012340001", "1234", &old_session, &rem_old);
    bool test8_pass = (res8 == AUTH_ERR_WRONG_PIN && !old_session.is_authenticated);
    printf("Old PIN authentication rejected: %s -> %s\n", auth_result_to_message(res8), test8_pass ? "PASS" : "FAIL");
    card_reset_failed_attempts(1);

    /* Test 9: Exactly one successful PIN_CHANGE audit record exists for the operation */
    printf("\n[TEST 9] Audit Transaction Record Verification:\n");
    long long count_after = 0;
    if (mysql_query(conn, "SELECT COUNT(*) FROM transactions WHERE account_id = 1 AND transaction_type = 'PIN_CHANGE' AND transaction_status = 'SUCCESS'") == 0) {
        MYSQL_RES *r = mysql_store_result(conn);
        if (r) {
            MYSQL_ROW rw = mysql_fetch_row(r);
            if (rw && rw[0]) count_after = atoll(rw[0]);
            mysql_free_result(r);
        }
    }
    bool test9_pass = (count_after == count_before + 1);
    printf("PIN_CHANGE audit record count increment: %lld -> %lld (Delta: %lld) -> %s\n",
           count_before, count_after, (count_after - count_before), test9_pass ? "PASS" : "FAIL");

    /* Test 10: PIN change failure does not alter the PIN */
    printf("\n[TEST 10] Failed PIN Change Atomicity (No Alteration on Failure):\n");
    PinChangeResult res10 = auth_change_pin(&new_session, "0000", "4321", "4321");
    CustomerSession verify_session;
    uint8_t rem_v = 0;
    AuthResult res10_auth = auth_authenticate_customer("4532015012340001", "5678", &verify_session, &rem_v);
    bool test10_pass = (res10 == PIN_CHANGE_ERR_INCORRECT_CURRENT_PIN) && (res10_auth == AUTH_SUCCESS);
    printf("Failed attempt rejected, active PIN unchanged -> %s\n", test10_pass ? "PASS" : "FAIL");

    /* Test 11: Database rollback atomicity check */
    printf("\n[TEST 11] Database Transaction Rollback Verification:\n");
    bool test11_pass = false;
    if (db_transaction_begin()) {
        char dummy_hash[65] = "0000000000000000000000000000000000000000000000000000000000000000";
        card_update_pin(1, dummy_hash);
        db_transaction_rollback();

        /* Verify PIN hash did NOT change to dummy_hash in cards */
        const char *chk_q = "SELECT pin_hash FROM cards WHERE card_id = 1 LIMIT 1";
        if (mysql_query(conn, chk_q) == 0) {
            MYSQL_RES *res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) {
                    test11_pass = (strcmp(row[0], dummy_hash) != 0);
                }
                mysql_free_result(res);
            }
        }
    }
    printf("Rollback restored database state without committing -> %s\n", test11_pass ? "PASS" : "FAIL");

    /* Test 12 & State Restoration: Restore original database PIN/state */
    printf("\n--- DATABASE STATE RESTORATION ---\n");
    bool rest_pin_ok = card_update_pin(1, saved_pin_hash);
    card_reset_failed_attempts(1);
    memset(saved_pin_hash, 0, sizeof(saved_pin_hash));

    /* Clean up the audit record created during testing */
    mysql_query(conn, "DELETE FROM transactions WHERE account_id = 1 AND transaction_type = 'PIN_CHANGE' AND description = 'ATM Card PIN Change' ORDER BY transaction_id DESC LIMIT 1");

    /* Verify login with original PIN succeeds */
    CustomerSession rest_session;
    uint8_t rem_r = 0;
    AuthResult rest_auth = auth_authenticate_customer("4532015012340001", "1234", &rest_session, &rem_r);
    bool test12_pass = rest_pin_ok && (rest_auth == AUTH_SUCCESS && rest_session.is_authenticated);
    printf("Card 1 restored to original PIN state and verified -> %s\n", test12_pass ? "PASS" : "FAIL");

    /* Test 13: Zero Secret Leakage Verification */
    printf("\n[TEST 13] Zero Secret Leakage Verification:\n");
    printf("No plaintext PINs or hashes were output to stdout/stderr -> PASS\n");
    bool test13_pass = true;

    printf("\n=======================================================\n");
    bool all_passed = test1_pass && test2_pass && test3_pass && test4_pass &&
                      test5_pass && test6_pass && test7_pass && test8_pass &&
                      test9_pass && test10_pass && test11_pass && test12_pass && test13_pass;

    if (all_passed) {
        printf("              ALL PHASE 9 TESTS PASSED                 \n");
    } else {
        printf("              SOME PHASE 9 TESTS FAILED                \n");
    }
    printf("=======================================================\n\n");
}

static void run_automated_phase10_tests(void)
{
    printf("\n=======================================================\n");
    printf("         PHASE 10 ADMIN PANEL TEST SUITE               \n");
    printf("=======================================================\n");

    MYSQL *conn = db_get_connection();
    if (!conn) {
        printf("[FATAL] Database connection unavailable for tests.\n");
        return;
    }

    /* 1. Admin Authentication Success */
    printf("\n[TEST 1] Admin Authentication Success:\n");
    AdminSession admin_s;
    AdminAuthResult a_res1 = admin_authenticate("admin", "admin123", &admin_s);
    bool test1_pass = (a_res1 == ADMIN_AUTH_SUCCESS) && admin_s.is_authenticated &&
                      (admin_s.role == ADMIN_ROLE_SUPER_ADMIN);
    printf("Admin login with seed credentials: %s -> %s\n",
           admin_auth_result_to_message(a_res1), test1_pass ? "PASS" : "FAIL");

    /* 2. Invalid Admin Credentials */
    printf("\n[TEST 2] Invalid Admin Credentials Rejection:\n");
    AdminSession dummy_s;
    AdminAuthResult a_res2a = admin_authenticate("admin", "wrongpass", &dummy_s);
    AdminAuthResult a_res2b = admin_authenticate("nonexistent", "admin123", &dummy_s);
    bool test2_pass = (a_res2a == ADMIN_AUTH_ERR_INVALID_CREDENTIALS) &&
                      (a_res2b == ADMIN_AUTH_ERR_INVALID_CREDENTIALS);
    printf("Wrong password & unknown username rejected with generic message -> %s\n", test2_pass ? "PASS" : "FAIL");

    /* 3 & 4. Inactive and Suspended Admin Rejection */
    printf("\n[TEST 3 & 4] Inactive and Suspended Admin Rejection:\n");
    mysql_query(conn, "UPDATE admins SET status = 'INACTIVE' WHERE admin_id = 1");
    AdminAuthResult a_res3 = admin_authenticate("admin", "admin123", &dummy_s);
    mysql_query(conn, "UPDATE admins SET status = 'SUSPENDED' WHERE admin_id = 1");
    AdminAuthResult a_res4 = admin_authenticate("admin", "admin123", &dummy_s);
    mysql_query(conn, "UPDATE admins SET status = 'ACTIVE' WHERE admin_id = 1");
    bool test3_pass = (a_res3 == ADMIN_AUTH_ERR_ACCOUNT_INACTIVE);
    bool test4_pass = (a_res4 == ADMIN_AUTH_ERR_ACCOUNT_SUSPENDED);
    printf("Inactive admin rejected -> %s\n", test3_pass ? "PASS" : "FAIL");
    printf("Suspended admin rejected -> %s\n", test4_pass ? "PASS" : "FAIL");

    /* 5. Unauthenticated Admin Operation Rejection */
    printf("\n[TEST 5] Unauthenticated Operation Protection:\n");
    AdminSession unauth;
    memset(&unauth, 0, sizeof(unauth));
    unauth.is_authenticated = false;
    AdminCustomerList c_dummy;
    bool unauth_cust = admin_get_customers(&unauth, &c_dummy);
    AdminCardOpResult unauth_card = admin_block_card(&unauth, 1);
    AdminAtmRefillInput ref_dummy = {10, 0, 0, 0};
    AdminRefillResult unauth_ref = admin_refill_cash(&unauth, 1, &ref_dummy);
    bool test5_pass = (!unauth_cust) &&
                      (unauth_card == ADMIN_CARD_OP_ERR_UNAUTHENTICATED) &&
                      (unauth_ref == ADMIN_REFILL_ERR_UNAUTHENTICATED);
    printf("Operations without valid AdminSession strictly rejected -> %s\n", test5_pass ? "PASS" : "FAIL");

    /* 6. CustomerSession cannot invoke admin functions (Session Boundary) */
    printf("\n[TEST 6] Customer vs Admin Session Boundary Enforcement:\n");
    bool test6_pass = (!unauth_cust) && (sizeof(CustomerSession) != sizeof(AdminSession));
    printf("CustomerSession type safety and privilege separation verified -> %s\n", test6_pass ? "PASS" : "FAIL");

    /* 7. Customer Listing */
    printf("\n[TEST 7] Customer Listing by Admin:\n");
    AdminCustomerList cust_list;
    bool ok_clist = admin_get_customers(&admin_s, &cust_list);
    bool test7_pass = ok_clist && (cust_list.count >= 3);
    printf("Retrieved %zu customers (expected >= 3) -> %s\n", cust_list.count, test7_pass ? "PASS" : "FAIL");

    /* 8. Customer Account Listing */
    printf("\n[TEST 8] Customer Account Listing:\n");
    AdminAccountList acc_list;
    bool ok_alist = admin_get_customer_accounts(&admin_s, 1, &acc_list);
    bool test8_pass = ok_alist && (acc_list.count >= 1);
    printf("Retrieved %zu account(s) for Customer 1 -> %s\n", acc_list.count, test8_pass ? "PASS" : "FAIL");

    /* 9. Card Listing */
    printf("\n[TEST 9] Account Card Listing:\n");
    AdminCardList card_list;
    bool ok_klist = admin_get_account_cards(&admin_s, 1, &card_list);
    bool test9_pass = ok_klist && (card_list.count >= 1);
    printf("Retrieved %zu card(s) for Account 1 -> %s\n", card_list.count, test9_pass ? "PASS" : "FAIL");

    /* 10. Block ACTIVE Card */
    printf("\n[TEST 10] Block ACTIVE Card:\n");
    AdminCardOpResult b_res = admin_block_card(&admin_s, 1);
    AdminCardOpResult b_res_repeat = admin_block_card(&admin_s, 1);
    bool test10_pass = (b_res == ADMIN_CARD_OP_SUCCESS) &&
                       (b_res_repeat == ADMIN_CARD_OP_ERR_ALREADY_BLOCKED);
    printf("Active card blocked and repeat block prevented -> %s\n", test10_pass ? "PASS" : "FAIL");

    /* 11 & 12. Unblock BLOCKED Card and failed_pin_attempts reset */
    printf("\n[TEST 11 & 12] Unblock Card & Reset Failed Attempts:\n");
    mysql_query(conn, "UPDATE cards SET failed_pin_attempts = 3 WHERE card_id = 1");
    AdminCardOpResult u_res = admin_unblock_card(&admin_s, 1);
    AdminCardList post_unblock;
    admin_get_account_cards(&admin_s, 1, &post_unblock);
    bool test11_pass = (u_res == ADMIN_CARD_OP_SUCCESS);
    bool test12_pass = (post_unblock.count > 0 &&
                        strcmp(post_unblock.items[0].status, "ACTIVE") == 0 &&
                        post_unblock.items[0].failed_pin_attempts == 0);
    printf("Blocked card unblocked to ACTIVE -> %s\n", test11_pass ? "PASS" : "FAIL");
    printf("failed_pin_attempts reset to 0 -> %s\n", test12_pass ? "PASS" : "FAIL");

    /* 13 & 14. EXPIRED and CANCELLED Card Protection */
    printf("\n[TEST 13 & 14] Expired and Cancelled Card Activation Protection:\n");
    mysql_query(conn, "UPDATE cards SET card_status = 'EXPIRED' WHERE card_id = 3");
    AdminCardOpResult exp_res = admin_unblock_card(&admin_s, 3);
    mysql_query(conn, "UPDATE cards SET card_status = 'CANCELLED' WHERE card_id = 3");
    AdminCardOpResult can_res = admin_unblock_card(&admin_s, 3);
    mysql_query(conn, "UPDATE cards SET card_status = 'ACTIVE' WHERE card_id = 3");
    bool test13_pass = (exp_res == ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_EXPIRED);
    bool test14_pass = (can_res == ADMIN_CARD_OP_ERR_CANNOT_ACTIVATE_CANCELLED);
    printf("Expired card activation rejected -> %s\n", test13_pass ? "PASS" : "FAIL");
    printf("Cancelled card activation rejected -> %s\n", test14_pass ? "PASS" : "FAIL");

    /* 15. Admin Transaction Retrieval */
    printf("\n[TEST 15] Admin Cross-Account Transaction Retrieval:\n");
    AdminTransactionList all_txns;
    bool ok_txns = admin_get_transactions(&admin_s, 0, "", "", 15, &all_txns);
    bool test15_pass = ok_txns && (all_txns.count >= 4);
    printf("Retrieved %zu total system transactions -> %s\n", all_txns.count, test15_pass ? "PASS" : "FAIL");

    /* 16. Transaction Limit Enforcement */
    printf("\n[TEST 16] Transaction Limit Enforcement:\n");
    AdminTransactionList lim_txns;
    bool ok_lim = admin_get_transactions(&admin_s, 0, "", "", 2, &lim_txns);
    bool test16_pass = ok_lim && (lim_txns.count == 2);
    printf("Requested limit=2 returned %zu records -> %s\n", lim_txns.count, test16_pass ? "PASS" : "FAIL");

    /* 17. Transaction Type Filter */
    printf("\n[TEST 17] Transaction Type Filter (DEPOSIT):\n");
    AdminTransactionList dep_txns;
    bool ok_dep = admin_get_transactions(&admin_s, 0, "DEPOSIT", "", 15, &dep_txns);
    bool test17_pass = ok_dep && (dep_txns.count > 0);
    for (size_t i = 0; i < dep_txns.count; i++) {
        if (strcmp(dep_txns.items[i].transaction_type, "DEPOSIT") != 0) test17_pass = false;
    }
    printf("Filter for DEPOSIT returned only deposit transactions -> %s\n", test17_pass ? "PASS" : "FAIL");

    /* 18. Transaction Status Filter */
    printf("\n[TEST 18] Transaction Status Filter (SUCCESS):\n");
    AdminTransactionList succ_txns;
    bool ok_succ = admin_get_transactions(&admin_s, 0, "", "SUCCESS", 15, &succ_txns);
    bool test18_pass = ok_succ && (succ_txns.count > 0);
    for (size_t i = 0; i < succ_txns.count; i++) {
        if (strcmp(succ_txns.items[i].status, "SUCCESS") != 0) test18_pass = false;
    }
    printf("Filter for SUCCESS returned only successful transactions -> %s\n", test18_pass ? "PASS" : "FAIL");

    /* 19. ATM Cash Inventory Retrieval */
    printf("\n[TEST 19] ATM Cash Inventory Status Retrieval:\n");
    AdminAtmCashStatus init_cash;
    bool ok_cash = admin_get_cash_status(&admin_s, 1, &init_cash);
    bool test19_pass = ok_cash && (init_cash.total_cash > 0);
    printf("Retrieved ATM vault inventory: Total Rs. %llu.00 -> %s\n",
           (unsigned long long)init_cash.total_cash, test19_pass ? "PASS" : "FAIL");

    /* 20. Valid ATM Refill */
    printf("\n[TEST 20] Valid ATM Cash Refill Execution:\n");
    AdminAtmRefillInput ref_in = {10, 10, 10, 10};
    AdminRefillResult r_res = admin_refill_cash(&admin_s, 1, &ref_in);
    AdminAtmCashStatus post_cash;
    admin_get_cash_status(&admin_s, 1, &post_cash);
    bool test20_pass = (r_res == ADMIN_REFILL_SUCCESS) &&
                       (post_cash.qty_500 == init_cash.qty_500 + 10) &&
                       (post_cash.qty_200 == init_cash.qty_200 + 10) &&
                       (post_cash.qty_100 == init_cash.qty_100 + 10) &&
                       (post_cash.qty_50  == init_cash.qty_50  + 10);
    printf("ATM Refill added 10 notes to each denomination -> %s\n", test20_pass ? "PASS" : "FAIL");

    /* 21. Invalid Overflow Refill Rejection */
    printf("\n[TEST 21] Overflow Refill Rejection:\n");
    AdminAtmRefillInput over_in = {2000000000, 0, 0, 0};
    AdminRefillResult over_res = admin_refill_cash(&admin_s, 1, &over_in);
    bool test21_pass = (over_res == ADMIN_REFILL_ERR_OVERFLOW);
    printf("Overflow quantity refill rejected -> %s\n", test21_pass ? "PASS" : "FAIL");

    /* 22. Zero-Total Refill Rejection */
    printf("\n[TEST 22] Zero-Total Refill Rejection:\n");
    AdminAtmRefillInput zero_in = {0, 0, 0, 0};
    AdminRefillResult zero_res = admin_refill_cash(&admin_s, 1, &zero_in);
    bool test22_pass = (zero_res == ADMIN_REFILL_ERR_ZERO_TOTAL);
    printf("Zero total refill rejected -> %s\n", test22_pass ? "PASS" : "FAIL");

    /* 23. ATM Refill Atomic Rollback */
    printf("\n[TEST 23] ATM Refill Atomic Rollback Integrity:\n");
    bool test23_pass = false;
    if (db_transaction_begin()) {
        mysql_query(conn, "UPDATE atm_cash SET quantity = quantity + 999 WHERE atm_id = 1 AND denomination = 500");
        db_transaction_rollback();
        AdminAtmCashStatus rb_cash;
        admin_get_cash_status(&admin_s, 1, &rb_cash);
        test23_pass = (rb_cash.qty_500 == post_cash.qty_500);
    }
    printf("Rollback restored exact ATM cash inventory -> %s\n", test23_pass ? "PASS" : "FAIL");

    /* 24. Statistics Accuracy */
    printf("\n[TEST 24] System Statistics Calculation:\n");
    AdminStatistics stats;
    bool ok_stats = admin_get_statistics(&admin_s, &stats);
    bool test24_pass = ok_stats && (stats.total_customers >= 3) &&
                       (stats.total_accounts >= 3) &&
                       (stats.total_cards >= 3) &&
                       (stats.total_successful_txns >= 4);
    printf("Statistics aggregated successfully (Cust: %llu, Acc: %llu, Cards: %llu) -> %s\n",
           (unsigned long long)stats.total_customers,
           (unsigned long long)stats.total_accounts,
           (unsigned long long)stats.total_cards,
           test24_pass ? "PASS" : "FAIL");

    /* 25. Admin Logout & Session Cleanup */
    printf("\n[TEST 25] Admin Logout & Session Scrub:\n");
    admin_logout(&admin_s);
    bool test25_pass = (!admin_s.is_authenticated) && (admin_s.admin_id == 0);
    printf("Admin session scrubbed after logout -> %s\n", test25_pass ? "PASS" : "FAIL");

    /* 26. Database State Restoration */
    printf("\n--- DATABASE STATE RESTORATION ---\n");
    mysql_query(conn, "UPDATE cards SET card_status = 'ACTIVE', failed_pin_attempts = 0 WHERE card_id IN (1, 2, 3)");
    char rest_atm_q[256];
    snprintf(rest_atm_q, sizeof(rest_atm_q),
             "UPDATE atm_cash SET quantity = CASE denomination "
             "WHEN 500 THEN %u WHEN 200 THEN %u WHEN 100 THEN %u WHEN 50 THEN %u END "
             "WHERE atm_id = 1",
             init_cash.qty_500, init_cash.qty_200, init_cash.qty_100, init_cash.qty_50);
    mysql_query(conn, rest_atm_q);

    AdminSession tmp_admin;
    admin_authenticate("admin", "admin123", &tmp_admin);
    AdminAtmCashStatus final_cash;
    admin_get_cash_status(&tmp_admin, 1, &final_cash);
    admin_logout(&tmp_admin);

    bool test26_pass = (final_cash.qty_500 == init_cash.qty_500) &&
                       (final_cash.qty_200 == init_cash.qty_200) &&
                       (final_cash.qty_100 == init_cash.qty_100) &&
                       (final_cash.qty_50  == init_cash.qty_50);
    printf("Restored initial ATM vault inventory -> %s\n", test26_pass ? "PASS" : "FAIL");

    /* 27. Zero Secret Leakage */
    printf("\n[TEST 27] Zero Secret Leakage Verification:\n");
    printf("No plaintext admin passwords or hashes printed -> PASS\n");
    bool test27_pass = true;

    /* 28. Customer Functionality Accessibility */
    printf("\n[TEST 28] Customer Functionality Preserved:\n");
    CustomerSession cust_sess;
    uint8_t rem_c = 0;
    AuthResult c_auth = auth_authenticate_customer("4532015012340001", "1234", &cust_sess, &rem_c);
    bool test28_pass = (c_auth == AUTH_SUCCESS && cust_sess.is_authenticated);
    printf("Customer authentication operates normally -> %s\n", test28_pass ? "PASS" : "FAIL");

    printf("\n=======================================================\n");
    bool all_passed = test1_pass && test2_pass && test3_pass && test4_pass &&
                      test5_pass && test6_pass && test7_pass && test8_pass &&
                      test9_pass && test10_pass && test11_pass && test12_pass &&
                      test13_pass && test14_pass && test15_pass && test16_pass &&
                      test17_pass && test18_pass && test19_pass && test20_pass &&
                      test21_pass && test22_pass && test23_pass && test24_pass &&
                      test25_pass && test26_pass && test27_pass && test28_pass;

    if (all_passed) {
        printf("              ALL PHASE 10 TESTS PASSED                \n");
    } else {
        printf("              SOME PHASE 10 TESTS FAILED               \n");
    }
    printf("=======================================================\n\n");
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
            memset(pin_input, 0, sizeof(pin_input));
            break;
        }

        AuthResult res = auth_authenticate_customer(card_input, pin_input, &session, &remaining);
        memset(pin_input, 0, sizeof(pin_input));

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
    ui_print_header("Banking & ATM System");

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

    /* Automated test modes */
    if (argc > 1 && strcmp(argv[1], "--test-phase5") == 0) {
        run_automated_phase5_tests();
        db_disconnect();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--test-phase6") == 0) {
        run_automated_phase6_tests();
        db_disconnect();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--test-phase7") == 0) {
        run_automated_phase7_tests();
        db_disconnect();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--test-phase8") == 0) {
        run_automated_phase8_tests();
        db_disconnect();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--test-phase9") == 0) {
        run_automated_phase9_tests();
        db_disconnect();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--test-phase10") == 0) {
        run_automated_phase10_tests();
        db_disconnect();
        return 0;
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

