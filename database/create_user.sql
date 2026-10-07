-- ============================================================================
-- ATM Simulation System - Application User & Privileges Setup
-- ============================================================================

CREATE DATABASE IF NOT EXISTS atm_simulation
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

-- Create dedicated least-privilege application user
-- IMPORTANT: The application password must be supplied interactively and must never be stored in this file.
-- Template placeholder:
CREATE USER IF NOT EXISTS 'atm_app'@'localhost' IDENTIFIED BY '<APPLICATION_PASSWORD>';

-- Grant required operational privileges on application database only
GRANT SELECT, INSERT, UPDATE, DELETE ON atm_simulation.* TO 'atm_app'@'localhost';

FLUSH PRIVILEGES;
