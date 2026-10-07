# ATM Simulation System - Database Design Specification

## 1. Database Overview
* **Database Name**: `atm_simulation`
* **RDBMS Engine**: MySQL 8.0 (Storage Engine: InnoDB)
* **Character Set / Collation**: `utf8mb4` / `utf8mb4_unicode_ci`
* **Financial Data Representation**: `DECIMAL(15, 2)` (strictly avoiding floating-point rounding errors for all balances, amounts, and limits)

---

## 2. Table Specifications & Entities

### 2.1 `customers`
Stores master customer profile and contact details.
* `customer_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `customer_number` (VARCHAR(32), UNIQUE, NOT NULL) — Human-readable customer code (e.g. `CUST-1001`)
* `full_name` (VARCHAR(100), NOT NULL)
* `date_of_birth` (DATE, NULL)
* `phone` (VARCHAR(20), NOT NULL)
* `email` (VARCHAR(100), NULL)
* `address` (TEXT, NULL)
* `status` (ENUM('ACTIVE', 'INACTIVE'), NOT NULL DEFAULT 'ACTIVE')
* `created_at` (TIMESTAMP, NOT NULL DEFAULT CURRENT_TIMESTAMP)

### 2.2 `accounts`
Stores bank account records associated with a customer.
* `account_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `customer_id` (BIGINT UNSIGNED, NOT NULL, FK -> `customers.customer_id`)
* `account_number` (VARCHAR(34), UNIQUE, NOT NULL) — Unique bank account number (e.g. `ACC1000000001`)
* `account_type` (ENUM('SAVINGS', 'CURRENT'), NOT NULL DEFAULT 'SAVINGS')
* `balance` (DECIMAL(15, 2), NOT NULL DEFAULT 0.00, CHECK (`balance` >= 0.00))
* `daily_withdrawal_limit` (DECIMAL(15, 2), NOT NULL DEFAULT 25000.00, CHECK (`daily_withdrawal_limit` >= 0.00))
* `status` (ENUM('ACTIVE', 'BLOCKED', 'CLOSED'), NOT NULL DEFAULT 'ACTIVE')
* `created_at` (TIMESTAMP, NOT NULL DEFAULT CURRENT_TIMESTAMP)

### 2.3 `cards`
Represents physical debit/ATM cards linked to bank accounts.
* `card_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `account_id` (BIGINT UNSIGNED, NOT NULL, FK -> `accounts.account_id`)
* `card_number` (VARCHAR(19), UNIQUE, NOT NULL) — Formatted 16-digit card number
* `pin_hash` (CHAR(64), NOT NULL) — 64-character SHA-256 hexadecimal hash (Plaintext PINs are strictly prohibited)
* `failed_pin_attempts` (TINYINT UNSIGNED, NOT NULL DEFAULT 0, CHECK (`failed_pin_attempts` <= 3))
* `card_status` (ENUM('ACTIVE', 'BLOCKED', 'EXPIRED', 'CANCELLED'), NOT NULL DEFAULT 'ACTIVE')
* `issued_at` (DATE, NOT NULL)
* `expiry_date` (DATE, NOT NULL)
* `last_used_at` (TIMESTAMP, NULL)

### 2.4 `beneficiaries`
Maintains registered transfer recipients for an account.
* `beneficiary_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `account_id` (BIGINT UNSIGNED, NOT NULL, FK -> `accounts.account_id`)
* `beneficiary_account_id` (BIGINT UNSIGNED, NOT NULL, FK -> `accounts.account_id`)
* `beneficiary_name` (VARCHAR(100), NOT NULL)
* `nickname` (VARCHAR(50), NULL)
* `status` (ENUM('ACTIVE', 'INACTIVE'), NOT NULL DEFAULT 'ACTIVE')
* `created_at` (TIMESTAMP, NOT NULL DEFAULT CURRENT_TIMESTAMP)
* *Constraint*: Enforced via MySQL triggers (`trg_beneficiaries_prevent_self_insert` and `trg_beneficiaries_prevent_self_update`) to prevent self-beneficiary entries (`account_id` <> `beneficiary_account_id`), avoiding MySQL 8.0 Error 3823 on foreign key columns with cascading actions.
* *Constraint*: UNIQUE (`account_id`, `beneficiary_account_id`) to prevent duplicate mappings.

