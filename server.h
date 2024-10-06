#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define TRAIN_NUM 5
#define SEAT_NUM 40
#define TRAIN_ID_START 902001
#define TRAIN_ID_END TRAIN_ID_START + (TRAIN_NUM - 1)
#define FILE_LEN 50
#define MAX_MSG_LEN 512

enum STATE {
    INVALID, // Invalid state
    SHIFT,   // Shift selection
    SEAT,    // Seat selection
    BOOKED   // Payment
};

enum SEAT {
    FREE,   // Seat is unknown
    CHOSEN, // Seat is currently being reserved
    PAID    // Seat is  already paid
};

typedef struct {
    int id;
    char path[FILE_LEN];
    enum SEAT seat_stat[SEAT_NUM];
    struct timespec least_modify;
} train_info;

typedef struct {
    int shift_id;
    int num_chosen;
    enum SEAT seatstats[SEAT_NUM];
} record;

typedef struct {
    char host[512];        // client's host
    int conn_fd;           // fd to talk with client
    char buf[MAX_MSG_LEN]; // data sent by/to client
    size_t buf_len;        // bytes used by buf
    enum STATE status;     // request status
    record booking_info;   // booking status
    long long start_at;    // connection start time
} request;

request *req_table = NULL;
train_info train_table[TRAIN_NUM];

const char *csie_trains_prefix = "./csie_trains/train_";

train_info *get_train_info(int shift_id) {
    if (shift_id < TRAIN_ID_START || shift_id > TRAIN_ID_END)
        return NULL;

    char fp[FILE_LEN];
    memset(fp, 0, sizeof(fp));
    sprintf(fp, "%s%d", csie_trains_prefix, shift_id);

    struct stat stat_buf;
    train_info *info = &train_table[shift_id - TRAIN_ID_START];
    // set values in info
    strcpy(info->path, fp);
    info->id = shift_id;
    if (stat(fp, &stat_buf) == -1)
        return NULL;

    int secdiff = stat_buf.st_mtim.tv_sec - info->least_modify.tv_sec;
    int nsecdiff = stat_buf.st_mtim.tv_nsec - info->least_modify.tv_nsec;
    info->least_modify = stat_buf.st_mtim;
    if (secdiff > 0 || (secdiff == 0 && nsecdiff > 0)) {
        FILE *f = fopen(fp, "r");
        for (int i = 0; i < SEAT_NUM; i++) {
            int status;
            fscanf(f, "%d", &status);
            if (info->seat_stat[i] != PAID)
                info->seat_stat[i] = status;
        }
        fclose(f);
    }
    return info;
}
int write_train_info(train_info *info) {
    FILE *f = fopen(info->path, "w");
    if (f == NULL)
        return 1; // fail to open
    for (int i = 0; i < SEAT_NUM; i++) {
        if ((i + 1) % 4)
            fprintf(f, "%d ", !!info->seat_stat[i]);
        else
            fprintf(f, "%d\n", !!info->seat_stat[i]);
    }
    fclose(f);
    struct stat stat_buf;
    if (stat(info->path, &stat_buf) == -1)
        return 2;
    info->least_modify = stat_buf.st_mtim;
    return 0;
}
int sprint_train_info(char *buf, train_info *info) {
    int len = 0;
    for (int i = 0; i < SEAT_NUM; i++) {
        if ((i + 1) % 4)
            len += sprintf(buf + len, "%d ", info->seat_stat[i]);
        else
            len += sprintf(buf + len, "%d\n", info->seat_stat[i]);
    }
    return 0;
}
int sprint_booking_info(char *dest, record rec) {
    /*
     * Booking info
     * |- Shift ID: 902001
     * |- Chose seat(s): 1,2
     * |- Paid: 3,4
     */
    char chosen_seat[MAX_MSG_LEN];
    char paid[MAX_MSG_LEN];
    int c_len = 0, p_len = 0;
    memset(chosen_seat, 0, sizeof(chosen_seat));
    memset(paid, 0, sizeof(paid));

    for (int i = 0; i < SEAT_NUM; i++) {
        switch (rec.seatstats[i]) {
        case FREE:
            break;
        case CHOSEN:
            if (c_len)
                c_len += sprintf(chosen_seat + c_len, ",%d", i + 1);
            else
                c_len += sprintf(chosen_seat + c_len, "%d", i + 1);
            break;
        case PAID:
            if (p_len)
                p_len += sprintf(paid + p_len, ",%d", i + 1);
            else
                p_len += sprintf(paid + p_len, "%d", i + 1);
            break;
        }
    }
    sprintf(dest, "\nBooking info\n"
                  "|- Shift ID: %d\n"
                  "|- Chose seat(s): %s\n"
                  "|- Paid: %s\n\n",
            rec.shift_id, chosen_seat, paid);
    return 0;
}

int train_full(int shift_id) {
    train_info *info = get_train_info(shift_id);
    for (int i = 0; i < SEAT_NUM; i++)
        if (info->seat_stat[i] == 0)
            return 0;
    return 1;
}