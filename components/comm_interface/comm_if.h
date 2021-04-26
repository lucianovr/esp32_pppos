#ifndef COMM_INTERFACE_H
#define COMM_INTERFACE_H

typedef void(*comm_if_connected_cb_t)(void);
typedef void(*comm_if_disconnected_cb_t)(void);


void comm_if_init(void);
void comm_if_deinit(void);
void comm_if_start(void);
void comm_if_stop(void);

void comm_if_reg_connected_cb(comm_if_connected_cb_t cb);
void comm_if_reg_disconnected_cb(comm_if_disconnected_cb_t cb);

#endif // COMM_INTERFACE_H