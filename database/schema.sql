-- ============================================================================
-- ATM Simulation System - Database Schema
-- Target RDBMS: MySQL 8.0+
-- Storage Engine: InnoDB
-- Charset: utf8mb4 / Collation: utf8mb4_unicode_ci
-- ============================================================================

CREATE DATABASE IF NOT EXISTS atm_simulation
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE atm_simulation;

-- Disable foreign key checks during schema re-creation if needed
SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS transactions;
DROP TABLE IF EXISTS beneficiaries;
DROP TABLE IF EXISTS cards;
DROP TABLE IF EXISTS accounts;
DROP TABLE IF EXISTS customers;
DROP TABLE IF EXISTS atm_cash;
DROP TABLE IF EXISTS atm;
DROP TABLE IF EXISTS admins;

SET FOREIGN_KEY_CHECKS = 1;

-- ----------------------------------------------------------------------------
-- 1. Customers Table
-- ----------------------------------------------------------------------------
CREATE TABLE customers (
    customer_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    customer_number VARCHAR(32) NOT NULL,
    full_name VARCHAR(100) NOT NULL,
    date_of_birth DATE NULL,
    phone VARCHAR(20) NOT NULL,
    email VARCHAR(100) NULL,
    address TEXT NULL,
    status ENUM('ACTIVE', 'INACTIVE') NOT NULL DEFAULT 'ACTIVE',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT pk_customers PRIMARY KEY (customer_id),
    CONSTRAINT uq_customers_number UNIQUE (customer_number),
    CONSTRAINT chk_customers_phone CHECK (CHAR_LENGTH(phone) >= 7)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- 2. Accounts Table
-- ----------------------------------------------------------------------------
CREATE TABLE accounts (
    account_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    customer_id BIGINT UNSIGNED NOT NULL,
    account_number VARCHAR(34) NOT NULL,
    account_type ENUM('SAVINGS', 'CURRENT') NOT NULL DEFAULT 'SAVINGS',
    balance DECIMAL(15, 2) NOT NULL DEFAULT 0.00,
    daily_withdrawal_limit DECIMAL(15, 2) NOT NULL DEFAULT 25000.00,
    status ENUM('ACTIVE', 'BLOCKED', 'CLOSED') NOT NULL DEFAULT 'ACTIVE',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT pk_accounts PRIMARY KEY (account_id),
    CONSTRAINT uq_accounts_number UNIQUE (account_number),
    CONSTRAINT fk_accounts_customer FOREIGN KEY (customer_id)
        REFERENCES customers (customer_id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    CONSTRAINT chk_accounts_balance CHECK (balance >= 0.00),
    CONSTRAINT chk_accounts_daily_limit CHECK (daily_withdrawal_limit >= 0.00)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- 3. Cards Table
-- ----------------------------------------------------------------------------
CREATE TABLE cards (
    card_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id BIGINT UNSIGNED NOT NULL,
    card_number VARCHAR(19) NOT NULL,
    pin_hash CHAR(64) NOT NULL,
    failed_pin_attempts TINYINT UNSIGNED NOT NULL DEFAULT 0,
    card_status ENUM('ACTIVE', 'BLOCKED', 'EXPIRED', 'CANCELLED') NOT NULL DEFAULT 'ACTIVE',
    issued_at DATE NOT NULL,
    expiry_date DATE NOT NULL,
    last_used_at TIMESTAMP NULL DEFAULT NULL,
    CONSTRAINT pk_cards PRIMARY KEY (card_id),
    CONSTRAINT uq_cards_number UNIQUE (card_number),
    CONSTRAINT fk_cards_account FOREIGN KEY (account_id)
        REFERENCES accounts (account_id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    CONSTRAINT chk_cards_failed_attempts CHECK (failed_pin_attempts <= 3)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- 4. Beneficiaries Table
-- ----------------------------------------------------------------------------
CREATE TABLE beneficiaries (
    beneficiary_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account_id BIGINT UNSIGNED NOT NULL,
    beneficiary_account_id BIGINT UNSIGNED NOT NULL,
    beneficiary_name VARCHAR(100) NOT NULL,
    nickname VARCHAR(50) NULL,
    status ENUM('ACTIVE', 'INACTIVE') NOT NULL DEFAULT 'ACTIVE',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT pk_beneficiaries PRIMARY KEY (beneficiary_id),
    CONSTRAINT uq_beneficiaries_pair UNIQUE (account_id, beneficiary_account_id),
    CONSTRAINT fk_beneficiaries_sender FOREIGN KEY (account_id)
        REFERENCES accounts (account_id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    CONSTRAINT fk_beneficiaries_receiver FOREIGN KEY (beneficiary_account_id)
        REFERENCES accounts (account_id)
        ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Enforce non-self beneficiary rule via MySQL trigger (MySQL 8.0 prohibits CHECK constraints referencing FK columns with cascading actions: Error 3823)
DELIMITER $$
CREATE TRIGGER trg_beneficiaries_prevent_self_insert
BEFORE INSERT ON beneficiaries
FOR EACH ROW
BEGIN
    IF NEW.account_id = NEW.beneficiary_account_id THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'A bank account cannot add itself as a beneficiary';
    END IF;
END$$

CREATE TRIGGER trg_beneficiaries_prevent_self_update
BEFORE UPDATE ON beneficiaries
FOR EACH ROW
BEGIN
    IF NEW.account_id = NEW.beneficiary_account_id THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'A bank account cannot add itself as a beneficiary';
    END IF;
END$$
DELIMITER ;

-- ----------------------------------------------------------------------------
-- 5. ATM Table
-- ----------------------------------------------------------------------------
CREATE TABLE atm (
    atm_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    atm_code VARCHAR(32) NOT NULL,
    location VARCHAR(255) NOT NULL,
    status ENUM('ACTIVE', 'INACTIVE', 'MAINTENANCE') NOT NULL DEFAULT 'ACTIVE',
    last_refilled_at TIMESTAMP NULL DEFAULT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT pk_atm PRIMARY KEY (atm_id),
    CONSTRAINT uq_atm_code UNIQUE (atm_code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- 6. ATM Cash Inventory Table
-- ----------------------------------------------------------------------------
CREATE TABLE atm_cash (
    atm_cash_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    atm_id BIGINT UNSIGNED NOT NULL,
    denomination INT UNSIGNED NOT NULL,
    quantity INT UNSIGNED NOT NULL DEFAULT 0,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    CONSTRAINT pk_atm_cash PRIMARY KEY (atm_cash_id),
    CONSTRAINT uq_atm_denomination UNIQUE (atm_id, denomination),
    CONSTRAINT fk_atm_cash_atm FOREIGN KEY (atm_id)
        REFERENCES atm (atm_id)
        ON UPDATE CASCADE ON DELETE CASCADE,
    CONSTRAINT chk_atm_cash_denomination CHECK (denomination IN (50, 100, 200, 500))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- 7. Transactions Table
-- ----------------------------------------------------------------------------
CREATE TABLE transactions (
    transaction_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    transaction_reference VARCHAR(36) NOT NULL,
    account_id BIGINT UNSIGNED NOT NULL,
    transaction_type ENUM('WITHDRAWAL', 'DEPOSIT', 'TRANSFER', 'BALANCE_ENQUIRY', 'PIN_CHANGE') NOT NULL,
    amount DECIMAL(15, 2) NOT NULL DEFAULT 0.00,
    balance_before DECIMAL(15, 2) NOT NULL DEFAULT 0.00,
    balance_after DECIMAL(15, 2) NOT NULL DEFAULT 0.00,
    related_account_id BIGINT UNSIGNED NULL DEFAULT NULL,
    atm_id BIGINT UNSIGNED NULL DEFAULT NULL,
    transaction_status ENUM('SUCCESS', 'FAILED', 'REVERSED') NOT NULL DEFAULT 'SUCCESS',
    description VARCHAR(255) NULL DEFAULT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT pk_transactions PRIMARY KEY (transaction_id),
    CONSTRAINT uq_transactions_ref UNIQUE (transaction_reference),
    CONSTRAINT fk_transactions_account FOREIGN KEY (account_id)
        REFERENCES accounts (account_id)
        ON UPDATE CASCADE ON DELETE RESTRICT,
    CONSTRAINT fk_transactions_related FOREIGN KEY (related_account_id)
        REFERENCES accounts (account_id)
        ON UPDATE CASCADE ON DELETE SET NULL,
    CONSTRAINT fk_transactions_atm FOREIGN KEY (atm_id)
        REFERENCES atm (atm_id)
        ON UPDATE CASCADE ON DELETE SET NULL,
    CONSTRAINT chk_transactions_amount CHECK (amount >= 0.00)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- 8. Admins Table
-- ----------------------------------------------------------------------------
CREATE TABLE admins (
    admin_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL,
    password_hash CHAR(64) NOT NULL,
    full_name VARCHAR(100) NOT NULL,
    role ENUM('SUPER_ADMIN', 'ADMIN', 'OPERATOR') NOT NULL DEFAULT 'ADMIN',
    status ENUM('ACTIVE', 'INACTIVE', 'SUSPENDED') NOT NULL DEFAULT 'ACTIVE',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_at TIMESTAMP NULL DEFAULT NULL,
    CONSTRAINT pk_admins PRIMARY KEY (admin_id),
    CONSTRAINT uq_admins_username UNIQUE (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ----------------------------------------------------------------------------
-- Strategic Indexes
-- ----------------------------------------------------------------------------
CREATE INDEX idx_customers_phone ON customers (phone);
CREATE INDEX idx_accounts_customer ON accounts (customer_id);
CREATE INDEX idx_cards_account ON cards (account_id);
CREATE INDEX idx_beneficiaries_account ON beneficiaries (account_id);
CREATE INDEX idx_transactions_account_date ON transactions (account_id, created_at DESC);
CREATE INDEX idx_transactions_type ON transactions (transaction_type);
CREATE INDEX idx_atm_cash_lookup ON atm_cash (atm_id, denomination);
