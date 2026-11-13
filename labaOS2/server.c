#define _GNU_SOURCE
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 5000

static volatile sig_atomic_t was_sighup = 0;

static void sighup_handler(int signo)
{
    (void)signo;
    was_sighup = 1;
}

int main(void)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { perror("socket"); return EXIT_FAILURE; }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind"); close(server_fd); return EXIT_FAILURE;
    }
    if (listen(server_fd, 16) == -1) {
        perror("listen"); close(server_fd); return EXIT_FAILURE;
    }

    sigset_t blocked, original;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blocked, &original) == -1) {
        perror("sigprocmask"); return EXIT_FAILURE;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) == -1) { perror("sigaction"); return EXIT_FAILURE; }

    printf("PID %d, слушаю порт %d\n", getpid(), PORT);
    fflush(stdout);

    int client_fd = -1;
    char buf[4096];

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);                       
        FD_SET(server_fd, &rfds);
        int max_fd = server_fd;
        if (client_fd != -1) {
            FD_SET(client_fd, &rfds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        int ready = pselect(max_fd + 1, &rfds, NULL, NULL, NULL, &original);
        if (ready == -1) {
            if (errno == EINTR) {
                if (was_sighup) {             
                    was_sighup = 0;
                    printf("Получен SIGHUP\n");
                    fflush(stdout);
                }
                continue;                   
            }
            perror("pselect");
            break;
        }

        if (FD_ISSET(server_fd, &rfds)) {     
            int fd = accept(server_fd, NULL, NULL);
            if (fd == -1) {
                perror("accept");
            } else if (client_fd == -1) {
                client_fd = fd;
                printf("Новое соединение принято (fd=%d)\n", fd);
            } else {
                printf("Новое соединение (fd=%d) закрыто: клиент уже есть\n", fd);
                close(fd);
            }
            fflush(stdout);
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &rfds)) {  
            ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
            if (n > 0) {
                printf("Получено байт: %zd\n", n);
            } else {
                if (n == 0) printf("Клиент отключился\n");
                else perror("recv");
                close(client_fd);
                client_fd = -1;
            }
            fflush(stdout);
        }
    }
    if (client_fd != -1) close(client_fd);
    close(server_fd);
    sigprocmask(SIG_SETMASK, &original, NULL);
    return EXIT_SUCCESS;
}
