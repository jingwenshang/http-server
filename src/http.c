#include "server.h"
#include <sys/select.h>



typedef struct {
    const char *ext;
    const char *mime;
} mime_entry_t;

static const mime_entry_t MIME_TABLE[] = {
    { ".html", "text/html"       },
    { ".htm",  "text/html"       },
    { ".txt",  "text/plain"      },
    { ".css",  "text/css"        },
    { ".js",   "application/javascript" },
    { ".jpg",  "image/jpeg"      },
    { ".jpeg", "image/jpeg"      },
    { ".gif",  "image/gif"       },
    { ".png",  "image/png"       },
    { ".ico",  "image/x-icon"    },
    { NULL, NULL }
};

const char *get_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (dot) {
        for (int i = 0; MIME_TABLE[i].ext != NULL; i++) {
            if (strcasecmp(dot, MIME_TABLE[i].ext) == 0) {
                return MIME_TABLE[i].mime;
            }
        }
    }
    return "application/octet-stream";
}



void get_date_string(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}



int compute_keepalive_timeout(server_t *srv) {
    int active = srv->active_connections;
    int timeout = DEFAULT_KEEPALIVE_SEC / (1 + active / KEEPALIVE_THRESHOLD);
    if (timeout < 1) timeout = 1;
    return timeout;
}



int parse_request(const char *raw, http_request_t *req) {
    memset(req, 0, sizeof(*req));

   
    char version_str[16] = {0};
    int n = sscanf(raw, "%15s %511s %15s", req->method, req->uri, version_str);
    if (n < 2) {
        return -1; 
    }

    
    if (n < 3 || strncmp(version_str, "HTTP/1.0", 8) == 0) {
        req->version = HTTP_10;
        req->keep_alive = 0;
    } else if (strncmp(version_str, "HTTP/1.1", 8) == 0) {
        req->version = HTTP_11;
        req->keep_alive = 1;
    } else {
        req->version = HTTP_10;
        req->keep_alive = 0;
    }

    
    if (strcasecmp(req->method, "GET") != 0) {
        return -1;
    }

   
    const char *line = raw;
    while ((line = strstr(line, "\r\n")) != NULL) {
        line += 2;
        if (*line == '\r' || *line == '\0') break; 

        if (strncasecmp(line, "Connection:", 11) == 0) {
            const char *val = line + 11;
            while (*val == ' ') val++;
            if (strncasecmp(val, "close", 5) == 0) {
                req->keep_alive = 0;
            } else if (strncasecmp(val, "keep-alive", 10) == 0) {
                req->keep_alive = 1;
            }
        }
    }

    
    if (strcmp(req->uri, "/") == 0) {
        strcpy(req->uri, "/index.html");
    }

    return 0;
}


void send_error(int fd, int status) {
    const char *reason;
    const char *body;

    switch (status) {
    case 400:
        reason = "Bad Request";
        body   = "<html><body><h1>400 Bad Request</h1></body></html>";
        break;
    case 403:
        reason = "Forbidden";
        body   = "<html><body><h1>403 Forbidden</h1></body></html>";
        break;
    case 404:
        reason = "Not Found";
        body   = "<html><body><h1>404 Not Found</h1></body></html>";
        break;
    default:
        reason = "Internal Server Error";
        body   = "<html><body><h1>500 Internal Server Error</h1></body></html>";
        status = 500;
        break;
    }

    send_response(fd, status, "text/html", body, strlen(body));
}



void send_response(int fd, int status, const char *content_type,
                   const char *body, size_t body_len) {
    char header[MAX_HEADER_SIZE];
    char date[64];
    const char *reason;

    get_date_string(date, sizeof(date));

    switch (status) {
    case 200: reason = "OK";                    break;
    case 400: reason = "Bad Request";           break;
    case 403: reason = "Forbidden";             break;
    case 404: reason = "Not Found";             break;
    default:  reason = "Internal Server Error"; break;
    }

    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Date: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, reason, date, content_type, body_len);

    write(fd, header, hlen);
    if (body && body_len > 0) {
        write(fd, body, body_len);
    }
}



