#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "auth.h"

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
 * Pause and prompt user to press enter to continue.
 */
void ui_pause(void);

#endif /* UI_H */
