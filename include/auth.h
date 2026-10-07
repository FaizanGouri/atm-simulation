#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stdint.h>
#include "card.h"

typedef struct {
    uint64_t customer_id;
    char customer_number[33];
    char customer_name[101];
    uint64_t account_id;
    char account_number[35];
    char account_type[16];
    uint64_t card_id;
    char card_number[20];
    bool is_authenticated;
} CustomerSession;

typedef enum {
    AUTH_SUCCESS = 0,
    AUTH_ERR_INVALID_CARD,
    AUTH_ERR_CARD_BLOCKED,
    AUTH_ERR_CARD_EXPIRED,
    AUTH_ERR_CARD_CANCELLED,
    AUTH_ERR_WRONG_PIN,
    AUTH_ERR_CARD_NOW_BLOCKED,
    AUTH_ERR_DB_FAILURE
} AuthResult;

/**
 * Perform complete authentication for a card with an entered PIN.
 *
 * @param card_number Input card number.
 * @param pin Input PIN.
 * @param session Output session structure populated on success.
 * @param attempts_remaining Output parameter indicating how many attempts remain (if PIN was wrong).
 * @return AuthResult status code.
 */
AuthResult auth_authenticate_customer(const char *card_number,
                                      const char *pin,
                                      CustomerSession *session,
                                      uint8_t *attempts_remaining);

/**
 * Terminate an authenticated session safely.
 */
void auth_logout(CustomerSession *session);

/**
 * Return human-readable message for an AuthResult code.
 */
const char *auth_result_to_message(AuthResult result);

typedef enum {
    PIN_CHANGE_SUCCESS = 0,
    PIN_CHANGE_ERR_UNAUTHENTICATED,
    PIN_CHANGE_ERR_INCORRECT_CURRENT_PIN,
    PIN_CHANGE_ERR_INVALID_NEW_PIN,
    PIN_CHANGE_ERR_CONFIRMATION_MISMATCH,
    PIN_CHANGE_ERR_SAME_PIN,
    PIN_CHANGE_ERR_DB_FAILURE
} PinChangeResult;

/**
 * Perform PIN change for an authenticated customer session.
 *
 * @param session Authenticated customer session.
 * @param current_pin Current PIN entered by customer.
 * @param new_pin New PIN entered by customer.
 * @param confirm_pin Confirmation of new PIN entered by customer.
 * @return PinChangeResult status code.
 */
PinChangeResult auth_change_pin(
    CustomerSession *session,
    const char *current_pin,
    const char *new_pin,
    const char *confirm_pin
);

/**
 * Return human-readable message for a PinChangeResult code.
 */
const char *auth_pin_change_result_to_message(PinChangeResult result);

#endif /* AUTH_H */
