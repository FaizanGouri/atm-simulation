#include "database.h"
#include "security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MYSQL *g_db_conn = NULL;
static char g_last_error_msg[512] = {0};
static unsigned int g_last_error_code = 0;

static void set_last_error(const char *msg, unsigned int code)
{
    if (msg) {
        strncpy(g_last_error_msg, msg, sizeof(g_last_error_msg) - 1);
        g_last_error_msg[sizeof(g_last_error_msg) - 1] = '\0';
    } else {
        g_last_error_msg[0] = '\0';
    }
    g_last_error_code = code;
}

static void trim_whitespace(char *str)
{
    if (!str) return;
    char *start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n')) {
        end--;
    }
    *end = '\0';
    if (start != str) {
        memmove(str, start, (size_t)(end - start + 1));
    }
}

static void load_from_ini_file(DBConfig *config, const char *filepath)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim_whitespace(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == '\0') {
            security_secure_zero(line, sizeof(line));
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) {
            security_secure_zero(line, sizeof(line));
            continue;
        }

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim_whitespace(key);
        trim_whitespace(val);

        if (strcmp(key, "DB_HOST") == 0 && val[0] != '\0') {
            strncpy(config->host, val, sizeof(config->host) - 1);
            config->host[sizeof(config->host) - 1] = '\0';
        } else if (strcmp(key, "DB_PORT") == 0 && val[0] != '\0') {
            config->port = (unsigned int)atoi(val);
        } else if (strcmp(key, "DB_USER") == 0 && val[0] != '\0') {
            strncpy(config->user, val, sizeof(config->user) - 1);
            config->user[sizeof(config->user) - 1] = '\0';
        } else if (strcmp(key, "DB_PASSWORD") == 0 && val[0] != '\0') {
            strncpy(config->password, val, sizeof(config->password) - 1);
            config->password[sizeof(config->password) - 1] = '\0';
        } else if (strcmp(key, "DB_NAME") == 0 && val[0] != '\0') {
            strncpy(config->database, val, sizeof(config->database) - 1);
            config->database[sizeof(config->database) - 1] = '\0';
        }
        security_secure_zero(line, sizeof(line));
    }

    fclose(fp);
}

bool db_init_config(DBConfig *config)
{
    if (!config) {
        return false;
    }

    /* 1. Default fallback parameters */
    strncpy(config->host, "127.0.0.1", sizeof(config->host) - 1);
    config->host[sizeof(config->host) - 1] = '\0';
    config->port = 3306;
    strncpy(config->user, "atm_app", sizeof(config->user) - 1);
    config->user[sizeof(config->user) - 1] = '\0';
    config->password[0] = '\0';
    strncpy(config->database, "atm_simulation", sizeof(config->database) - 1);
    config->database[sizeof(config->database) - 1] = '\0';

    /* 2. Check Git-ignored local configuration file */
    load_from_ini_file(config, "config.local.ini");

    /* 3. Environment variables override file settings */
    const char *env_host = getenv("DB_HOST");
    if (env_host && env_host[0] != '\0') {
        strncpy(config->host, env_host, sizeof(config->host) - 1);
        config->host[sizeof(config->host) - 1] = '\0';
    }

    const char *env_port = getenv("DB_PORT");
    if (env_port && env_port[0] != '\0') {
        config->port = (unsigned int)atoi(env_port);
    }

    const char *env_user = getenv("DB_USER");
    if (env_user && env_user[0] != '\0') {
        strncpy(config->user, env_user, sizeof(config->user) - 1);
        config->user[sizeof(config->user) - 1] = '\0';
    }

    const char *env_pass = getenv("DB_PASSWORD");
    if (env_pass && env_pass[0] != '\0') {
        strncpy(config->password, env_pass, sizeof(config->password) - 1);
        config->password[sizeof(config->password) - 1] = '\0';
    }

    const char *env_name = getenv("DB_NAME");
    if (env_name && env_name[0] != '\0') {
        strncpy(config->database, env_name, sizeof(config->database) - 1);
        config->database[sizeof(config->database) - 1] = '\0';
    }

    return true;
}

bool db_connect(const DBConfig *config)
{
    DBConfig local_cfg;

    if (g_db_conn) {
        db_disconnect();
    }

    if (config) {
        local_cfg = *config;
    } else {
        if (!db_init_config(&local_cfg)) {
            set_last_error("Failed to initialize database configuration", 0);
            return false;
        }
    }

    g_db_conn = mysql_init(NULL);
    if (!g_db_conn) {
        set_last_error("mysql_init() failed to allocate connection structure", 0);
        return false;
    }

    /* Configure connection timeout */
    unsigned int connect_timeout = 5;
    mysql_options(g_db_conn, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);

    /* Enforce utf8mb4 encoding */
    mysql_options(g_db_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    bool connected = (mysql_real_connect(g_db_conn,
                                         local_cfg.host,
                                         local_cfg.user,
                                         local_cfg.password,
                                         local_cfg.database,
                                         local_cfg.port,
                                         NULL,
                                         0) != NULL);

    /* Immediately wipe temporary local password buffer */
    security_secure_zero(local_cfg.password, sizeof(local_cfg.password));

    if (!connected) {
        set_last_error(mysql_error(g_db_conn), mysql_errno(g_db_conn));
        mysql_close(g_db_conn);
        g_db_conn = NULL;
        return false;
    }

    set_last_error(NULL, 0);
    return true;
}

bool db_is_connected(void)
{
    return (g_db_conn != NULL);
}

MYSQL *db_get_connection(void)
{
    return g_db_conn;
}

void db_disconnect(void)
{
    if (g_db_conn) {
        mysql_close(g_db_conn);
        g_db_conn = NULL;
    }
    set_last_error(NULL, 0);
}

const char *db_get_last_error(unsigned int *err_code)
{
    if (err_code) {
        *err_code = g_last_error_code;
    }
    return g_last_error_msg;
}

bool db_ping(void)
{
    if (!g_db_conn) {
        set_last_error("Database is not connected", 0);
        return false;
    }
    if (mysql_ping(g_db_conn) != 0) {
        set_last_error(mysql_error(g_db_conn), mysql_errno(g_db_conn));
        return false;
    }
    return true;
}

bool db_transaction_begin(void)
{
    if (!g_db_conn) {
        set_last_error("Cannot begin transaction: database disconnected", 0);
        return false;
    }
    if (mysql_query(g_db_conn, "START TRANSACTION") != 0) {
        set_last_error(mysql_error(g_db_conn), mysql_errno(g_db_conn));
        return false;
    }
    return true;
}

bool db_transaction_commit(void)
{
    if (!g_db_conn) {
        set_last_error("Cannot commit transaction: database disconnected", 0);
        return false;
    }
    if (mysql_commit(g_db_conn) != 0) {
        set_last_error(mysql_error(g_db_conn), mysql_errno(g_db_conn));
        return false;
    }
    return true;
}

bool db_transaction_rollback(void)
{
    if (!g_db_conn) {
        set_last_error("Cannot rollback transaction: database disconnected", 0);
        return false;
    }
    if (mysql_rollback(g_db_conn) != 0) {
        set_last_error(mysql_error(g_db_conn), mysql_errno(g_db_conn));
        return false;
    }
    return true;
}