### 2.5 `transactions`
Immutable ledger of financial and non-financial transactions.
* `transaction_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `transaction_reference` (VARCHAR(36), UNIQUE, NOT NULL) — e.g., `TXN20261007142301`
* `account_id` (BIGINT UNSIGNED, NOT NULL, FK -> `accounts.account_id`)
* `transaction_type` (ENUM('WITHDRAWAL', 'DEPOSIT', 'TRANSFER', 'BALANCE_ENQUIRY', 'PIN_CHANGE'), NOT NULL)
* `amount` (DECIMAL(15, 2), NOT NULL DEFAULT 0.00, CHECK (`amount` >= 0.00))
* `balance_before` (DECIMAL(15, 2), NOT NULL DEFAULT 0.00)
* `balance_after` (DECIMAL(15, 2), NOT NULL DEFAULT 0.00)
* `related_account_id` (BIGINT UNSIGNED, NULL, FK -> `accounts.account_id`) — Beneficiary/counterparty account for transfers
* `atm_id` (BIGINT UNSIGNED, NULL, FK -> `atm.atm_id`) — Originating ATM for cash operations
* `transaction_status` (ENUM('SUCCESS', 'FAILED', 'REVERSED'), NOT NULL DEFAULT 'SUCCESS')
* `description` (VARCHAR(255), NULL)
* `created_at` (TIMESTAMP, NOT NULL DEFAULT CURRENT_TIMESTAMP)

### 2.6 `atm`
Terminal metadata and state.
* `atm_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `atm_code` (VARCHAR(32), UNIQUE, NOT NULL) — e.g., `ATM-DELHI-001`
* `location` (VARCHAR(255), NOT NULL)
* `status` (ENUM('ACTIVE', 'INACTIVE', 'MAINTENANCE'), NOT NULL DEFAULT 'ACTIVE')
* `last_refilled_at` (TIMESTAMP, NULL)
* `created_at` (TIMESTAMP, NOT NULL DEFAULT CURRENT_TIMESTAMP)

### 2.7 `atm_cash`
Physical cash vault inventory broken down by banknote denominations.
* `atm_cash_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `atm_id` (BIGINT UNSIGNED, NOT NULL, FK -> `atm.atm_id` ON DELETE CASCADE)
* `denomination` (INT UNSIGNED, NOT NULL, CHECK (`denomination` IN (50, 100, 200, 500)))
* `quantity` (INT UNSIGNED, NOT NULL DEFAULT 0)
* `updated_at` (TIMESTAMP, NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP)
* *Constraint*: UNIQUE (`atm_id`, `denomination`) ensures exactly one inventory row per denomination per ATM.

### 2.8 `admins`
System administration and maintenance operators.
* `admin_id` (BIGINT UNSIGNED, AUTO_INCREMENT, PK)
* `username` (VARCHAR(50), UNIQUE, NOT NULL)
* `password_hash` (CHAR(64), NOT NULL) — SHA-256 digest
* `full_name` (VARCHAR(100), NOT NULL)
* `role` (ENUM('SUPER_ADMIN', 'ADMIN', 'OPERATOR'), NOT NULL DEFAULT 'ADMIN')
* `status` (ENUM('ACTIVE', 'INACTIVE', 'SUSPENDED'), NOT NULL DEFAULT 'ACTIVE')
* `created_at` (TIMESTAMP, NOT NULL DEFAULT CURRENT_TIMESTAMP)
* `last_login_at` (TIMESTAMP, NULL)

---

## 3. Relational Mapping & Cardinality

```text
CUSTOMERS (1)  ────────< (N) ACCOUNTS
                              │ (1)
                              ├────────< (N) CARDS
                              ├────────< (N) TRANSACTIONS
                              ├────────< (N) BENEFICIARIES (as sender)
                              └────────< (N) BENEFICIARIES (as recipient)

ATM (1)        ────────< (N) ATM_CASH
ATM (1)        ────────< (N) TRANSACTIONS (optional foreign key)

ADMINS         (Role-based operational management & audit)
```

---

## 4. Normalization Analysis

* **First Normal Form (1NF)**:
  - All tables contain single atomic values in each column (e.g., individual denomination rows in `atm_cash`, individual transactions in `transactions`, separate names/contacts).
  - Every table has a clearly designated Primary Key (`AUTO_INCREMENT` surrogate key) ensuring tuple uniqueness.
  - No repeating groups or comma-separated lists.

* **Second Normal Form (2NF)**:
  - Meets all 1NF criteria.
  - In all tables, every non-key attribute is fully functionally dependent on the primary key. Composite business keys (e.g. `atm_id` + `denomination`) are enforced via unique constraints, eliminating partial key dependencies.

* **Third Normal Form (3NF)**:
  - Meets all 2NF criteria.
  - No transitive dependencies exist between non-key attributes. For example, customer profile details (`full_name`, `phone`) reside in `customers` and are referenced by `accounts` via `customer_id`, rather than replicating customer address or status in `accounts`. Transaction metadata references `account_id` and does not replicate account owner data.

---

## 5. Strategic Indexing Plan

1. `customers(customer_number)` — Fast customer lookup during administrative onboarding.
2. `accounts(account_number)` — Crucial for direct account resolution and receiver lookup in fund transfers.
3. `cards(card_number)` — High-frequency lookup upon ATM card insertion.
4. `cards(account_id)` — Foreign key navigation to retrieve associated bank balance.
5. `transactions(transaction_reference)` — Unique receipt verification and idempotency check.
6. `transactions(account_id, created_at)` — Optimized composite index for Mini Statement retrieval (`ORDER BY created_at DESC LIMIT 10`).
7. `beneficiaries(account_id)` — Rapid retrieval of beneficiary lists for fund transfers.
8. `atm(atm_code)` — Machine registration lookup.
9. `atm_cash(atm_id, denomination)` — Instant retrieval of available note denominations.
