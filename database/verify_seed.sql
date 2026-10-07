-- ============================================================================
-- ATM Simulation System - Seed Data Verification Queries
-- Non-sensitive read-only queries for row counts, denominations, and FK checks
-- ============================================================================

USE atm_simulation;

-- 1. Table row counts
SELECT 'CUSTOMERS_COUNT' AS metric, COUNT(*) AS count FROM customers
UNION ALL
SELECT 'ACCOUNTS_COUNT', COUNT(*) FROM accounts
UNION ALL
SELECT 'CARDS_COUNT', COUNT(*) FROM cards
UNION ALL
SELECT 'BENEFICIARIES_COUNT', COUNT(*) FROM beneficiaries
UNION ALL
SELECT 'TRANSACTIONS_COUNT', COUNT(*) FROM transactions
UNION ALL
SELECT 'ATM_COUNT', COUNT(*) FROM atm
UNION ALL
SELECT 'ATM_CASH_COUNT', COUNT(*) FROM atm_cash
UNION ALL
SELECT 'ADMINS_COUNT', COUNT(*) FROM admins;

-- 2. Verify ATM Denominations
SELECT denomination, quantity 
FROM atm_cash 
ORDER BY denomination;

-- 3. Verify Foreign-Key Referential Integrity (Orphan checks: all must return 0)
SELECT 'ORPHAN_ACCOUNTS' AS check_name, COUNT(*) AS orphan_count 
FROM accounts a LEFT JOIN customers c ON a.customer_id = c.customer_id WHERE c.customer_id IS NULL
UNION ALL
SELECT 'ORPHAN_CARDS', COUNT(*) 
FROM cards cd LEFT JOIN accounts a ON cd.account_id = a.account_id WHERE a.account_id IS NULL
UNION ALL
SELECT 'ORPHAN_BENEFICIARIES_SENDER', COUNT(*) 
FROM beneficiaries b LEFT JOIN accounts a ON b.account_id = a.account_id WHERE a.account_id IS NULL
UNION ALL
SELECT 'ORPHAN_BENEFICIARIES_RECEIVER', COUNT(*) 
FROM beneficiaries b LEFT JOIN accounts a ON b.beneficiary_account_id = a.account_id WHERE a.account_id IS NULL
UNION ALL
SELECT 'ORPHAN_ATM_CASH', COUNT(*) 
FROM atm_cash ac LEFT JOIN atm m ON ac.atm_id = m.atm_id WHERE m.atm_id IS NULL
UNION ALL
SELECT 'ORPHAN_TRANSACTIONS', COUNT(*) 
FROM transactions t LEFT JOIN accounts a ON t.account_id = a.account_id WHERE a.account_id IS NULL;
