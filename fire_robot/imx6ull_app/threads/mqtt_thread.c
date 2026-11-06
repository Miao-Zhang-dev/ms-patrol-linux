#include "mqtt_thread.h"

#include "app_config.h"
#include "app_context.h"
#include "app_msg.h"
#include "logger.h"
#include "mailbox.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
    int sockfd;
    time_t last_activity;
} mqtt_client_t;

static const char * alarm_level_name(alarm_level_t level)
{
    switch(level) {
        case ALARM_LEVEL_WARN: return "WARN";
        case ALARM_LEVEL_DANGER: return "DANGER";
        case ALARM_LEVEL_NORMAL:
        default: return "NORMAL";
    }
}

static int send_all(int fd, const void * buf, size_t len)
{
    const uint8_t * p = (const uint8_t *)buf;

    while(len > 0u) {
        ssize_t ret = send(fd, p, len, 0);
        if(ret < 0) {
            if(errno == EINTR) {
                continue;
            }
            return -errno;
        }
        if(ret == 0) {
            return -EPIPE;
        }
        p += ret;
        len -= (size_t)ret;
    }

    return 0;
}

static int recv_all(int fd, void * buf, size_t len)
{
    uint8_t * p = (uint8_t *)buf;

    while(len > 0u) {
        ssize_t ret = recv(fd, p, len, 0);
        if(ret < 0) {
            if(errno == EINTR) {
                continue;
            }
            return -errno;
        }
        if(ret == 0) {
            return -ECONNRESET;
        }
        p += ret;
        len -= (size_t)ret;
    }

    return 0;
}

static size_t encode_remaining_length(uint8_t * buf, size_t len)
{
    size_t i = 0u;

    do {
        uint8_t digit = (uint8_t)(len % 128u);
        len /= 128u;
        if(len > 0u) {
            digit |= 0x80u;
        }
        buf[i++] = digit;
    } while(len > 0u && i < 4u);

    return i;
}

static void mqtt_disconnect(mqtt_client_t * client)
{
    if(client == NULL) {
        return;
    }

    if(client->sockfd >= 0) {
        close(client->sockfd);
        client->sockfd = -1;
    }
    client->last_activity = 0;
}

static int mqtt_connect_socket(mqtt_client_t * client)
{
    char port_str[16];
    struct addrinfo hints;
    struct addrinfo * result = NULL;
    struct addrinfo * rp;
    int sockfd = -1;
    int ret;

    if(client == NULL) {
        return -EINVAL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_str, sizeof(port_str), "%d", MQTT_BROKER_PORT);
    ret = getaddrinfo(MQTT_BROKER_HOST, port_str, &hints, &result);
    if(ret != 0) {
        LOG_ERROR("mqtt getaddrinfo failed: %s", gai_strerror(ret));
        return -EINVAL;
    }

    for(rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sockfd < 0) {
            continue;
        }

        if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            struct timeval tv;

            tv.tv_sec  = 3;
            tv.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            client->sockfd = sockfd;
            freeaddrinfo(result);
            return 0;
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(result);
    return -ECONNREFUSED;
}

static int mqtt_send_connect(mqtt_client_t * client)
{
    uint8_t packet[256];
    size_t pos = 0u;
    size_t remaining_len;
    size_t rem_len_pos;
    uint8_t rem_len_buf[4];
    size_t rem_len_len;
    uint8_t connack[4];
    size_t client_id_len = strlen(MQTT_CLIENT_ID);

    if(client == NULL || client->sockfd < 0) {
        return -EINVAL;
    }

    remaining_len = 2u + 4u + 1u + 1u + 2u + client_id_len;
    packet[pos++] = 0x10u;
    rem_len_pos   = pos;
    rem_len_len   = encode_remaining_length(rem_len_buf, remaining_len);
    memcpy(packet + pos, rem_len_buf, rem_len_len);
    pos += rem_len_len;

    packet[pos++] = 0x00u;
    packet[pos++] = 0x04u;
    packet[pos++] = 'M';
    packet[pos++] = 'Q';
    packet[pos++] = 'T';
    packet[pos++] = 'T';
    packet[pos++] = 0x04u;
    packet[pos++] = 0x02u;
    packet[pos++] = (uint8_t)((MQTT_KEEPALIVE_S >> 8) & 0xffu);
    packet[pos++] = (uint8_t)(MQTT_KEEPALIVE_S & 0xffu);
    packet[pos++] = (uint8_t)((client_id_len >> 8) & 0xffu);
    packet[pos++] = (uint8_t)(client_id_len & 0xffu);
    memcpy(packet + pos, MQTT_CLIENT_ID, client_id_len);
    pos += client_id_len;

    (void)rem_len_pos;
    if(send_all(client->sockfd, packet, pos) != 0) {
        return -EIO;
    }

    if(recv_all(client->sockfd, connack, sizeof(connack)) != 0) {
        return -EIO;
    }

    if(connack[0] != 0x20u || connack[1] != 0x02u || connack[3] != 0x00u) {
        return -ECONNREFUSED;
    }

    client->last_activity = time(NULL);
    return 0;
}

