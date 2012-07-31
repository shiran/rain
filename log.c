#include<stdio.h>
#include<time.h>
#include<string.h>
#include<unistd.h>
#include<stdarg.h>

#include"common.h"
#include"rain.h"
#include"log.h"

void rain_log(rain_t * srv, const char * file, const int line, int l, const char * fmt, ...) {
    time_t t;
    char log[MAX_LINE_LEN], msg[64];
    FILE * fp;
    va_list ap;
    const char *level[] = {"DEBUG", "NOTICE", "WARNING", "ERROR"};

    if (l < srv->log_level) {
        return ;
    }

    fp = (srv->log_file == NULL) ? stdout : fopen(srv->log_file, "a+");

    va_start(ap, fmt);
    vsnprintf(log, sizeof(log), fmt, ap);
    va_end(ap);

    time(&t);
    strftime(msg, sizeof(msg), "%H:%M:%S", localtime(&t));
    fprintf(fp, "[%s %s] %s LINE: %d %s\n", level[l], msg, file, line, log);
    fflush(fp);

    if (srv->log_file) fclose(fp);
}
