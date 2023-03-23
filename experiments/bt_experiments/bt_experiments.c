#include <stdio.h>
#include <string.h>
#include "btstack.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

enum bt_action {
    NOP,
    INIT,
};

struct bt_command {
    enum bt_action action;
};

struct output {
    char buf[128];
};

queue_t command_queue;
queue_t output_queue;

void comm_runloop()
{
    char buf[32];
    char *current=buf;
    struct output msg;
    while(true) {
        int fetched = getchar_timeout_us(500);
        if(fetched != PICO_ERROR_TIMEOUT) {
            char input=(char)fetched;

            if(input=='\r' || input=='\n') {
                puts("");
                *current=0;
                current=buf;
                if(strcmp(current,"AT")==0) {
                    puts("OK - bt experiments 0.1");
                } else if(strcmp(current,"ATNOP")==0) {
                    struct bt_command cmd;
                    cmd.action = NOP;
                    if(!queue_try_add(&command_queue,(void*)&cmd)) {
                        puts("ERR - BT Subsystem nonresponsive");
                    }
                } else if(strcmp(current,"ATINIT")==0) {
                    struct bt_command cmd;
                    cmd.action=INIT;
                    if(!queue_try_add(&command_queue,(void*)&cmd)) {
                        puts("ERR - BT subsystem nonresponsive");
                    }
                }
            } else {
                putchar(input);
                *current=input;
                current++;
            }
        }
        if(queue_try_remove(&output_queue,&msg)) {
            puts(msg.buf);
        }
    }
}

void qprint(char *msg) {
    struct output output;
    strcpy(output.buf,msg);
    queue_try_add(&output_queue,(void*)&output);
}

void qprintf(char *msg, ...) {
    va_list args;
    va_start(args,msg);
    struct output output;
    vsprintf(output.buf,msg,args);
    va_end(args);
    queue_try_add(&output_queue,(void*)&output);
}

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_timer_source_t heartbeat;


static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event_type = hci_event_packet_get_type(packet);
    switch(event_type){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                gap_local_bd_addr(local_addr);
                qprintf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                qprint("OK - init done");
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            break;
        default:
            break;
    }
}

static void heartbeat_handler(struct btstack_timer_source *ts) {
    // Invert the led
    struct bt_command cmd;
    if(queue_try_remove(&command_queue,&cmd)) {
        if(cmd.action == NOP) {
            qprint("OK - NOP executed");
        } else if(cmd.action == INIT) {
            qprint("ERR - already initialized");
        }
    }

    // Restart timer
    btstack_run_loop_set_timer(ts, 50);
    btstack_run_loop_add_timer(ts);
}


void bt_init() {
    qprint("WAIT bt init in progress");
    if (cyw43_arch_init()) {
        qprint("failed to initialise cyw43_arch");
        return;
    }

    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gatt_client_init();

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // set one-shot btstack timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 50);
    btstack_run_loop_add_timer(&heartbeat);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_run_loop_execute();

}

int main()
{
    stdio_init_all();
    queue_init(&command_queue,sizeof(struct bt_command),1);
    queue_init(&output_queue,sizeof(struct output),5);

    multicore_launch_core1(comm_runloop);
    //we will initialize a queue to command the bluetooth environment
    //additionally we initialize a output queue so the bt environment can send output to usb
    //while the bluetooth environment is shut off we wait on the queue for a command to init it
    //once we initialized we check for bt commands on a timer once every x milliseconds
    while(true) {
        struct bt_command cmd;
        if(queue_try_remove(&command_queue,&cmd)) {
            if(cmd.action == NOP) {
                qprint("OK - NOP executed");
            } else if(cmd.action == INIT) {
                bt_init();
            } else {
                qprint("ERR - not supported at this stage");
            }
        }
    }
    return 0;
}
