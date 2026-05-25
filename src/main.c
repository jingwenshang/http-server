#include "server.h"

static server_t server;



static void handle_signal(int sig) {
    (void)sig;
    fprintf(stderr, "\n[server] Shutting down...\n");
    server.running = 0;
    close(server.listen_fd);
}



static void parse_args(int argc, char *argv[]) {
    server.port        = DEFAULT_PORT;
    server.num_workers = NUM_WORKERS;
    strncpy(server.doc_root, DEFAULT_DOC_ROOT, MAX_PATH_LEN - 1);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            server.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-document_root") == 0 && i + 1 < argc) {
            strncpy(server.doc_root, argv[++i], MAX_PATH_LEN - 1);
        } else if (strcmp(argv[i], "-workers") == 0 && i + 1 < argc) {
            server.num_workers = atoi(argv[++i]);
            if (server.num_workers < 1) server.num_workers = 1;
            if (server.num_workers > 256) server.num_workers = 256;
        }
    }
}



static int setup_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Allow port reuse (avoid "Address already in use") */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}



int main(int argc, char *argv[]) {
    parse_args(argc, argv);


    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);


    queue_init(&server.queue);
    server.active_connections = 0;
    server.running = 1;

 
    struct stat st;
    if (stat(server.doc_root, &st) < 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "[server] Error: document root '%s' does not exist\n",
                server.doc_root);
        exit(EXIT_FAILURE);
    }


    server.listen_fd = setup_listen_socket(server.port);

    fprintf(stderr, "╔══════════════════════════════════════════╗\n");
    fprintf(stderr, "║  Web Server Started                      ║\n");
    fprintf(stderr, "║  Port:     %-28d  ║\n", server.port);
    fprintf(stderr, "║  Workers:  %-28d  ║\n", server.num_workers);
    fprintf(stderr, "║  DocRoot:  %-28s  ║\n", server.doc_root);
    fprintf(stderr, "╚══════════════════════════════════════════╝\n");


    pthread_t *workers = malloc(sizeof(pthread_t) * server.num_workers);
    for (int i = 0; i < server.num_workers; i++) {
        if (pthread_create(&workers[i], NULL, worker_main, &server) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_detach(workers[i]);
    }

   
    while (server.running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server.listen_fd,
                               (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!server.running) break;
            perror("accept");
            continue;
        }

        fprintf(stderr, "[server] New connection from %s:%d (fd=%d)\n",
                inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port),
                client_fd);

        
        queue_push(&server.queue, client_fd);
    }

   
    free(workers);
    fprintf(stderr, "[server] Goodbye.\n");

    return 0;
}
