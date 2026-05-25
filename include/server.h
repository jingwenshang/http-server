#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


#define DEFAULT_PORT          8888
#define BACKLOG               128
#define NUM_WORKERS           64
#define QUEUE_CAPACITY        1024
#define BUF_SIZE              8192
#define MAX_HEADER_SIZE       4096
#define MAX_PATH_LEN          512
#define DEFAULT_KEEPALIVE_SEC 30
#define KEEPALIVE_THRESHOLD   32
#define DEFAULT_DOC_ROOT      "./docroot"


typedef enum {
    HTTP_10,
    HTTP_11
} http_version_t;


typedef struct {
    char method[16];
    char uri[MAX_PATH_LEN];
    http_version_t version;
    int  keep_alive;             
} http_request_t;


typedef struct {
    int  fds[QUEUE_CAPACITY];
    int  head;
    int  tail;
    int  count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} task_queue_t;


typedef struct {
    int             listen_fd;
    char            doc_root[MAX_PATH_LEN];
    int             port;
    int             num_workers;
    task_queue_t    queue;
    volatile int    active_connections; 
    volatile int    running;       
} server_t;


void  queue_init(task_queue_t *q);
void  queue_push(task_queue_t *q, int fd);
int   queue_pop(task_queue_t *q);
void *worker_main(void *arg);


int   parse_request(const char *raw, http_request_t *req);
void  handle_connection(server_t *srv, int client_fd);
void  send_response(int fd, int status, const char *content_type,
                    const char *body, size_t body_len);
void  send_file_response(int fd, int status, const char *content_type,
                         const char *filepath, size_t file_size);
void  send_error(int fd, int status);
const char *get_mime_type(const char *path);
int   compute_keepalive_timeout(server_t *srv);


void  set_nonblocking(int fd);
void  get_date_string(char *buf, size_t len);

#endif
