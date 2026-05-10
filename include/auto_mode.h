#ifndef AUTO_MODE_H
#define AUTO_MODE_H

void auto_mode_init(void);
void auto_mode_task(void);
void auto_mode_request_path(char cmd);
void auto_mode_set_path4(unsigned char *src, unsigned char len);

#endif