static int mqtt_send_ping(mqtt_client_t * client)
{
    uint8_t packet[2] = {0xC0u, 0x00u};

    if(client == NULL || client->sockfd < 0) {
        return -EINVAL;
    }

    if(send_all(client->sockfd, packet, sizeof(packet)) != 0) {
        return -EIO;
    }

    client->last_activity = time(NULL);
    return 0;
}

static int mqtt_send_publish(mqtt_client_t * client, const char * topic, const char * payload)
{
    uint8_t packet[2048];
    size_t pos = 0u;
    size_t topic_len;
    size_t payload_len;
    size_t remaining_len;
    uint8_t rem_len_buf[4];
    size_t rem_len_len;

    if(client == NULL || client->sockfd < 0 || topic == NULL || payload == NULL) {
        return -EINVAL;
    }

    topic_len     = strlen(topic);
    payload_len   = strlen(payload);
    remaining_len = 2u + topic_len + payload_len;
    if(remaining_len + 5u > sizeof(packet)) {
        return -EMSGSIZE;
    }

    packet[pos++] = 0x30u;
    rem_len_len   = encode_remaining_length(rem_len_buf, remaining_len);
    memcpy(packet + pos, rem_len_buf, rem_len_len);
    pos += rem_len_len;

    packet[pos++] = (uint8_t)((topic_len >> 8) & 0xffu);
    packet[pos++] = (uint8_t)(topic_len & 0xffu);
    memcpy(packet + pos, topic, topic_len);
    pos += topic_len;
    memcpy(packet + pos, payload, payload_len);
    pos += payload_len;

    if(send_all(client->sockfd, packet, pos) != 0) {
        return -EIO;
    }

    client->last_activity = time(NULL);
    return 0;
}

static void json_append_escaped(char * buf, size_t size, size_t * off, const char * text)
{
    const unsigned char * p = (const unsigned char *)(text != NULL ? text : "");

    if(buf == NULL || off == NULL || *off >= size) {
        return;
    }

    if(*off + 1u < size) {
        buf[(*off)++] = '"';
    }

    while(*p != '\0' && *off + 2u < size) {
        switch(*p) {
            case '\\':
                buf[(*off)++] = '\\';
                buf[(*off)++] = '\\';
                break;
            case '"':
                buf[(*off)++] = '\\';
                buf[(*off)++] = '"';
                break;
            case '\n':
                buf[(*off)++] = '\\';
                buf[(*off)++] = 'n';
                break;
            case '\r':
                buf[(*off)++] = '\\';
                buf[(*off)++] = 'r';
                break;
            case '\t':
                buf[(*off)++] = '\\';
                buf[(*off)++] = 't';
                break;
            default: buf[(*off)++] = (char)*p; break;
        }
        p++;
    }

    if(*off + 1u < size) {
        buf[(*off)++] = '"';
    }
    if(*off < size) {
        buf[*off] = '\0';
    }
}

static void build_sensor_payload(const sensor_data_t * data, char * buf, size_t size)
{
    if(buf == NULL || size == 0u || data == NULL) {
        return;
    }

    snprintf(buf, size,
             "{\"timestamp\":%lld,"
             "\"temperature\":%.3f,"
             "\"humidity\":%.3f,"
             "\"light_lux\":%.3f,"
             "\"mq2_raw_volt\":%.6f,"
             "\"mq2_real_ao_volt\":%.6f,"
             "\"flame_detected\":%d,"
             "\"pitch\":%.3f,"
             "\"roll\":%.3f,"
             "\"valid_mask\":%u}",
             (long long)data->timestamp, data->temperature, data->humidity, data->light_lux, data->mq2_raw_volt,
             data->mq2_real_ao_volt, data->flame_detected, data->pitch, data->roll, data->valid_mask);
}

