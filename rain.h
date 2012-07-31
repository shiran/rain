#ifndef _RAIN_H
#define _RAIN_H

typedef struct rain {
    int sock_fd;
    int child_num;
    int log_level;
    int is_debug;
    int is_root;
    unsigned int port;
    char * log_file;
    char * pid_file;
    char * addr;
    char * app;
    uid_t uid;
    gid_t gid;
    pid_t *pid_list;
} rain_t;

rain_t rain_srv;
int rain_stop;

int parse_opt(int argc, char * argv[]);
void init_server();
void sign_fpm(int signo);
int worker_dispatch(int num, char * app, char ** argv);
int rain_help();
void monitor();
void srv_stop();
int process_call(int child, char * app, char * * cgi_argv);
#endif
