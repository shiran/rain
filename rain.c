#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<netdb.h>
#include<pwd.h>
#include<grp.h>
#include<signal.h>
#include<fcntl.h>
#include<pthread.h>
#include<errno.h>

#include"common.h"
#include"rain.h"
#include"log.h"
#include"net.h"


int main(int argc, char * argv[]) {

    char ** cgi_argv = {NULL};
    struct timeval tv = {0, 100 * 1000};

    // init rain_srv
    init_server();

    // parse option
    if(RAIN_ERROR == parse_opt(argc, argv)) {
        return RAIN_ERROR;
    }

    // start as server
    if (start_server() != RAIN_OK) {
        return RAIN_ERROR;
    }

    worker_dispatch(rain_srv.child_num, rain_srv.app, cgi_argv);

    monitor();

    for(;;) {
        if (rain_stop) {
            logger(DEBUG, "rain server is stop now...");
            break;
        }
        select(0, NULL, NULL, NULL, &tv);
    }

    return 0;
}

int parse_opt(int argc, char * argv[]) {

    int res = 0, is_root = 0;
    char * endptr = NULL, *uname = NULL, *ugrp = NULL;
    int port = 0;
    struct passwd * pwd;
    struct group * grp;

    opterr = 0;
    while((res = getopt(argc, argv, "a:p:u:g:n:l:f:d")) != -1) {
        switch (res) {
            case 'a' :
                rain_srv.addr = strdup((char *) optarg);
                break;
            case 'p' :
                port = strtol(optarg, &endptr, 10);
                if (*endptr) {
                    logger(ERROR, "rain: invalid port: %u\n", (unsigned int) port);
                    return RAIN_ERROR;
                }
                rain_srv.port = port;
                break;
            case 'n' :
                rain_srv.child_num = atoi(optarg);
                if (rain_srv.child_num < 1) {
                    logger(WARNING, "child num must be greater than one");
                }
                rain_srv.pid_list = malloc(sizeof(pid_t) * rain_srv.child_num);
                break;
            case 'l' : rain_srv.log_file = strdup((char * ) optarg); break;
            case 'u' : uname = (char *) optarg; break;
            case 'g' : ugrp = (char *) optarg; break;
            case 'd' : rain_srv.is_debug = 1; break;
            case 'f' : rain_srv.app = strdup((char *) optarg); break;
            case '?' :
            case 'h' :
            default :
                rain_help();
                return RAIN_OK;
        }
    }

    // daemon
    if (!rain_srv.is_debug) {
        if (fork()){
            exit(0);
        }
    }

    is_root = (getuid() == 0);
    rain_srv.is_root = is_root;

    if (is_root && (uname || ugrp)) {

        if (uname) {
            pwd = getpwnam(uname);
            if (pwd) {
                rain_srv.uid = pwd->pw_uid;
            }
        }

        if (ugrp) {

            grp = getgrnam(ugrp);
            if (grp) {
                rain_srv.gid = grp->gr_gid;
            }
        }
    }

    // test pid file
    if (rain_srv.pid_file && !rain_srv.is_debug) {
        int pid_fd ;
        if (-1 == (pid_fd = open(rain_srv.pid_file, O_CREAT|O_WRONLY|O_TRUNC, 0755))) {
            logger(WARNING, "open pid file error: %s", rain_srv.pid_file);
        } else {
            close(pid_fd);
        }
    }

    return RAIN_OK;
}

int rain_help() {
    fprintf(stderr, "Usage: rain [options]\n" \
            "-a <address> bind to address\n" \
            "-p bind to tcp port\n" \
            "-f filename of application\n" \
            "-n child process number\n" \
            "-? or -h show this help\n" \
            );
    return RAIN_OK;
}

void init_server() {

    rain_srv.port = 9000;
    rain_srv.child_num = 1;
    rain_srv.log_level = -1;
    rain_srv.sock_fd = -1;
    rain_srv.gid = 0;
    rain_srv.uid = 0;
    rain_srv.addr = "127.0.0.1";
    rain_srv.log_file = NULL;
    rain_srv.pid_file = "/tmp/rain.pid";
    rain_srv.is_debug = 0;

}

