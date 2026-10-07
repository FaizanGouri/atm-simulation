#ifndef VALIDATION_H
#define VALIDATION_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Validate card number format (digits only, length between 12 and 19 characters).
 */
bool validation_is_valid_card_number(const char *card_number);

/**
 * Validate PIN format (digits only, length typically 4 to 6 characters).
 */
bool validation_is_valid_pin(const char *pin);

/**
 * Sanitize input string by trimming leading/trailing whitespace.
 */
void validation_trim(char *str);

#endif /* VALIDATION_H */
