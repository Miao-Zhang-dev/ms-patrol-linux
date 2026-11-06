#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define SENSOR_SAMPLE_PERIOD_MS 1000
#define LVGL_TIMER_PERIOD_MS 20

#define ALARM_TEMP_WARN_C 50.0f
#define ALARM_TEMP_DANGER_C 65.0f
#define ALARM_MQ2_WARN_VOLT 1.20f
#define ALARM_MQ2_DANGER_VOLT 1.80f
#define ALARM_EVENT_COOLDOWN_S 5

#define DB_PATH "/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/fire_robot/fire_robot.db"
#define DB_SENSOR_SAVE_INTERVAL_S 5

#define ALARM_IMAGE_DIR "/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/fire_robot/alarm_images"
#define WEB_LISTEN_PORT 8080
#define WEB_QUERY_LIMIT 20

#define MQTT_BROKER_HOST "127.0.0.1"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "fire_robot_imx6ull"
#define MQTT_SENSOR_TOPIC "fire_robot/sensor"
#define MQTT_ALARM_TOPIC "fire_robot/alarm"
#define MQTT_KEEPALIVE_S 30
#define MQTT_RECONNECT_DELAY_MS 3000

#endif