static void build_alarm_payload(const alarm_event_t * event, char * buf, size_t size)
{
    size_t off = 0u;

    if(buf == NULL || size == 0u || event == NULL) {
        return;
    }

    snprintf(buf, size,
             "{\"timestamp\":%lld,"
             "\"level\":\"%s\","
             "\"source_mask\":%u,"
             "\"reason\":",
             (long long)event->timestamp, alarm_level_name(event->level), event->source_mask);
    off = strlen(buf);
    json_append_escaped(buf, size, &off, event->reason);
    snprintf(buf + off, size - off, ",\"image_path\":");
    off = strlen(buf);
    json_append_escaped(buf, size, &off, event->image_path);
    snprintf(buf + off, size - off,
             ",\"snapshot\":{"
             "\"temperature\":%.3f,"
             "\"humidity\":%.3f,"
             "\"light_lux\":%.3f,"
             "\"mq2_raw_volt\":%.6f,"
             "\"mq2_real_ao_volt\":%.6f,"
             "\"flame_detected\":%d,"
             "\"pitch\":%.3f,"
             "\"roll\":%.3f,"
             "\"valid_mask\":%u}}",
             event->snapshot.temperature, event->snapshot.humidity, event->snapshot.light_lux,
             event->snapshot.mq2_raw_volt, event->snapshot.mq2_real_ao_volt, event->snapshot.flame_detected,
             event->snapshot.pitch, event->snapshot.roll, event->snapshot.valid_mask);
}

static int mqtt_publish_sensor(mqtt_client_t * client, const sensor_data_t * data)
{
    char payload[512];

    build_sensor_payload(data, payload, sizeof(payload));
    return mqtt_send_publish(client, MQTT_SENSOR_TOPIC, payload);
}

static int mqtt_publish_alarm(mqtt_client_t * client, const alarm_event_t * event)
{
    char payload[768];

    build_alarm_payload(event, payload, sizeof(payload));
    return mqtt_send_publish(client, MQTT_ALARM_TOPIC, payload);
}

static int mqtt_ensure_connected(mqtt_client_t * client)
{
    int ret;

    if(client == NULL) {
        return -EINVAL;
    }

    if(client->sockfd >= 0) {
        return 0;
    }

    ret = mqtt_connect_socket(client);
    if(ret != 0) {
        return ret;
    }

    ret = mqtt_send_connect(client);
    if(ret != 0) {
        mqtt_disconnect(client);
        return ret;
    }

    LOG_INFO("mqtt connected to %s:%d", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    return 0;
}

void * mqtt_thread_entry(void * arg)
{
    app_context_t * ctx = (app_context_t *)arg;
    mqtt_client_t client;

    if(ctx == NULL || ctx->mqtt_mailbox == NULL) {
        return NULL;
    }

    memset(&client, 0, sizeof(client));
    client.sockfd = -1;

    LOG_INFO("mqtt_thread started");

    while(ctx->running) {
        app_msg_t msg;
        int ret;
        time_t now;

        ret = mailbox_recv_timeout(ctx->mqtt_mailbox, &msg, 1000);
        if(ret == -ETIMEDOUT) {
            now = time(NULL);
            if(client.sockfd >= 0 && now - client.last_activity >= (MQTT_KEEPALIVE_S / 2)) {
                if(mqtt_send_ping(&client) != 0) {
                    LOG_WARN("mqtt ping failed");
                    mqtt_disconnect(&client);
                }
            }
            continue;
        }
        if(ret != 0) {
            LOG_WARN("mqtt mailbox recv failed: %d", ret);
            continue;
        }
        if(msg.type == APP_MSG_EXIT) {
            break;
        }

        if(mqtt_ensure_connected(&client) != 0) {
            LOG_WARN("mqtt connect failed, drop message");
            mqtt_disconnect(&client);
            sleep(3);
            continue;
        }

        if(msg.type == APP_MSG_SENSOR_DATA) {
            if(mqtt_publish_sensor(&client, &msg.data.sensor) != 0) {
                LOG_WARN("mqtt publish sensor failed");
                mqtt_disconnect(&client);
            }
        } else if(msg.type == APP_MSG_ALARM_EVENT) {
            if(mqtt_publish_alarm(&client, &msg.data.alarm) != 0) {
                LOG_WARN("mqtt publish alarm failed");
                mqtt_disconnect(&client);
            }
        }

        client.last_activity = time(NULL);
    }

    mqtt_disconnect(&client);
    LOG_INFO("mqtt_thread stopped");
    return NULL;
}
