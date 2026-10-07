#ifndef BENEFICIARY_H
#define BENEFICIARY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define BENEFICIARY_MAX_LIST 32

typedef enum {
    BENEFICIARY_SUCCESS = 0,
    BENEFICIARY_ERR_INVALID_ACCOUNT,       /* Account number invalid format or empty */
    BENEFICIARY_ERR_ACCOUNT_NOT_FOUND,     /* Target account does not exist */
    BENEFICIARY_ERR_ACCOUNT_INACTIVE,      /* Target account is inactive or blocked */
    BENEFICIARY_ERR_SELF_ADD,              /* Source account cannot add itself */
    BENEFICIARY_ERR_DUPLICATE,             /* Already registered as active beneficiary */
    BENEFICIARY_ERR_NOT_FOUND,             /* Beneficiary record not found */
    BENEFICIARY_ERR_UNAUTHORIZED,          /* Relationship does not belong to user */
    BENEFICIARY_ERR_DATABASE,              /* SQL execution error */
    BENEFICIARY_ERR_SYSTEM
} BeneficiaryResult;

typedef struct {
    uint64_t beneficiary_id;
    uint64_t account_id;
    uint64_t beneficiary_account_id;
    char beneficiary_name[100];
    char nickname[50];
    char beneficiary_account_number[35];
    char status[16];
} BeneficiaryRecord;

typedef struct {
    BeneficiaryRecord items[BENEFICIARY_MAX_LIST];
    size_t count;
} BeneficiaryList;

/**
 * Add a new beneficiary for an account.
 *
 * @param source_account_id Authenticated customer's account ID.
 * @param target_account_number Destination account number string.
 * @param nickname Optional nickname (can be NULL or empty).
 * @param out_beneficiary_id Output receiving newly inserted beneficiary ID (can be NULL).
 * @return BeneficiaryResult code.
 */
BeneficiaryResult beneficiary_add(uint64_t source_account_id,
                                  const char *target_account_number,
                                  const char *nickname,
                                  uint64_t *out_beneficiary_id);

/**
 * List all active beneficiaries for an account.
 *
 * @param source_account_id Authenticated customer's account ID.
 * @param list Output structure holding array of BeneficiaryRecord.
 * @return true on success, false on database error.
 */
bool beneficiary_get_list(uint64_t source_account_id, BeneficiaryList *list);

/**
 * Remove (delete) a beneficiary belonging to the source account.
 *
 * @param source_account_id Authenticated customer's account ID.
 * @param beneficiary_id ID of beneficiary row to remove.
 * @return BeneficiaryResult code.
 */
BeneficiaryResult beneficiary_remove(uint64_t source_account_id, uint64_t beneficiary_id);

/**
 * Get single active beneficiary record by ID, verifying ownership by source_account_id.
 *
 * @param source_account_id Authenticated customer's account ID.
 * @param beneficiary_id ID of beneficiary to look up.
 * @param record Output struct receiving beneficiary details.
 * @return BeneficiaryResult code.
 */
BeneficiaryResult beneficiary_get_by_id(uint64_t source_account_id,
                                        uint64_t beneficiary_id,
                                        BeneficiaryRecord *record);

/**
 * Convert BeneficiaryResult enum to descriptive error message.
 */
const char *beneficiary_result_to_string(BeneficiaryResult result);

#endif /* BENEFICIARY_H */
