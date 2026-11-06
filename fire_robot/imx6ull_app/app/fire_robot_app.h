#ifndef FIRE_ROBOT_APP_H
#define FIRE_ROBOT_APP_H

int fire_robot_app_init(void);
int fire_robot_app_start(void);
void fire_robot_app_poll_ui(void);
int fire_robot_app_is_running(void);
void fire_robot_app_request_stop(void);
void fire_robot_app_stop(void);
void fire_robot_app_deinit(void);

#endif
