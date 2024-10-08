#include "server.h"

#ifndef TEST
#define TEST
#endif

const char *welcome_banner = "======================================\n"
                             " Welcome to CSIE Train Booking System \n"
                             "======================================\n";

const char *lock_msg = ">>> Locked.\n";
const char *exit_msg = ">>> Client exit.\n";
const char *cancel_msg = ">>> You cancel the seat.\n";
const char *full_msg = ">>> The shift is fully booked.\n";
const char *seat_booked_msg = ">>> The seat is booked.\n";
const char *no_seat_msg = ">>> No seat to pay.\n";
const char *book_succ_msg = ">>> Your train booking is successful.\n";
const char *invalid_op_msg = ">>> Invalid operation.\n";

char *read_shift_msg = "Please select the shift you want to check [902001-902005]: ";
char *write_shift_msg = "Please select the shift you want to book [902001-902005]: ";
char *write_seat_msg = "Select the seat [1-40] or type \"pay\" to confirm: ";
char *write_seat_or_exit_msg = "Type \"seat\" to continue or \"exit\" to quit [seat/exit]: ";

int maxReq; // get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);
int init_req_table(void);
int init_train_table(void);
int handle_request(request *req, char *input, char *output);
int call_handle(request *req);
int rm_req(int conn_fd);

