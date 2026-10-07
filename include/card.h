#ifndef CARD_H
#define CARD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CARD_STATUS_ACTIVE,
    CARD_STATUS_BLOCKED,
    CARD_STATUS_EXPIRED,
    CARD_STATUS_CANCELLED,
    CARD_STATUS_UNKNOWN
} CardStatus;

typedef struct {
    uint64_t card_id;
    uint64_t account_id;
    char card_number[20];
    char pin_hash[65];
    uint8_t failed_pin_attempts;
    CardStatus status;
    char expiry_date[12];
} CardRecord;

/**
 * Convert string status from DB to CardStatus enum.
 */
CardStatus card_status_from_string(const char *status_str);

/**
 * Convert CardStatus enum to human-readable string.
 */
const char *card_status_to_string(CardStatus status);

/**
 * Find a card by card_number using a prepared statement.
 *
 * @param card_number The formatted card number string.
 * @param card Output structure to receive card details.
 * @return true if card found, false if not found or on SQL error.
 */
bool card_find_by_number(const char *card_number, CardRecord *card);

/**
 * Increment failed PIN attempt counter in database.
 * If new count reaches 3, status is atomically updated to 'BLOCKED'.
 *
 * @param card_id Card ID.
 * @param updated_attempts Output parameter for new attempt count.
 * @param is_now_blocked Output parameter set to true if card was blocked.
 * @return true on success, false on SQL error.
 */
bool card_increment_failed_attempts(uint64_t card_id, uint8_t *updated_attempts, bool *is_now_blocked);

/**
 * Reset failed PIN attempt counter to 0 upon successful login.
 * Also updates last_used_at timestamp.
 *
 * @param card_id Card ID.
 * @return true on success, false on SQL error.
 */
bool card_reset_failed_attempts(uint64_t card_id);

/**
 * Update card status in database (e.g., to BLOCKED).
 *
 * @param card_id Card ID.
 * @param new_status Target CardStatus.
 * @return true on success, false on SQL error.
 */
bool card_update_status(uint64_t card_id, CardStatus new_status);

/**
 * Update card PIN hash in database and reset failed attempts counter to 0.
 *
 * @param card_id Card ID.
 * @param new_pin_hash SHA-256 hexadecimal digest of the new PIN.
 * @return true on success, false on SQL error.
 */
bool card_update_pin(uint64_t card_id, const char *new_pin_hash);

#endif /* CARD_H */
