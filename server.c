/*
** server.c -- a cheezy multiperson chat server
*/

#include "server.h"

#define PORT "9034" // port we're listening on

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

// #ifdef READ_SERVER
char *read_shift_msg = "Please select the shift you want to check [902001-902005]: ";
// #elif defined WRITE_SERVER
char *write_shift_msg = "Please select the shift you want to book [902001-902005]: ";
char *write_seat_msg = "Select the seat [1-40] or type \"pay\" to confirm: ";
char *write_seat_or_exit_msg = "Type \"seat\" to continue or \"exit\" to quit [seat/exit]: ";
// #endif

int maxReq; // get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);
int init_req_table(void);
int init_train_table(void);
int handleRequest(request *req);

int main(void) {
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
    init_req_table();

    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
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
        fprintf(stderr, "selectserver: failed to bind\n");
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

    // main loop
    for (;;) {
        read_fds = master; // copy it
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

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
                        send(newfd, welcome_banner, strlen(welcome_banner), 0);
                        req_table[newfd].conn_fd = newfd;
                        sprintf(req_table[newfd].host, inet_ntop(remoteaddr.ss_family,
                                                                 get_in_addr((struct sockaddr *)&remoteaddr),
                                                                 remoteIP, INET6_ADDRSTRLEN));
                        printf("selectserver: new connection from %s on "
                               "socket %d\n",
                               req_table[newfd].host,
                               newfd);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i);           // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        req_table[i].buf_len = sprintf(req_table[i].buf, buf);
                        printf("%s", req_table[i].buf);
                        if (handleRequest(&req_table[i]) == -1) {
                            printf("client %s exits.\n", req_table[i].host);
                            FD_CLR(i, &master);
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
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
        printf("read train %d", shift_id);
        sprintf(fp, "%s%d", csie_trains_prefix, shift_id);
        FILE *f = fopen(fp, "r");
        for (j = 0; j < SEAT_NUM; j++) {
            fscanf(f, "%d", &train_table[i].seat_stat[j]);
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
    maxReq = 30;
    req_table = malloc(sizeof(request) * maxReq);
    if (req_table == NULL)
        return 1;
    memset(req_table, 0, sizeof(request) * maxReq);
    return 0;
}
int handleRequest(request *req) {
    char buf[MAX_MSG_LEN], reqbuf[MAX_MSG_LEN], tmpbuf[MAX_MSG_LEN];
    char *endptr;
    int status = req->status;
    memset(buf, 0, sizeof(buf));
    strcpy(reqbuf, req->buf);
    reqbuf[strcspn(reqbuf, "\r\n")] = '\0';
    memset(req->buf, 0, sizeof(reqbuf));
    req->buf_len = 0;
    // test for good request

    switch (status) {
    case INVALID:
        // select shift
        // no input
        break;
    case SHIFT:
        // input shift id
        int shift;
        shift = strtol(reqbuf, &endptr, 10);
        if (*endptr != '\0' || shift < TRAIN_ID_START || shift > TRAIN_ID_END) {
            strcat(buf, invalid_op_msg);
            printf("end = %d; shift = %d\n", *endptr, shift);
            status = INVALID;
        } else {
            req->booking_info.shift_id = shift;
        }
        break;
    case SEAT:
        // input seat id or "pay"
        if (strcmp(reqbuf, "pay") == 0) {
            if (req->booking_info.num_chosen == 0) {
                strcat(buf, no_seat_msg);
            } else {
                train_info *tra_info;
                get_train_info(req->booking_info.shift_id, &tra_info);
                for (int i = 0; i < SEAT_NUM; i++) {
                    if (req->booking_info.seatstats[i] == CHOSEN) {
                        tra_info->seat_stat[i] = BOOKED;
                        req->booking_info.seatstats[i] = BOOKED;
                    }
                }
                if (write_train_info(tra_info) != 0) {
                    fprintf(stderr, "File IO error");
                    exit(-1);
                }
                status = BOOKED;
            }
        } else {
            char *endptr;
            int seat;
            seat = strtol(reqbuf, &endptr, 10);
            if (*endptr != '\0' || seat < 1 || seat > SEAT_NUM) {
                strcat(buf, invalid_op_msg);
                status = SHIFT;
            } else {
                // select a valid seat id

                int rb = 0;
                train_info *tra_info;
                get_train_info(req->booking_info.shift_id, &tra_info);
                switch (tra_info->seat_stat[seat]) {
                case FREE:
                    tra_info->seat_stat[seat] = CHOSEN;
                    rb = 1;
                    break;
                case CHOSEN:
                    if (req->booking_info.seatstats[seat] == CHOSEN) {
                        tra_info->seat_stat[seat] = FREE;
                        strcat(buf, cancel_msg);
                        req->booking_info.seatstats[seat] = FREE;
                        rb = 1;
                    } else {
                        strcat(buf, lock_msg);
                    }
                    break;
                case PAID:
                    strcat(buf, full_msg);
                    break;
                }
                if (rb != 0) {
                    if (write_train_info(tra_info) != 0) {
                        fprintf(stderr, "File IO error ");
                        exit(-1);
                    }
                }
            }
        }
        break;
    case BOOKED:
        // input "seat" or "pay"
        if (strcmp(reqbuf, "seat") == 0)
            status = SEAT;
        else if (strcmp(reqbuf, "exit") == 0) {
            close(req->conn_fd);
            return -1;
        } else {
            strcat(buf, invalid_op_msg);
        }
        break;
    }
    // the next question
    switch (status) {
    case INVALID:
        strcat(buf, write_shift_msg);
        status = SHIFT;
        break;
    case SHIFT:
        strcat(buf, write_seat_msg);
        status = SEAT;
        break;
    case SEAT:
        strcat(buf, write_seat_msg);
        break;
    case BOOKED:
        strcat(buf, write_seat_or_exit_msg);
        status = BOOKED;
        break;
    }
    strcpy(reqbuf, buf);
    int len = strlen(reqbuf);
    req->status = status;
    send(req->conn_fd, reqbuf, len, 0);
    printf("%s is on status %d\n", req->host, req->status);
    return 0;
}
