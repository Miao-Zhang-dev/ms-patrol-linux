#include "../config/app_config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include </home/linux/nfs/rootfs/usr/include/sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef WEB_LISTEN_PORT
#define WEB_LISTEN_PORT 8080
#endif

#ifndef WEB_QUERY_LIMIT
#define WEB_QUERY_LIMIT 20
#endif

#ifndef DB_PATH
#define DB_PATH "/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/fire_robot/fire_robot.db"
#endif

#ifndef ALARM_IMAGE_DIR
#define ALARM_IMAGE_DIR "/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/fire_robot/alarm_images"
#endif

#define HTTP_BUF_SIZE 4096
#define JSON_BUF_SIZE 65536

static volatile sig_atomic_t g_stop;

static const char * INDEX_HTML =
    "<!doctype html>\n"
    "<html lang=\"zh-CN\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "<title>Fire Robot Monitor</title>\n"
    "<style>\n"
    ":root{--bg:#eef1f4;--panel:#ffffff;--text:#1d252c;--muted:#697783;--line:#d8dee5;--red:#c93636;--amber:#bd7b16;--"
    "green:#178060;--blue:#2e6f9e;}\n"
    "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Arial,'Microsoft "
    "YaHei',sans-serif;font-size:14px;}\n"
    ".shell{max-width:1180px;margin:0 "
    "auto;padding:20px}.top{display:flex;align-items:flex-end;justify-content:space-between;gap:16px;margin-bottom:"
    "16px;border-bottom:3px solid var(--text);padding-bottom:12px}\n"
    "h1{font-size:28px;line-height:1;margin:0;font-weight:800}.sub{color:var(--muted);margin-top:6px}.badge{border:1px "
    "solid var(--line);background:var(--panel);padding:8px "
    "12px;font-weight:700}.grid{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px}\n"
    ".metric{background:var(--panel);border:1px solid var(--line);padding:14px 12px;min-height:86px}.metric "
    "label{display:block;color:var(--muted);font-size:12px;margin-bottom:10px}.metric "
    "strong{font-size:24px}.metric.danger{border-color:var(--red);box-shadow:inset 4px 0 0 var(--red)}\n"
    ".section{background:var(--panel);border:1px solid var(--line);margin-bottom:16px}.section "
    "h2{font-size:17px;margin:0;padding:12px 14px;border-bottom:1px solid "
    "var(--line)}.table-wrap{overflow:auto}table{width:100%;border-collapse:collapse;min-width:860px}th,td{text-align:"
    "left;border-bottom:1px solid var(--line);padding:9px "
    "10px;white-space:nowrap}th{font-size:12px;color:var(--muted);background:#f8fafb}td.path{max-width:360px;overflow:"
    "hidden;text-overflow:ellipsis}.level-warn{color:var(--amber);font-weight:700}.level-danger{color:var(--red);font-"
    "weight:800}.level-normal{color:var(--green);font-weight:700}a{color:var(--blue);text-decoration:none}a:hover{text-"
    "decoration:underline}.empty{padding:18px;color:var(--muted)}\n"
    "@media(max-width:860px){.shell{padding:12px}.top{align-items:flex-start;flex-direction:column}.grid{grid-template-"
    "columns:repeat(2,1fr)}.metric strong{font-size:20px}h1{font-size:24px}}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<main class=\"shell\">\n"
    "<header class=\"top\"><div><h1>Fire Robot Monitor</h1><div class=\"sub\">i.MX6ULL local web panel</div></div><div "
    "class=\"badge\" id=\"status\">Loading</div></header>\n"
    "<section class=\"grid\">\n"
    "<div class=\"metric\"><label>Temperature</label><strong id=\"temperature\">-- C</strong></div>\n"
    "<div class=\"metric\"><label>Humidity</label><strong id=\"humidity\">-- %</strong></div>\n"
    "<div class=\"metric\"><label>Light</label><strong id=\"light\">-- lx</strong></div>\n"
    "<div class=\"metric\"><label>Smoke</label><strong id=\"smoke\">-- V</strong></div>\n"
    "<div class=\"metric\" id=\"flameBox\"><label>Flame</label><strong id=\"flame\">--</strong></div>\n"
    "</section>\n"
    "<section class=\"section\"><h2>Alarm log</h2><div "
    "class=\"table-wrap\"><table><thead><tr><th>ID</th><th>Time</th><th>Level</th><th>Source</th><th>Reason</"
    "th><th>Image</th></tr></thead><tbody id=\"alarmRows\"></tbody></table></div></section>\n"
    "<section class=\"section\"><h2>Sensor history</h2><div "
    "class=\"table-wrap\"><table><thead><tr><th>ID</th><th>Time</th><th>Temp</th><th>Humi</th><th>Light</th><th>Smoke</"
    "th><th>Flame</th><th>Mask</th></tr></thead><tbody id=\"sensorRows\"></tbody></table></div></section>\n"
    "</main>\n"
    "<script>\n"
    "function qs(id){return document.getElementById(id)}\n"
    "function text(v,s){return v===null||v===undefined?'--':Number(v).toFixed(s)}\n"
    "function level(v){if(v===2)return '<span class=\"level-danger\">Danger</span>';if(v===1)return '<span "
    "class=\"level-warn\">Warning</span>';return '<span class=\"level-normal\">Normal</span>'}\n"
    "function time(t){if(!t)return '--';var d=new Date(t*1000);return d.toLocaleString()}\n"
    "function esc(s){return String(s||'').replace(/[&<>\"']/g,function(c){return "
    "{'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c]})}\n"
    "async function getJson(url){var r=await fetch(url,{cache:'no-store'});if(!r.ok)throw new Error(url);return "
    "r.json()}\n"
    "async function refresh(){try{var latest=await "
    "getJson('/api/latest');if(latest&&latest.id){qs('temperature').textContent=text(latest.temperature,1)+' "
    "C';qs('humidity').textContent=text(latest.humidity,1)+' %';qs('light').textContent=text(latest.light_lux,1)+' "
    "lx';qs('smoke').textContent=text(latest.mq2_real_ao_volt,3)+' "
    "V';qs('flame').textContent=latest.flame_detected?'FIRE':'NORMAL';qs('flameBox').className='metric'+(latest.flame_"
    "detected?' danger':'');}\n"
    "var alarms=await "
    "getJson('/api/alarms?limit=20');qs('alarmRows').innerHTML=alarms.length?alarms.map(function(a){var "
    "img=a.image_url?'<a href=\"'+esc(a.image_url)+'\" target=\"_blank\">Open</a>':'--';return "
    "'<tr><td>'+a.id+'</td><td>'+time(a.timestamp)+'</td><td>'+level(a.level)+'</td><td>'+a.source_mask+'</"
    "td><td>'+esc(a.reason)+'</td><td class=\"path\">'+img+' '+esc(a.image_path||'')+'</td></tr>'}).join(''):'<tr><td "
    "colspan=\"6\" class=\"empty\">No alarm records</td></tr>';\n"
    "var sensors=await "
    "getJson('/api/sensors?limit=20');qs('sensorRows').innerHTML=sensors.length?sensors.map(function(s){return "
    "'<tr><td>'+s.id+'</td><td>'+time(s.timestamp)+'</td><td>'+text(s.temperature,1)+'</td><td>'+text(s.humidity,1)+'</"
    "td><td>'+text(s.light_lux,1)+'</td><td>'+text(s.mq2_real_ao_volt,3)+'</"
    "td><td>'+(s.flame_detected?'FIRE':'NORMAL')+'</td><td>'+s.valid_mask+'</td></tr>'}).join(''):'<tr><td "
    "colspan=\"8\" class=\"empty\">No sensor records</td></tr>';\n"
    "qs('status').textContent='Online';}catch(e){qs('status').textContent='Database offline'}}\n"
    "refresh();setInterval(refresh,3000);\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

