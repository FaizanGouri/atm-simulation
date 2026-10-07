#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "auth.h"
#include "deposit.h"
#include "withdrawal.h"

/**
 * Display header with system title.
 */
void ui_print_header(const char *subtitle);

/**
 * Display main customer banking menu.
 */
void ui_print_customer_menu(const CustomerSession *session);

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
 * Display deposit receipt / confirmation screen.
 */
void ui_display_deposit_receipt(const DepositReceipt *receipt, const char *customer_name);

/**
 * Display withdrawal receipt / confirmation screen with note breakdown.
 */
void ui_display_withdrawal_receipt(const WithdrawalReceipt *receipt, const char *customer_name);

/**
 * Pause and prompt user to press enter to continue.
 */
void ui_pause(void);

#endif /* UI_H */
