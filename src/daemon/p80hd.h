/* p80hd: the tablet-side half of p80h. Line protocol on TCP, see docs/daemon.md. */
#ifndef P80HD_H
#define P80HD_H

#define VERSION "0.2"
#define DEFAULT_ADDR "172.16.42.1" /* the USB-net address only: this is an unauthenticated root service */
#define DEFAULT_PORT 4444
#define MAX_CONN 16

/* every handler gets the connected socket and the rest of the header line */
void cmd_exec(int fd, const char *arg);
void cmd_shell(int fd, const char *arg);
void cmd_put(int fd, const char *arg);
void cmd_get(int fd, const char *arg);
void cmd_reboot(int fd, const char *arg);
void cmd_inject(int fd, const char *arg);
void cmd_status(int fd, const char *arg);

void progress(const char *label, int pm);

void exec_init(void);
void enter_root(void);

int listen_on(const char *addr, int port);
void tune(int fd);
int read_line(int fd, char *line, int max, int secs);
int say(int fd, const char *fmt, ...);
int read_full(int fd, void *buf, long n);
int write_full(int fd, const void *buf, long n);
long slurp(const char *path, char *buf, long max);

#endif