int main(int argc, char **argv) {
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }
    fd_set master;   // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    int fdmax;       // maximum file descriptor number

    int listener;                       // listening socket descriptor
    int newfd;                          // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256]; // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;
    init_train_table();
    init_req_table();

    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &ai)) != 0) {
        fprintf(stderr, "server: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // add 0.05 seconds timeout for select
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_usec = 50000;

    // main loop
    for (;;) {
        read_fds = master; // copy it
        if (select(fdmax + 1, &read_fds, NULL, NULL, &timeout) == -1) {
            perror("select");
            exit(4);
        }
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        long long tv_usec = tv_now.tv_usec + 1000000 * tv_now.tv_sec;

        // run through the existing connections looking for data to read
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                                   (struct sockaddr *)&remoteaddr, &addrlen);
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        req_table[newfd].conn_fd = newfd;
                        req_table[newfd].start_at = tv_usec;
                        strcpy(req_table[newfd].host, inet_ntop(remoteaddr.ss_family,
                                                                get_in_addr((struct sockaddr *)&remoteaddr),
                                                                remoteIP, INET6_ADDRSTRLEN));
                        printf("server: new connection from %s on "
                               "socket %d\n",
                               req_table[newfd].host,
                               newfd);
                        handle_request(&req_table[newfd], NULL, (req_table[newfd].buf));
                        send(req_table[newfd].conn_fd, req_table[newfd].buf, strlen(req_table[newfd].buf), 0);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("server: socket %d hung up\n", i);
                            rm_req(i);
                        } else {
                            perror("recv");
                        }
                        close(i);           // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        strcpy(req_table[i].buf, buf);
                        printf("%s says: '''%s'''\n", req_table[i].host, buf);
                        memset(buf, 0, sizeof(buf));
                        req_table[i].buf_len = nbytes;
                        int exit_code = call_handle(&req_table[i]);
                        send(req_table[i].conn_fd, req_table[i].buf, strlen(req_table[i].buf), 0);
                        switch (exit_code) {
                        case -2:
                            printf("Client %s invalid operation.\n", req_table[i].host);
                        case -1:
                            printf("Client %s exits.\n", req_table[i].host);
                            FD_CLR(i, &master);
                            close(i);
                            rm_req(i);
                            break;
                        default:
                            break;
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
#ifndef TEST
            if (FD_ISSET(i, &master) && i != listener) {
                float tv_diff = (float)(tv_usec - req_table[i].start_at) / 1000000;
                // 1000000 microseconds = 1 second
                if (tv_diff > 5) {
                    close(i);
                    printf("Timeout: client %s closed in %f seconds.\n", req_table[i].host, tv_diff);
                    FD_CLR(i, &master);
                    rm_req(i);
                }
            }
#endif
        } // END looping through file descriptors
        fflush(stdout);
    } // END for(;;)--and you thought it would never end!

    return 0;
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int init_train_table(void) {
    int shift_id, i, j;
    char fp[FILE_LEN];
    struct stat stat_buf;
    memset(fp, 0, sizeof(fp));

    for (i = 0; i < TRAIN_NUM; i++) {
        shift_id = TRAIN_ID_START + i;
        sprintf(fp, "%s%d", csie_trains_prefix, shift_id);
        FILE *f = fopen(fp, "r");
        for (j = 0; j < SEAT_NUM; j++) {
            fscanf(f, "%d", &train_table[i].seat_stat[j]);
            train_table[i].seat_stat[j] *= 2;
        }
        if (stat(fp, &stat_buf) == -1)
            return -1;
        train_table[i].least_modify = stat_buf.st_mtim;
        train_table[i].id = shift_id;
        strcpy(train_table[i].path, fp);
    }
    return 0;
}
int init_req_table(void) {
    maxReq = getdtablesize();
    req_table = malloc(sizeof(request) * maxReq);
    if (req_table == NULL)
        return 1;
    memset(req_table, 0, sizeof(request) * maxReq);
    return 0;
}
int handle_request(request *req, char *input, char *buf) {
    char tmpbuf[MAX_MSG_LEN];
    char *endptr;
    int shift_id;

    if (input != NULL && strcmp(input, "exit") == 0) {
        strcat(buf, exit_msg);
        return -1;
    }
    // test for good request
    switch (req->status) {

    case INVALID:
        strcat(buf, welcome_banner);
        req->status = SHIFT;
        break;
    case SHIFT:
        // input shift id
        shift_id = strtol(input, &endptr, 10);
        if (*endptr != '\0' || shift_id < TRAIN_ID_START || shift_id > TRAIN_ID_END) {
            strcat(buf, invalid_op_msg);
            return -2;
        } else {
            // it's a valid shift id
#ifdef WRITE_SERVER
            req->booking_info.shift_id = shift_id;
            if (train_full(shift_id)) {
                strcat(buf, full_msg);
            } else {
                sprint_booking_info(tmpbuf, req->booking_info);
                strcat(buf, tmpbuf);
                req->status = SEAT;
            }
#elif defined READ_SERVER
            train_info *info = get_train_info(shift_id);
            sprint_train_info(tmpbuf, info);
            strcat(buf, tmpbuf);
#endif
        }
        break;
    case SEAT:
        // input seat id or "pay"
        if (strcmp(input, "pay") == 0) {
            if (req->booking_info.num_chosen == 0) {
                strcat(buf, no_seat_msg);
            } else {
                strcat(buf, book_succ_msg);
                train_info *tra_info = get_train_info(req->booking_info.shift_id);
                for (int i = 0; i < SEAT_NUM; i++) {
                    if (req->booking_info.seatstats[i] == CHOSEN) {
                        tra_info->seat_stat[i] = PAID;
                        req->booking_info.seatstats[i] = PAID;
                    }
                }
                int err = write_train_info(tra_info);
                if (err != 0)
                    exit(-1);
                req->status = BOOKED;
            }
        } else {
            char *endptr;
            int seat;
            seat = strtol(input, &endptr, 10);
            if (*endptr != '\0' || seat < 1 || seat > SEAT_NUM) {
                strcat(buf, invalid_op_msg);
                return -2;
            } else {
                // select a valid seat id
                seat--;
                train_info *tra_info = get_train_info(req->booking_info.shift_id);
                switch (tra_info->seat_stat[seat]) {
                case FREE:
                    tra_info->seat_stat[seat] = CHOSEN;
                    req->booking_info.seatstats[seat] = CHOSEN;
                    req->booking_info.num_chosen++;
                    break;
                case CHOSEN:
                    if (req->booking_info.seatstats[seat] == CHOSEN) {
                        tra_info->seat_stat[seat] = FREE;
                        req->booking_info.seatstats[seat] = FREE;
                        req->booking_info.num_chosen--;
                        strcat(buf, cancel_msg);
                    } else {
                        strcat(buf, lock_msg);
                    }
                    break;
                case PAID:
                    strcat(buf, seat_booked_msg);
                    break;
                }
                int err = write_train_info(tra_info);
                if (err != 0)
                    exit(-1);
            }
        }
        sprint_booking_info(tmpbuf, req->booking_info);
        strcat(buf, tmpbuf);
        break;
    case BOOKED:
        // input "seat"
        if (strcmp(input, "seat") == 0)
            req->status = SEAT;
        else {
            strcat(buf, invalid_op_msg);
            return -2;
        }
        break;
    }
    // the next question
    switch (req->status) {
    case INVALID:
        break;
    case SHIFT:
#ifdef WRITE_SERVER
        strcat(buf, write_shift_msg);
#elif defined READ_SERVER
        strcat(buf, read_shift_msg);
#endif
        break;
    case SEAT:
        strcat(buf, write_seat_msg);
        break;
    case BOOKED:
        strcat(buf, write_seat_or_exit_msg);
        break;
    }
    // printf("%s sends %s\n", req->host, req->buf);
    return 0;
}

int rm_req(int conn_id) {
    int i;
    request *req = &req_table[conn_id];
    train_info *info = get_train_info(req->booking_info.shift_id);
    for (i = 0; i < SEAT_NUM; i++) {
        if (req->booking_info.seatstats[i] == CHOSEN)
            info->seat_stat[i] = FREE;
    }
    write_train_info(info);
    request null_req = {};
    *req = null_req;
    return 0;
}

int call_handle(request *req) {
    char recv_str[MAX_MSG_LEN];
    int exit_code;
    strcpy(recv_str, req->buf);
    memset(req->buf, 0, sizeof(req->buf));
    char *pch = NULL;

    pch = strtok(recv_str, "\n");
    while (pch != NULL) {
        if (strlen(pch) == 0)
            continue;
        exit_code = handle_request(req, pch, req->buf);
        if (exit_code)
            return exit_code;
        pch = strtok(NULL, "\n");
    };
    return 0;
}
