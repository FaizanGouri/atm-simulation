#include <stdio.h>
#include "database.h"

int main(void)
{
    printf("========================================\n");
    printf("       ATM SIMULATION SYSTEM\n");
    printf("========================================\n\n");
    printf("Phase 3: Database Connectivity Test\n\n");

    DBConfig cfg;
    if (!db_init_config(&cfg)) {
        printf("Failed to initialize database configuration.\n");
        return 1;
    }

    printf("Connecting to database '%s' at %s:%u as user '%s'...\n",
           cfg.database, cfg.host, cfg.port, cfg.user);

    if (!db_connect(&cfg)) {
        unsigned int err_code = 0;
        const char *err_msg = db_get_last_error(&err_code);
        printf("[FAILED] Database connection error (code %u): %s\n", err_code, err_msg);
        return 1;
    }

    printf("[SUCCESS] Connected to MySQL Server version: %s\n",
           mysql_get_server_info(db_get_connection()));
    printf("[SUCCESS] Connection status: Active (Ping: %s)\n",
           db_ping() ? "OK" : "FAIL");

    /* Test simple metadata query */
    MYSQL *conn = db_get_connection();
    if (mysql_query(conn, "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'atm_simulation'") == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) {
                printf("[SUCCESS] Verified schema 'atm_simulation' contains %s tables.\n", row[0]);
            }
            mysql_free_result(res);
        }
    }

    db_disconnect();
    printf("[SUCCESS] Database disconnected cleanly.\n\n");
    printf("Phase 3 Status: READY\n");

    return 0;
}
