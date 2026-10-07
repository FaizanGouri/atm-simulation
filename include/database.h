#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <mysql.h>

/**
 * Structure holding database configuration parameters.
 * Note: Never hardcode credentials into source or header files.
 */
typedef struct {
    char host[64];
    unsigned int port;
    char user[64];
    char password[128];
    char database[64];
} DBConfig;

/**
 * Initialize database module and load configuration safely.
 * Configuration priority:
 * 1. Environment variables (DB_HOST, DB_PORT, DB_USER, DB_PASSWORD, DB_NAME)
 * 2. Git-ignored local configuration file (config.local.ini)
 * Defaults:
 *   host: "127.0.0.1"
 *   port: 3306
 *   user: "atm_app"
 *   database: "atm_simulation"
 *
 * @param config Pointer to output DBConfig structure.
 * @return true if configuration was populated, false on error.
 */
bool db_init_config(DBConfig *config);

/**
 * Establish connection to MySQL server using provided or default config.
 *
 * @param config Pointer to DBConfig, or NULL to load automatically via db_init_config().
 * @return true if connection established successfully, false otherwise.
 */
bool db_connect(const DBConfig *config);

/**
 * Check whether database is currently connected.
 *
 * @return true if active connection exists, false otherwise.
 */
bool db_is_connected(void);

/**
 * Get the underlying MYSQL connection handle.
 *
 * @return Pointer to active MYSQL struct, or NULL if not connected.
 */
MYSQL *db_get_connection(void);

/**
 * Close active database connection and free associated driver resources.
 */
void db_disconnect(void);

/**
 * Get latest database error message and error code.
 *
 * @param err_code Pointer to store MySQL error code (optional, can be NULL).
 * @return Static error string describing the last error.
 */
const char *db_get_last_error(unsigned int *err_code);

/**
 * Ping the server to check/refresh connection liveness.
 *
 * @return true if ping succeeded, false otherwise.
 */
bool db_ping(void);

/**
 * Transaction control functions.
 */
bool db_transaction_begin(void);
bool db_transaction_commit(void);
bool db_transaction_rollback(void);

#endif /* DATABASE_H */