void send_file_response(int fd, int status, const char *content_type,
                        const char *filepath, size_t file_size) {
    char header[MAX_HEADER_SIZE];
    char date[64];

    get_date_string(date, sizeof(date));

    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Date: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        date, content_type, file_size);

    write(fd, header, hlen);

   
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) return;

    char buf[BUF_SIZE];
    ssize_t nread;
    while ((nread = read(file_fd, buf, sizeof(buf))) > 0) {
        ssize_t total_sent = 0;
        while (total_sent < nread) {
            ssize_t nsent = write(fd, buf + total_sent, nread - total_sent);
            if (nsent <= 0) {
                close(file_fd);
                return;
            }
            total_sent += nsent;
        }
    }

    close(file_fd);
}



static int read_request(int fd, char *buf, size_t buf_size, int timeout_sec) {
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec  = timeout_sec;
    tv.tv_usec = 0;

    int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ret <= 0) {
        return -1; 
    }

    int total = 0;
    while (total < (int)buf_size - 1) {
        ssize_t n = read(fd, buf + total, buf_size - 1 - total);
        if (n <= 0) {
            if (total > 0) break;
            return -1;
        }
        total += n;
        buf[total] = '\0';

       
        if (strstr(buf, "\r\n\r\n")) {
            break;
        }
    }

    return total;
}



static int resolve_path(server_t *srv, const char *uri,
                        char *filepath, size_t filepath_len,
                        struct stat *st, int *status) {
   
    if (strstr(uri, "..")) {
        *status = 403;
        return -1;
    }

    snprintf(filepath, filepath_len, "%s%s", srv->doc_root, uri);

    
    if (stat(filepath, st) == 0 && S_ISDIR(st->st_mode)) {
        size_t len = strlen(filepath);
        if (filepath[len - 1] != '/') {
            strncat(filepath, "/", filepath_len - len - 1);
        }
        strncat(filepath, "index.html", filepath_len - strlen(filepath) - 1);
    }

    
    if (stat(filepath, st) < 0) {
        *status = 404;
        return -1;
    }

   
    if (!(st->st_mode & S_IROTH)) {
        *status = 403;
        return -1;
    }

    return 0;
}



void handle_connection(server_t *srv, int client_fd) {
    char buf[MAX_HEADER_SIZE];
    http_request_t req;
    int keep_alive = 1;

    while (keep_alive && srv->running) {
        /* Compute dynamic timeout based on current load */
        int timeout = compute_keepalive_timeout(srv);

        /* Read request (with timeout) */
        int nread = read_request(client_fd, buf, sizeof(buf), timeout);
        if (nread <= 0) {
            break;  /* timeout or client closed */
        }

        /* Parse request */
        if (parse_request(buf, &req) < 0) {
            send_error(client_fd, 400);
            break;
        }

        /* Resolve file path */
        char filepath[MAX_PATH_LEN];
        struct stat st;
        int status = 0;

        if (resolve_path(srv, req.uri, filepath, sizeof(filepath), &st, &status) < 0) {
            send_error(client_fd, status);
            /* On HTTP/1.0 or error, close connection */
            if (req.version == HTTP_10) break;
            /* On HTTP/1.1, we can keep going after 404/403 */
            keep_alive = req.keep_alive;
            continue;
        }

        /* Send file */
        const char *mime = get_mime_type(filepath);
        send_file_response(client_fd, 200, mime, filepath, st.st_size);

        /* Decide whether to keep connection alive */
        keep_alive = req.keep_alive;

        fprintf(stderr, "[worker %lu] %s %s -> 200 (%zu bytes)\n",
                (unsigned long)pthread_self(), req.method, req.uri, (size_t)st.st_size);
    }
}
