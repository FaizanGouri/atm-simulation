#include "atm.h"
#include "database.h"
#include <stdio.h>
#include <string.h>

bool atm_get_cash_inventory(uint64_t atm_id, DenominationBreakdown *available)
{
    if (!available) return false;
    memset(available, 0, sizeof(DenominationBreakdown));

    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    const char *query = "SELECT denomination, quantity FROM atm_cash WHERE atm_id = ? FOR UPDATE";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b_in[1];
    memset(b_in, 0, sizeof(b_in));
    unsigned long long aid = atm_id;
    b_in[0].buffer_type = MYSQL_TYPE_LONGLONG;
    b_in[0].buffer = &aid;

    if (mysql_stmt_bind_param(stmt, b_in) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned int denom = 0;
    unsigned int qty = 0;
    MYSQL_BIND b_out[2];
    memset(b_out, 0, sizeof(b_out));
    b_out[0].buffer_type = MYSQL_TYPE_LONG;
    b_out[0].buffer = &denom;
    b_out[1].buffer_type = MYSQL_TYPE_LONG;
    b_out[1].buffer = &qty;

    if (mysql_stmt_bind_result(stmt, b_out) != 0 || mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        if (denom == 500) available->count_500 = qty;
        else if (denom == 200) available->count_200 = qty;
        else if (denom == 100) available->count_100 = qty;
        else if (denom == 50)  available->count_50  = qty;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}

bool atm_calculate_denominations(uint64_t amount_rupees,
                                 const DenominationBreakdown *available,
                                 DenominationBreakdown *dispensed)
{
    if (!available || !dispensed) return false;
    memset(dispensed, 0, sizeof(DenominationBreakdown));

    if (amount_rupees == 0 || (amount_rupees % 50) != 0) {
        return false;
    }

    /* Deterministic backtracking search:
       Tries largest notes first, but backtracks if smaller notes are required
       to form the exact amount. */
    uint32_t max_500 = (uint32_t)(amount_rupees / 500);
    if (max_500 > available->count_500) max_500 = available->count_500;

    for (int n500 = (int)max_500; n500 >= 0; n500--) {
        uint64_t rem_after_500 = amount_rupees - ((uint64_t)n500 * 500);
        uint32_t max_200 = (uint32_t)(rem_after_500 / 200);
        if (max_200 > available->count_200) max_200 = available->count_200;

        for (int n200 = (int)max_200; n200 >= 0; n200--) {
            uint64_t rem_after_200 = rem_after_500 - ((uint64_t)n200 * 200);
            uint32_t max_100 = (uint32_t)(rem_after_200 / 100);
            if (max_100 > available->count_100) max_100 = available->count_100;

            for (int n100 = (int)max_100; n100 >= 0; n100--) {
                uint64_t rem_after_100 = rem_after_200 - ((uint64_t)n100 * 100);
                uint32_t needed_50 = (uint32_t)(rem_after_100 / 50);

                if (needed_50 <= available->count_50 && (rem_after_100 % 50 == 0)) {
                    dispensed->count_500 = (uint32_t)n500;
                    dispensed->count_200 = (uint32_t)n200;
                    dispensed->count_100 = (uint32_t)n100;
                    dispensed->count_50  = needed_50;
                    return true;
                }
            }
        }
    }

    return false;
}

static bool update_single_denom(MYSQL *conn, uint64_t atm_id, uint32_t denom, uint32_t deduct_qty)
{
    if (deduct_qty == 0) return true;

    const char *query = "UPDATE atm_cash SET quantity = quantity - ? WHERE atm_id = ? AND denomination = ?";
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, (unsigned long)strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND b[3];
    memset(b, 0, sizeof(b));
    unsigned int q = deduct_qty;
    unsigned long long aid = atm_id;
    unsigned int d = denom;

    b[0].buffer_type = MYSQL_TYPE_LONG;
    b[0].buffer = &q;
    b[1].buffer_type = MYSQL_TYPE_LONGLONG;
    b[1].buffer = &aid;
    b[2].buffer_type = MYSQL_TYPE_LONG;
    b[2].buffer = &d;

    if (mysql_stmt_bind_param(stmt, b) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool atm_deduct_cash_inventory(uint64_t atm_id, const DenominationBreakdown *dispensed)
{
    if (!dispensed) return false;
    MYSQL *conn = db_get_connection();
    if (!conn) return false;

    if (!update_single_denom(conn, atm_id, 500, dispensed->count_500)) return false;
    if (!update_single_denom(conn, atm_id, 200, dispensed->count_200)) return false;
    if (!update_single_denom(conn, atm_id, 100, dispensed->count_100)) return false;
    if (!update_single_denom(conn, atm_id, 50,  dispensed->count_50))  return false;

    return true;
}
