#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "auth.h"
#include "admin.h"
#include "deposit.h"
#include "withdrawal.h"
#include "transfer.h"
#include "beneficiary.h"
#include "transaction.h"

/**
 * Display header with system title.
 */
void ui_print_header(const char *subtitle);

/**
 * Display main customer banking menu (1-7).
 */
void ui_print_customer_menu(const CustomerSession *session);

/**
 * Display beneficiary management submenu (1-4).
 */
void ui_print_beneficiary_menu(void);

/**
 * Display balance inquiry screen.
 *
 * @param session Active authenticated customer session.
 */
void ui_display_balance_inquiry(const CustomerSession *session);

/**
 * Handle interactive deposit flow.
 *
 * @param session Active authenticated customer session.
 */
void ui_handle_deposit(const CustomerSession *session);

/**
 * Handle interactive withdrawal flow.
 *
 * @param session Active authenticated customer session.
 */
void ui_handle_withdrawal(const CustomerSession *session);

/**
 * Handle interactive fund transfer flow.
 *
 * @param session Active authenticated customer session.
 */
void ui_handle_transfer(const CustomerSession *session);

/**
 * Handle interactive beneficiary management flow.
 *
 * @param session Active authenticated customer session.
 */
void ui_handle_manage_beneficiaries(const CustomerSession *session);

/**
 * Handle interactive mini statement flow.
 *
 * @param session Active authenticated customer session.
 */
void ui_handle_mini_statement(const CustomerSession *session);

/**
 * Handle interactive PIN change flow.
 *
 * @param session Active authenticated customer session.
 */
void ui_handle_pin_change(CustomerSession *session);

/**
 * Display deposit receipt / confirmation screen.
 */
void ui_display_deposit_receipt(const DepositReceipt *receipt, const char *customer_name);

/**
 * Display withdrawal receipt / confirmation screen with note breakdown.
 */
void ui_display_withdrawal_receipt(const WithdrawalReceipt *receipt, const char *customer_name);

/**
 * Display transfer receipt / confirmation screen.
 */
void ui_display_transfer_receipt(const TransferReceipt *receipt, const char *customer_name);

/**
 * Display formatted beneficiary table.
 */
void ui_display_beneficiary_list(const BeneficiaryList *list);

/**
 * Display formatted mini statement table.
 */
void ui_display_mini_statement(const CustomerSession *session, const StatementList *list);

/**
 * Pause and prompt user to press enter to continue.
 */
void ui_pause(void);

/* Admin UI */
void ui_print_top_menu(void);
void ui_print_admin_menu(const AdminSession *session);
bool ui_handle_admin_login(AdminSession *session);
void ui_handle_admin_dashboard(AdminSession *session);
void ui_display_admin_customers(const AdminSession *session);
void ui_display_admin_accounts(const AdminSession *session);
void ui_display_admin_cards(const AdminSession *session);
void ui_handle_admin_block_card(const AdminSession *session);
void ui_handle_admin_unblock_card(const AdminSession *session);
void ui_display_admin_transactions(const AdminSession *session);
void ui_display_admin_cash_status(const AdminSession *session);
void ui_handle_admin_refill_cash(const AdminSession *session);
void ui_display_admin_statistics(const AdminSession *session);

#endif /* UI_H */
