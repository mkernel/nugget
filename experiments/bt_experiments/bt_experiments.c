#include <stdio.h>
#include <string.h>
#include "btstack.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/watchdog.h"

enum bt_action {
    NOP,
    INIT,
    STARTSCAN,
    STOPSCAN,
    OVERWATCH,
};

struct bt_command {
    enum bt_action action;
};

struct output {
    char buf[128];
};

queue_t command_queue;
queue_t output_queue;

struct ble_device {
    bd_addr_t address;
    char name[64];
    bool long_name;
    bool name_found;
};

struct ble_device found_devices[64];
bd_addr_t overwatch;
bool overwatch_mode;
bool present;
int free_device=0;

int find_device(bd_addr_t addr) {
    for(int i=0;i<free_device;i++) {
        bool equal = true;
        for(int j =0;j<sizeof(bd_addr_t);j++) {
            if(addr[j] != found_devices[i].address[j]) {
                equal = false;
            }
        }
        if(equal) {
            return i;
        }
    }
    return -1;
}

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
                } else if(strcmp(current,"ATSTARTSCAN")==0) {
                    struct bt_command cmd;
                    cmd.action=STARTSCAN;
                    if(!queue_try_add(&command_queue,(void*)&cmd)) {
                        puts("ERR - BT subsystem nonresponsive");
                    }
                } else if(strcmp(current,"ATSTOPSCAN")==0) {
                    struct bt_command cmd;
                    cmd.action=STOPSCAN;
                    if(!queue_try_add(&command_queue,(void*)&cmd)) {
                        puts("ERR - BT Subsystem nonresponsive");
                    }
                } else if(strcmp(current,"ATREBOOT")==0) {
                    watchdog_enable(1,1);
                    while(true);
                } else if(strcmp(current,"ATOVERWATCH")==0) {
                    struct bt_command cmd;
                    cmd.action=OVERWATCH;
                    if(!queue_try_add(&command_queue,(void*)&cmd)) {
                        puts("ERR - BT subsystem nonresponsive");
                    }
                } else if(current[0]=='A' && current[1]=='T' && current[2]=='+') {
                    //this is a variable access. its either read or write.
                    //the pattern is AT+{VARIABLE}? for reading and
                    //AT+{VARIABLE}={VALUE} for writing.
                    int idx=3;
                    bool read=false;
                    bool write=false;
                    while(current[idx]!='?' && current[idx]!='=') {
                        idx++;
                        if(current[idx]==0) break;
                        if(current[idx]=='?') read=true;
                        if(current[idx]=='=') write=true;
                    }
                    if(!read && !write) {
                        puts("ERR - Syntax");
                    } else {
                        char variable[32];
                        memcpy(variable,current+3,idx-3);
                        variable[idx-3]=0;
                        if(strcmp(variable,"OVERWATCH")==0) {
                            if(write) {
                                char value[128];
                                strcpy(value,current+idx+1);
                                sscanf_bd_addr(value,overwatch);
                            } else {
                                puts(bd_addr_to_str(overwatch));
                            }
                        } else {
                            printf("ERR - Unknown Variable %s\n",variable);
                        }
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
static btstack_timer_source_t overwatch_timeout;

static void overwatch_timeout_handler(struct btstack_timer_source *ts) {
    qprint("OVERWATCH - disappeared");
    present=false;
}

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
            {
                bd_addr_t address;
                gap_event_advertising_report_get_address(packet, address);
                if(overwatch_mode) {
                    bool equal=true;
                    for(int i=0;i<sizeof(bd_addr_t);i++) {
                        if(overwatch[i] != address[i]) {
                            equal=false;
                        }
                    }
                    if(equal) {
                        if(present) {
                            btstack_run_loop_remove_timer(&overwatch_timeout);
                        }
                        if(!present) {
                            qprintf("OVERWATCH - appeared %s",bd_addr_to_str(address));
                            present=true;
                        }
                        btstack_run_loop_set_timer(&overwatch_timeout, 5000);
                        btstack_run_loop_add_timer(&overwatch_timeout);

                    }
                } else {
                    uint8_t event_type = gap_event_advertising_report_get_advertising_event_type(packet);
                    uint8_t address_type = gap_event_advertising_report_get_address_type(packet);
                    int8_t rssi = gap_event_advertising_report_get_rssi(packet);
                    uint8_t length = gap_event_advertising_report_get_data_length(packet);
                    const uint8_t * adv_data = gap_event_advertising_report_get_data(packet);
                    ad_context_t context;
                    char name[128];
                    bool namefound=false;
                    bool longname=false;
                    for (ad_iterator_init(&context, length, (uint8_t *)adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
                        uint8_t data_type  = ad_iterator_get_data_type(&context);
                        uint8_t size     = ad_iterator_get_data_len(&context);
                        const uint8_t * data = ad_iterator_get_data(&context);
                        if(data_type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME) {
                            char *walk = name;
                            for(int i=0;i<size;i++) {
                                *walk=data[i];
                                walk++;
                            }
                            *walk=0;
                            namefound=true;
                            longname=true;
                        }
                        if(data_type == BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME) {
                            if(!namefound) {
                                char *walk = name;
                                for(int i=0;i<size;i++) {
                                    *walk=data[i];
                                    walk++;
                                }
                                *walk=0;
                                namefound=true;
                            }
                        }
                    }
                    bool update = false;
                    int idx = find_device(address);
                    if(idx == -1) {
                        if(free_device >=64) {
                            qprint("ERR - more than 64 devices in reach");
                        }
                        else {
                            idx = free_device;
                            free_device++;
                            memcpy(found_devices[idx].address,address,sizeof(bd_addr_t));
                            if(namefound) {
                                strcpy(found_devices[idx].name,name);
                            }
                            found_devices[idx].long_name=longname;
                            found_devices[idx].name_found=namefound;
                            update = true;
                        }
                    } else {
                        if(namefound) {
                            if(!found_devices[idx].name_found || (longname && !found_devices[idx].long_name)) {
                                strcpy(found_devices[idx].name,name);
                                found_devices[idx].name_found=namefound;
                                found_devices[idx].long_name=longname;
                                update=true;
                            }
                        }
                    }
                    if(update) {
                        if(found_devices[idx].name_found) {
                            qprintf("* %s: %s",bd_addr_to_str(found_devices[idx].address),found_devices[idx].name);
                        } else {
                            qprintf("* %s",bd_addr_to_str(found_devices[idx].address));
                        }
                    }
                }
            }
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
        } else if(cmd.action == STARTSCAN) {
            free_device = 0;
            overwatch_mode=false;
            gap_set_scan_parameters(0,0x30,0x400);
            gap_start_scan();
            qprint("OK - scan initialized");
        } else if(cmd.action == STOPSCAN) {
            gap_stop_scan();
            qprint("OK - scan cancelled");
        } else if(cmd.action== OVERWATCH) {
            free_device = 0;
            overwatch_mode=true;
            gap_set_scan_parameters(0,0x30,0x400);
            gap_start_scan();
            qprint("OK - overwatching");
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

    overwatch_timeout.process = &overwatch_timeout_handler;

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
