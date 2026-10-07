-- ============================================================================
-- ATM Simulation System - Seed / Demo Data Script
-- ENVIRONMENT: DEVELOPMENT / DEMO DATA ONLY
-- NOTE: Never use these credentials or records in production.
-- ============================================================================

USE atm_simulation;

SET FOREIGN_KEY_CHECKS = 0;
TRUNCATE TABLE transactions;
TRUNCATE TABLE beneficiaries;
TRUNCATE TABLE cards;
TRUNCATE TABLE accounts;
TRUNCATE TABLE customers;
TRUNCATE TABLE atm_cash;
TRUNCATE TABLE atm;
TRUNCATE TABLE admins;
SET FOREIGN_KEY_CHECKS = 1;

-- ----------------------------------------------------------------------------
-- 1. Demo Customers
-- ----------------------------------------------------------------------------
INSERT INTO customers (customer_id, customer_number, full_name, date_of_birth, phone, email, address, status) VALUES
(1, 'CUST-1001', 'Rahul Sharma', '1995-05-14', '+919876543210', 'rahul.sharma@example.com', 'A-12, Sector 15, Noida, UP', 'ACTIVE'),
(2, 'CUST-1002', 'Priya Verma',  '1998-11-20', '+919876543211', 'priya.verma@example.com',  '45-B, Park Street, Kolkata, WB', 'ACTIVE'),
(3, 'CUST-1003', 'Amit Patel',   '1992-03-08', '+919876543212', 'amit.patel@example.com',   '78, CG Road, Ahmedabad, GJ', 'ACTIVE');

-- ----------------------------------------------------------------------------
-- 2. Demo Accounts
-- ----------------------------------------------------------------------------
INSERT INTO accounts (account_id, customer_id, account_number, account_type, balance, daily_withdrawal_limit, status) VALUES
(1, 1, 'ACC1000000001', 'SAVINGS', 45000.00, 25000.00, 'ACTIVE'),
(2, 2, 'ACC1000000002', 'SAVINGS', 75000.00, 40000.00, 'ACTIVE'),
(3, 3, 'ACC1000000003', 'CURRENT', 120000.00, 50000.00, 'ACTIVE');

-- ----------------------------------------------------------------------------
-- 3. Demo Cards
-- Hash references: NON-PRODUCTION DETERMINISTIC SAMPLE SHA-256 DIGESTS ONLY
-- Card 1: Sample Demo PIN -> 03ac674216f3e15c761ee1a5e255f067953623c8b388b4459e13f978d7c846f4
-- Card 2: Sample Demo PIN -> 1eb5211d141e98d9298f553f7706d86a60e0a5c43d81b4f4f5f590558b38615a
-- Card 3: Sample Demo PIN -> c149d29486c7d9bc376378c2e71d3c0542385311de1ff745bbdf6ae985fb4a4f
-- ----------------------------------------------------------------------------
INSERT INTO cards (card_id, account_id, card_number, pin_hash, failed_pin_attempts, card_status, issued_at, expiry_date) VALUES
(1, 1, '4532015012340001', '03ac674216f3e15c761ee1a5e255f067953623c8b388b4459e13f978d7c846f4', 0, 'ACTIVE', '2024-01-01', '2029-12-31'),
(2, 2, '4532015012340002', '1eb5211d141e98d9298f553f7706d86a60e0a5c43d81b4f4f5f590558b38615a', 0, 'ACTIVE', '2024-02-15', '2029-12-31'),
(3, 3, '4532015012340003', 'c149d29486c7d9bc376378c2e71d3c0542385311de1ff745bbdf6ae985fb4a4f', 0, 'ACTIVE', '2023-06-10', '2028-06-30');

-- ----------------------------------------------------------------------------
-- 4. Demo Beneficiaries
-- Account 1 has Account 2 as registered beneficiary
-- Account 2 has Account 3 as registered beneficiary
-- ----------------------------------------------------------------------------
INSERT INTO beneficiaries (beneficiary_id, account_id, beneficiary_account_id, beneficiary_name, nickname, status) VALUES
(1, 1, 2, 'Priya Verma', 'Priya Svg', 'ACTIVE'),
(2, 2, 3, 'Amit Patel', 'Amit Business', 'ACTIVE');

-- ----------------------------------------------------------------------------
-- 5. Demo ATM Terminal
-- ----------------------------------------------------------------------------
INSERT INTO atm (atm_id, atm_code, location, status, last_refilled_at) VALUES
(1, 'ATM-DELHI-001', 'Connaught Place Branch, New Delhi', 'ACTIVE', CURRENT_TIMESTAMP);

-- ----------------------------------------------------------------------------
-- 6. Demo ATM Cash Inventory
-- Total Initial ATM Cash: (120*500) + (80*200) + (100*100) + (40*50) = 60000 + 16000 + 10000 + 2000 = Rs. 88,000.00
-- ----------------------------------------------------------------------------
INSERT INTO atm_cash (atm_cash_id, atm_id, denomination, quantity) VALUES
(1, 1, 500, 120),
(2, 1, 200, 80),
(3, 1, 100, 100),
(4, 1, 50,  40);

-- ----------------------------------------------------------------------------
-- 7. Demo Initial Transactions (Audit History)
-- ----------------------------------------------------------------------------
INSERT INTO transactions (transaction_id, transaction_reference, account_id, transaction_type, amount, balance_before, balance_after, related_account_id, atm_id, transaction_status, description, created_at) VALUES
(1, 'TXN20261001100001', 1, 'DEPOSIT', 50000.00, 0.00, 50000.00, NULL, 1, 'SUCCESS', 'Opening cash deposit', '2026-10-01 10:00:01'),
(2, 'TXN20261002143022', 1, 'WITHDRAWAL', 5000.00, 50000.00, 45000.00, NULL, 1, 'SUCCESS', 'ATM cash withdrawal', '2026-10-02 14:30:22'),
(3, 'TXN20261003091510', 2, 'DEPOSIT', 75000.00, 0.00, 75000.00, NULL, 1, 'SUCCESS', 'Opening account balance deposit', '2026-10-03 09:15:10'),
(4, 'TXN20261003112045', 3, 'DEPOSIT', 120000.00, 0.00, 120000.00, NULL, 1, 'SUCCESS', 'Commercial account initial deposit', '2026-10-03 11:20:45');

-- ----------------------------------------------------------------------------
-- 8. Demo Administrator (NON-PRODUCTION DEVELOPMENT SAMPLE RECORD ONLY)
-- Username: admin
-- Password hash: Sample SHA-256 digest
-- ----------------------------------------------------------------------------
INSERT INTO admins (admin_id, username, password_hash, full_name, role, status) VALUES
(1, 'admin', '240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9', 'System Administrator', 'SUPER_ADMIN', 'ACTIVE');
