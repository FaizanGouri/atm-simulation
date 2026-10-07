#ifndef ATM_H
#define ATM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t count_500;
    uint32_t count_200;
    uint32_t count_100;
    uint32_t count_50;
} DenominationBreakdown;

/**
 * Load available note counts for ATM 1 inside an active transaction.
 *
 * @param atm_id ATM ID (defaults to 1).
 * @param available Output structure receiving available note counts.
 * @return true on success, false on SQL error.
 */
bool atm_get_cash_inventory(uint64_t atm_id, DenominationBreakdown *available);

/**
 * Deterministic note selection algorithm.
 * Calculates note breakdown for requested amount in rupees (multiple of 50).
 *
 * @param amount_rupees Withdrawal amount in whole rupees.
 * @param available Available note counts in ATM.
 * @param dispensed Output structure populated with required note quantities.
 * @return true if an exact dispense combination exists, false otherwise.
 */
bool atm_calculate_denominations(uint64_t amount_rupees,
                                 const DenominationBreakdown *available,
                                 DenominationBreakdown *dispensed);

/**
 * Atomically deduct dispensed notes from atm_cash table.
 *
 * @param atm_id ATM ID.
 * @param dispensed Notes to deduct.
 * @return true on success, false on SQL error.
 */
bool atm_deduct_cash_inventory(uint64_t atm_id, const DenominationBreakdown *dispensed);

#endif /* ATM_H */