static void signal_handler(int signo)
{
    (void)signo;
    g_stop = 1;
}

static int send_all(int fd, const void * buf, size_t len)
{
    const char * p = (const char *)buf;

    while(len > 0u) {
        ssize_t ret = send(fd, p, len, 0);
        if(ret < 0) {
            if(errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += ret;
        len -= (size_t)ret;
    }

    return 0;
}

static void send_response(int client, const char * status, const char * type, const void * body, size_t body_len)
{
    char header[256];
    int n;

    n = snprintf(header, sizeof(header),
                 "HTTP/1.1 %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %u\r\n"
                 "Connection: close\r\n"
                 "Cache-Control: no-store\r\n"
                 "\r\n",
                 status, type, (unsigned int)body_len);

    send_all(client, header, (size_t)n);
    send_all(client, body, body_len);
}

static void send_text(int client, const char * status, const char * text)
{
    send_response(client, status, "text/plain; charset=utf-8", text, strlen(text));
}

static void json_append(char * buf, size_t size, size_t * off, const char * fmt, ...)
{
    va_list ap;
    int n;

    if(buf == NULL || off == NULL || *off >= size) {
        return;
    }

    va_start(ap, fmt);
    n = vsnprintf(buf + *off, size - *off, fmt, ap);
    va_end(ap);

    if(n < 0) {
        return;
    }

    if((size_t)n >= size - *off) {
        *off = size - 1u;
    } else {
        *off += (size_t)n;
    }
}

static void json_append_string(char * buf, size_t size, size_t * off, const char * s)
{
    const unsigned char * p = (const unsigned char *)(s != NULL ? s : "");

    json_append(buf, size, off, "\"");
    while(*p != '\0') {
        switch(*p) {
            case '\\': json_append(buf, size, off, "\\\\"); break;
            case '"': json_append(buf, size, off, "\\\""); break;
            case '\n': json_append(buf, size, off, "\\n"); break;
            case '\r': json_append(buf, size, off, "\\r"); break;
            case '\t': json_append(buf, size, off, "\\t"); break;
            default:
                if(*p < 32u) {
                    json_append(buf, size, off, "\\u%04x", *p);
                } else {
                    json_append(buf, size, off, "%c", *p);
                }
                break;
        }
        p++;
    }
    json_append(buf, size, off, "\"");
}

static int db_open_readonly(sqlite3 ** db)
{
    int ret;

    ret = sqlite3_open_v2(DB_PATH, db, SQLITE_OPEN_READONLY, NULL);
    if(ret != SQLITE_OK) {
        if(*db != NULL) {
            sqlite3_close(*db);
            *db = NULL;
        }
        return -1;
    }

    sqlite3_busy_timeout(*db, 1000);
    return 0;
}

static int parse_limit(const char * path)
{
    const char * p = strstr(path, "limit=");
    int limit;

    if(p == NULL) {
        return WEB_QUERY_LIMIT;
    }

    limit = atoi(p + 6);
    if(limit <= 0) {
        return WEB_QUERY_LIMIT;
    }
    if(limit > 100) {
        return 100;
    }

    return limit;
}

static const char * basename_const(const char * path)
{
    const char * p;

    if(path == NULL) {
        return "";
    }

    p = strrchr(path, '/');
    return p != NULL ? p + 1 : path;
}

static void send_latest_sensor(int client)
{
    static const char * sql = "SELECT id,timestamp,temperature,humidity,light_lux,"
                              "mq2_real_ao_volt,flame_detected,valid_mask "
                              "FROM sensor_log ORDER BY id DESC LIMIT 1;";
    sqlite3 * db        = NULL;
    sqlite3_stmt * stmt = NULL;
    char json[1024];
    size_t off = 0u;
    int ret;

    if(db_open_readonly(&db) != 0) {
        send_text(client, "503 Service Unavailable", "database open failed\n");
        return;
    }

    ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(ret != SQLITE_OK) {
        sqlite3_close(db);
        send_text(client, "500 Internal Server Error", "query prepare failed\n");
        return;
    }

    if(sqlite3_step(stmt) == SQLITE_ROW) {
        json_append(json, sizeof(json), &off,
                    "{\"id\":%d,\"timestamp\":%lld,"
                    "\"temperature\":%.3f,\"humidity\":%.3f,"
                    "\"light_lux\":%.3f,\"mq2_real_ao_volt\":%.6f,"
                    "\"flame_detected\":%d,\"valid_mask\":%d}",
                    sqlite3_column_int(stmt, 0), (long long)sqlite3_column_int64(stmt, 1),
                    sqlite3_column_double(stmt, 2), sqlite3_column_double(stmt, 3), sqlite3_column_double(stmt, 4),
                    sqlite3_column_double(stmt, 5), sqlite3_column_int(stmt, 6), sqlite3_column_int(stmt, 7));
    } else {
        json_append(json, sizeof(json), &off, "{}");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    send_response(client, "200 OK", "application/json; charset=utf-8", json, off);
}

static void send_sensor_list(int client, const char * path)
{
    static const char * sql = "SELECT id,timestamp,temperature,humidity,light_lux,"
                              "mq2_real_ao_volt,flame_detected,valid_mask "
                              "FROM sensor_log ORDER BY id DESC LIMIT ?;";
    sqlite3 * db        = NULL;
    sqlite3_stmt * stmt = NULL;
    char json[JSON_BUF_SIZE];
    size_t off = 0u;
    int first  = 1;

    if(db_open_readonly(&db) != 0) {
        send_text(client, "503 Service Unavailable", "database open failed\n");
        return;
    }

    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        send_text(client, "500 Internal Server Error", "query prepare failed\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, parse_limit(path));
    json_append(json, sizeof(json), &off, "[");
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        if(!first) {
            json_append(json, sizeof(json), &off, ",");
        }
        first = 0;
        json_append(json, sizeof(json), &off,
                    "{\"id\":%d,\"timestamp\":%lld,"
                    "\"temperature\":%.3f,\"humidity\":%.3f,"
                    "\"light_lux\":%.3f,\"mq2_real_ao_volt\":%.6f,"
                    "\"flame_detected\":%d,\"valid_mask\":%d}",
                    sqlite3_column_int(stmt, 0), (long long)sqlite3_column_int64(stmt, 1),
                    sqlite3_column_double(stmt, 2), sqlite3_column_double(stmt, 3), sqlite3_column_double(stmt, 4),
                    sqlite3_column_double(stmt, 5), sqlite3_column_int(stmt, 6), sqlite3_column_int(stmt, 7));
    }
    json_append(json, sizeof(json), &off, "]");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    send_response(client, "200 OK", "application/json; charset=utf-8", json, off);
}

static void send_alarm_list(int client, const char * path)
{
    static const char * sql = "SELECT id,timestamp,level,source_mask,reason,image_path "
                              "FROM alarm_log ORDER BY id DESC LIMIT ?;";
    sqlite3 * db        = NULL;
    sqlite3_stmt * stmt = NULL;
    char json[JSON_BUF_SIZE];
    size_t off = 0u;
    int first  = 1;

    if(db_open_readonly(&db) != 0) {
        send_text(client, "503 Service Unavailable", "database open failed\n");
        return;
    }

    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        send_text(client, "500 Internal Server Error", "query prepare failed\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, parse_limit(path));
    json_append(json, sizeof(json), &off, "[");
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char * reason     = (const char *)sqlite3_column_text(stmt, 4);
        const char * image_path = (const char *)sqlite3_column_text(stmt, 5);
        const char * image_name = basename_const(image_path);

        if(!first) {
            json_append(json, sizeof(json), &off, ",");
        }
        first = 0;

        json_append(json, sizeof(json), &off,
                    "{\"id\":%d,\"timestamp\":%lld,\"level\":%d,"
                    "\"source_mask\":%d,\"reason\":",
                    sqlite3_column_int(stmt, 0), (long long)sqlite3_column_int64(stmt, 1), sqlite3_column_int(stmt, 2),
                    sqlite3_column_int(stmt, 3));
        json_append_string(json, sizeof(json), &off, reason);
        json_append(json, sizeof(json), &off, ",\"image_path\":");
        json_append_string(json, sizeof(json), &off, image_path);
        json_append(json, sizeof(json), &off, ",\"image_url\":");
        if(image_path != NULL && image_path[0] != '\0') {
            char image_url[256];
            snprintf(image_url, sizeof(image_url), "/alarm_images/%s", image_name);
            json_append_string(json, sizeof(json), &off, image_url);
        } else {
            json_append_string(json, sizeof(json), &off, "");
        }
        json_append(json, sizeof(json), &off, "}");
    }
    json_append(json, sizeof(json), &off, "]");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    send_response(client, "200 OK", "application/json; charset=utf-8", json, off);
}

static int is_safe_image_name(const char * name)
{
    const char * p;

    if(name == NULL || name[0] == '\0') {
        return 0;
    }
    if(strstr(name, "..") != NULL || strchr(name, '/') != NULL) {
        return 0;
    }

    for(p = name; *p != '\0'; p++) {
        if(!(('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z') || ('0' <= *p && *p <= '9') || *p == '_' ||
             *p == '-' || *p == '.')) {
            return 0;
        }
    }

    return 1;
}

static void send_alarm_image(int client, const char * name)
{
    char path[512];
    FILE * fp;
    struct stat st;
    char * buf;

    if(!is_safe_image_name(name)) {
        send_text(client, "400 Bad Request", "bad image name\n");
        return;
    }

    snprintf(path, sizeof(path), "%s/%s", ALARM_IMAGE_DIR, name);
    if(stat(path, &st) != 0 || st.st_size <= 0) {
        send_text(client, "404 Not Found", "image not found\n");
        return;
    }

    fp = fopen(path, "rb");
    if(fp == NULL) {
        send_text(client, "404 Not Found", "image open failed\n");
        return;
    }

    buf = (char *)malloc((size_t)st.st_size);
    if(buf == NULL) {
        fclose(fp);
        send_text(client, "500 Internal Server Error", "no memory\n");
        return;
    }

    if(fread(buf, 1u, (size_t)st.st_size, fp) != (size_t)st.st_size) {
        free(buf);
        fclose(fp);
        send_text(client, "500 Internal Server Error", "image read failed\n");
        return;
    }

    fclose(fp);
    send_response(client, "200 OK", "image/x-portable-pixmap", buf, (size_t)st.st_size);
    free(buf);
}

static void handle_client(int client)
{
    char req[HTTP_BUF_SIZE];
    char method[16];
    char path[512];
    ssize_t n;

    n = recv(client, req, sizeof(req) - 1u, 0);
    if(n <= 0) {
        return;
    }
    req[n] = '\0';

    if(sscanf(req, "%15s %511s", method, path) != 2) {
        send_text(client, "400 Bad Request", "bad request\n");
        return;
    }

    if(strcmp(method, "GET") != 0) {
        send_text(client, "405 Method Not Allowed", "only GET supported\n");
        return;
    }

    if(strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        send_response(client, "200 OK", "text/html; charset=utf-8", INDEX_HTML, strlen(INDEX_HTML));
    } else if(strncmp(path, "/api/latest", 11) == 0) {
        send_latest_sensor(client);
    } else if(strncmp(path, "/api/sensors", 12) == 0) {
        send_sensor_list(client, path);
    } else if(strncmp(path, "/api/alarms", 11) == 0) {
        send_alarm_list(client, path);
    } else if(strncmp(path, "/alarm_images/", 14) == 0) {
        send_alarm_image(client, path + 14);
    } else {
        send_text(client, "404 Not Found", "not found\n");
    }
}

int main(void)
{
    int server_fd;
    int opt = 1;
    struct sockaddr_in addr;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        perror("socket");
        return 1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(WEB_LISTEN_PORT);

    if(bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if(listen(server_fd, 8) != 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("fire_robot_web listening on port %d\n", WEB_LISTEN_PORT);

    while(!g_stop) {
        int client = accept(server_fd, NULL, NULL);
        if(client < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        handle_client(client);
        close(client);
    }

    close(server_fd);
    return 0;
}
