#ifndef _RAIN_LOG_H
#define _RAIN_LOG_H

#define logger(l, fmt, ...) rain_log(&rain_srv, __FILE__, __LINE__, l, fmt, ##__VA_ARGS__)

void rain_log(rain_t * srv, const char * file, const int line, int l, const char * fmt,...);
#endif