int worker_dispatch(int num, char * app, char ** cgi_argv) {

    int i, pid_fd, child = 1;
    pid_t pid = 0;
    char pid_buf[32];

    pid_fd = open(rain_srv.pid_file, O_WRONLY|O_APPEND|O_CREAT, 0755);
    if (pid_fd == -1) {
        logger(WARNING, "open pid file error: %s", rain_srv.pid_file);
    }

    // debug mode force one process
    if (rain_srv.is_debug) {
        num = 1;
        child = 0;
    }

    for(i = 0; i < num; i++) {

        if (!rain_srv.is_debug) {
            pid = fork();
        }

        switch (pid) {
            case 0 :

                process_call(child, app, cgi_argv);
                return RAIN_ERROR;
            case -1:

                logger(ERROR, "fork child process error.");
                return RAIN_ERROR;
            default :

                if (pid_fd != -1) {
                    snprintf(pid_buf, sizeof(pid_buf), "%d\n", pid);
                    write(pid_fd, pid_buf, strlen(pid_buf));
                }
                * (rain_srv.pid_list + i) = pid;
        }
    }

    close(pid_fd);

    return RAIN_OK;
}

void sign_fpm(int signo) {

    pid_t pid, i=0;
    char ** cgi_env = {NULL};

    switch (signo) {

        case SIGCHLD :
            pid = wait(NULL);
#ifdef _DEBUG
            logger(WARNING, "pid : %d gone away.", pid);
#endif

            for (i = 0; i < rain_srv.child_num; i++) {
                if (*(rain_srv.pid_list + i) == pid) {
                    logger(DEBUG, "child %d is start...", i + 1);
                    worker_dispatch(1, rain_srv.app, cgi_env);
                    break;
                }
            }
            break;
        case SIGTERM :
            srv_stop();
            break;
    }
}

void srv_stop() {

    FILE * fp = NULL;
    char buf[125] ;
    int res;
    pid_t pid;
    struct sigaction act;

    act.sa_flags = SA_NODEFER;
    act.sa_handler = SIG_IGN;
    // ignore child exit
    sigaction(SIGCHLD, &act, NULL);

    fp = fopen(rain_srv.pid_file, "r");
    if (fp) {
        while (fgets(buf, 64, fp) != NULL) {
            pid = atol(buf);

            if (pid > 0) {
                res = kill(pid, SIGKILL);

                if (res == -1) {
                    logger(ERROR, "kill child %d error.", pid);
                }
            }
        }

        fclose(fp);
    }

    close(rain_srv.sock_fd);
    unlink(rain_srv.pid_file);

    // stop server
    rain_stop = 1;
}

int process_call(int child, char * app, char * * cgi_argv) {

    int i = 3, max_fd = -1;
    char path[256];

    if (rain_srv.sock_fd != FCGI_LISTENSOCK_FILENO) {
        close(FCGI_LISTENSOCK_FILENO);
        dup2(rain_srv.sock_fd, FCGI_LISTENSOCK_FILENO);
        close(rain_srv.sock_fd);
    }

    if (child) {

        setsid();
        max_fd = open("/dev/null", O_RDWR);

        if (max_fd != -1) {
            // reset  stdout&stderr
            if (max_fd != STDOUT_FILENO) dup2(max_fd, STDOUT_FILENO);
            if (max_fd != STDERR_FILENO) dup2(max_fd, STDERR_FILENO);
            if (max_fd != STDOUT_FILENO && max_fd != STDERR_FILENO) close(max_fd);
        } else {
            logger(WARNING, "open /dev/null error.");
        }
    }

    // reset client socket
    for (i = 3; i < max_fd; i++) {
        if (i != FCGI_LISTENSOCK_FILENO) close(i);
    }

    if (rain_srv.is_root) {
        if (rain_srv.uid) {
            setuid(rain_srv.uid);
        }

        if (rain_srv.gid) {
            setgid(rain_srv.gid);
        }
    }

    snprintf(path, sizeof(path), "%s", app);
#ifdef _DEBUG
    logger(DEBUG, "app : %s", path);
#endif
    execl("/bin/sh", "sh", "-c", path, (char * )NULL);
    return RAIN_ERROR;
}

void monitor() {

    struct sigaction act;

    act.sa_flags = SA_RESETHAND|SA_NODEFER;
    act.sa_handler = sign_fpm;
    sigaction(SIGCHLD, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}
