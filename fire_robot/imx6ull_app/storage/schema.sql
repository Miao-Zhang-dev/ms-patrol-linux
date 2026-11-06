CREATE TABLE IF NOT EXISTS sensor_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    temperature REAL,
    humidity REAL,
    light_lux REAL,
    mq2_raw_volt REAL,
    mq2_real_ao_volt REAL,
    flame_detected INTEGER,
    valid_mask INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS alarm_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    level INTEGER NOT NULL,
    source_mask INTEGER NOT NULL,
    reason TEXT,
    image_path TEXT,
    temperature REAL,
    humidity REAL,
    light_lux REAL,
    mq2_real_ao_volt REAL,
    flame_detected INTEGER
);
