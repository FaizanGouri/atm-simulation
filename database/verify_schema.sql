-- ============================================================================
-- ATM Simulation System - Schema Verification Script
-- Read-only verification queries against INFORMATION_SCHEMA and atm_simulation
-- ============================================================================

USE atm_simulation;

-- 1. Table inventory
SELECT 'TABLES IN ATM_SIMULATION' AS verify_section;
SHOW TABLES;

-- 2. Structure of all 8 tables
SELECT 'DESCRIBE CUSTOMERS' AS verify_section;
DESCRIBE customers;

SELECT 'DESCRIBE ACCOUNTS' AS verify_section;
DESCRIBE accounts;

SELECT 'DESCRIBE CARDS' AS verify_section;
DESCRIBE cards;

SELECT 'DESCRIBE BENEFICIARIES' AS verify_section;
DESCRIBE beneficiaries;

SELECT 'DESCRIBE TRANSACTIONS' AS verify_section;
DESCRIBE transactions;

SELECT 'DESCRIBE ATM' AS verify_section;
DESCRIBE atm;

SELECT 'DESCRIBE ATM_CASH' AS verify_section;
DESCRIBE atm_cash;

SELECT 'DESCRIBE ADMINS' AS verify_section;
DESCRIBE admins;

-- 3. Foreign Keys Verification
SELECT 'FOREIGN KEYS' AS verify_section;
SELECT 
    CONSTRAINT_NAME,
    TABLE_NAME,
    COLUMN_NAME,
    REFERENCED_TABLE_NAME,
    REFERENCED_COLUMN_NAME
FROM 
    INFORMATION_SCHEMA.KEY_COLUMN_USAGE
WHERE 
    TABLE_SCHEMA = 'atm_simulation' 
    AND REFERENCED_TABLE_NAME IS NOT NULL
ORDER BY 
    TABLE_NAME, CONSTRAINT_NAME;

-- 4. Indexes Verification
SELECT 'NON-PRIMARY INDEXES' AS verify_section;
SELECT 
    TABLE_NAME,
    INDEX_NAME,
    NON_UNIQUE,
    COLUMN_NAME,
    SEQ_IN_INDEX
FROM 
    INFORMATION_SCHEMA.STATISTICS
WHERE 
    TABLE_SCHEMA = 'atm_simulation'
    AND INDEX_NAME <> 'PRIMARY'
ORDER BY 
    TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;

-- 5. Triggers Verification
SELECT 'ACTIVE TRIGGERS' AS verify_section;
SELECT 
    TRIGGER_NAME,
    EVENT_MANIPULATION,
    EVENT_OBJECT_TABLE,
    ACTION_TIMING
FROM 
    INFORMATION_SCHEMA.TRIGGERS
WHERE 
    TRIGGER_SCHEMA = 'atm_simulation';

-- 6. Self-Beneficiary Constraint Test
-- Tests that inserting account_id = beneficiary_account_id is rejected by trigger
-- All test rows rolled back immediately to leave zero test data behind
SELECT 'SELF-BENEFICIARY TRIGGER TEST' AS verify_section;
START TRANSACTION;

INSERT INTO customers (customer_id, customer_number, full_name, phone) 
VALUES (99999, 'CUST-TEST-01', 'Test Customer', '+919999999999');

INSERT INTO accounts (account_id, customer_id, account_number, balance) 
VALUES (99999, 99999, 'ACC-TEST-99999', 1000.00);

-- Attempt self-beneficiary insert (Must fail with SQLSTATE 45000)
-- If this fails as expected in Workbench, the transaction will rollback.
-- Run in Workbench:
-- INSERT INTO beneficiaries (account_id, beneficiary_account_id, beneficiary_name) VALUES (99999, 99999, 'Self');
ROLLBACK;
