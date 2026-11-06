#include "db.h"

#include </home/linux/nfs/rootfs/usr/include/sqlite3.h>
#include <stdio.h>

static sqlite3 * g_db = NULL;

static int db_exec(const char * sql)
{
    char * errmsg = NULL;
    int ret;

    ret = sqlite3_exec(g_db, sql, NULL, NULL, &errmsg);
    if(ret != SQLITE_OK) {
        if(errmsg != NULL) {
            fprintf(stderr, "sqlite exec failed: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
        return -1;
    }

    return 0;
}

int db_open(const char * path)
{
    if(path == NULL) {
        return -1;
    }

    if(sqlite3_open(path, &g_db) != SQLITE_OK) {
        fprintf(stderr, "sqlite open failed: %s\n", sqlite3_errmsg(g_db));
        db_close();
        return -1;
    }

    return 0;
}

void db_close(void)
{
    if(g_db != NULL) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

int db_init_schema(void)
{
    const char * sensor_sql = "CREATE TABLE IF NOT EXISTS sensor_log ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "timestamp INTEGER NOT NULL,"
                              "temperature REAL,"
                              "humidity REAL,"
                              "light_lux REAL,"
                              "mq2_raw_volt REAL,"
                              "mq2_real_ao_volt REAL,"
                              "flame_detected INTEGER,"
                              "valid_mask INTEGER NOT NULL"
                              ");";

    const char * alarm_sql = "CREATE TABLE IF NOT EXISTS alarm_log ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                             "timestamp INTEGER NOT NULL,"
                             "level INTEGER NOT NULL,"
                             "source_mask INTEGER NOT NULL,"
                             "reason TEXT,"
                             "image_path TEXT,"
                             "temperature REAL,"
                             "humidity REAL,"
                             "light_lux REAL,"
                             "mq2_real_ao_volt REAL,"
                             "flame_detected INTEGER"
                             ");";

    if(g_db == NULL) {
        return -1;
    }

    if(db_exec(sensor_sql) != 0) {
        return -1;
    }

    return db_exec(alarm_sql);
}

int db_insert_sensor_data(const sensor_data_t * data)
{
    const char * sql = "INSERT INTO sensor_log "
                       "(timestamp, temperature, humidity, light_lux, mq2_raw_volt, "
                       "mq2_real_ao_volt, flame_detected, valid_mask) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt * stmt = NULL;
    int ret;

    if(g_db == NULL || data == NULL) {
        return -1;
    }

    ret = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if(ret != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)data->timestamp);
    sqlite3_bind_double(stmt, 2, data->temperature);
    sqlite3_bind_double(stmt, 3, data->humidity);
    sqlite3_bind_double(stmt, 4, data->light_lux);
    sqlite3_bind_double(stmt, 5, data->mq2_raw_volt);
    sqlite3_bind_double(stmt, 6, data->mq2_real_ao_volt);
    sqlite3_bind_int(stmt, 7, data->flame_detected);
    sqlite3_bind_int(stmt, 8, (int)data->valid_mask);

    ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return ret == SQLITE_DONE ? 0 : -1;
}

int db_insert_alarm_event(const alarm_event_t * event)
{
    const char * sql = "INSERT INTO alarm_log "
                       "(timestamp, level, source_mask, reason, image_path, temperature, "
                       "humidity, light_lux, mq2_real_ao_volt, flame_detected) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt * stmt = NULL;
    int ret;

    if(g_db == NULL || event == NULL) {
        return -1;
    }

    ret = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if(ret != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)event->timestamp);
    sqlite3_bind_int(stmt, 2, (int)event->level);
    sqlite3_bind_int(stmt, 3, (int)event->source_mask);
    sqlite3_bind_text(stmt, 4, event->reason, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, event->image_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, event->snapshot.temperature);
    sqlite3_bind_double(stmt, 7, event->snapshot.humidity);
    sqlite3_bind_double(stmt, 8, event->snapshot.light_lux);
    sqlite3_bind_double(stmt, 9, event->snapshot.mq2_real_ao_volt);
    sqlite3_bind_int(stmt, 10, event->snapshot.flame_detected);

    ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return ret == SQLITE_DONE ? 0 : -1;
}