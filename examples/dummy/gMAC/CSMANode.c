#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/rtimer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "net/packetbuf.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */
#define BROADCAST_INTERVAL_MIN (3 * CLOCK_SECOND)
#define BROADCAST_INTERVAL_MAX (8 * CLOCK_SECOND)
#define MAX_MESSAGE_LENGTH 50
#define MAX_NEIGHBORS 20
#define MAX_TWO_HOP_NEIGHBORS 200
#define BYTES_PER_ADDRESS 2
#define PERCENTAGE_CHANCE_BROADCAST 10
#define MAX_LAST_SEQUENCE_NUMBER 100


/* Process declaration */
PROCESS(simple_mac_process, "Simple MAC Broadcast");
AUTOSTART_PROCESSES(&simple_mac_process);

/* Message structure */
typedef struct {
  uint16_t node_id;
  uint16_t sequence_number;
  char message[30];
} broadcast_message_t;

typedef struct {
  //uint8_t node_id[BYTES_PER_ADDRESS];
  uint16_t node_id;
  uint16_t last_sequence_number;
} neighbor_t;

typedef struct {
    uint16_t dest_id;
    uint16_t src_id;
    uint16_t sequence_number;
    char message[30];
} unicast_message_t;

typedef struct {
    uint16_t node_id;
    uint16_t sequence_number;
    uint8_t neighbor_count;
    uint16_t neighbor_ids[MAX_NEIGHBORS];
} neighbor_broadcast_t;

int number_of_neigbors=0;

neighbor_t neighbors[MAX_NEIGHBORS];

uint16_t twoHopNeighbors[MAX_TWO_HOP_NEIGHBORS];

int number_of_two_hop_neighbors = 0;



int abs(int x) {
    return x < 0 ? -x : x;
}

int add_or_update_sorted_neighbor(uint16_t node_id, uint16_t sequence_number) {
    int head = 0; int tail = number_of_neigbors - 1;
    // Binary search for the correct position
    while(head <= tail) {
        int mid = (head + tail) / 2;
        if(neighbors[mid].node_id == node_id) {
            neighbors[mid].last_sequence_number = sequence_number;
            return 0; // Updated existing neighbor
        } else if(neighbors[mid].node_id < node_id) {
            head = mid + 1;
        } else {
            tail = mid - 1;
        }
    }
    // Insert new neighbor at position 'head'
    if(number_of_neigbors < MAX_NEIGHBORS) {
        for(int i = number_of_neigbors; i > head; i--) {
            neighbors[i] = neighbors[i - 1];
        }
        neighbors[head].node_id = node_id;
        neighbors[head].last_sequence_number = sequence_number;
        number_of_neigbors++;
        LOG_INFO("Added new neighbor %u, total neighbors: %d\n", node_id, number_of_neigbors);
        return 1;
    } else {
        LOG_WARN("Neighbor list full, cannot add new neighbor %u\n", node_id);
        return 0;
    }
}

int lookup_neighbor(uint16_t node_id) {
    int head = 0; int tail = number_of_neigbors - 1;
    int mid;
    while(head <= tail) {
        mid = (head + tail) / 2;
        if(neighbors[mid].node_id == node_id) {
            return mid;
        } else if(neighbors[mid].node_id < node_id) {
            head = mid + 1;
        } else {
            tail = mid - 1;
        }
    }
    return -1; // Not found
}

int lookup_two_hop_neighbor(uint16_t node_id) {
    int head = 0; int tail = number_of_two_hop_neighbors - 1;
    while(head <= tail) {
         int mid = (head + tail) / 2;
        if(twoHopNeighbors[mid] == node_id) {
            return mid;
        } else if(twoHopNeighbors[mid] < node_id) {
            head = mid + 1;
        } else {
            tail = mid - 1; 
        }
    }
    return -head-1; // Not found
}

int add_two_hop_neighbor(uint16_t node_id) {
    if (number_of_two_hop_neighbors >= MAX_TWO_HOP_NEIGHBORS) {
        LOG_WARN("Two-hop neighbor list full, cannot add new neighbor %u\n", node_id);
        return 0;
    }
    int index = lookup_neighbor(node_id);
    if (index  >= 0) {
        return 0; // Already a direct neighbor
    }
    index = lookup_two_hop_neighbor(node_id); // Get insertion point
    // Check if already in two-hop neighbors
    if (index >= 0) {
        return 0; // Already a two-hop neighbor
    }
    index = -index - 1; // Convert to insertion point
    number_of_two_hop_neighbors++;
    for(int i = number_of_two_hop_neighbors; i > index; i--) {
        twoHopNeighbors[i] = twoHopNeighbors[i - 1];
    }

    twoHopNeighbors[index] = node_id;
    LOG_INFO("Added new two-hop neighbor %u, total two-hop neighbors: %d\n", node_id, number_of_two_hop_neighbors);
    return 1;

}

int add_or_update_neighbor(uint16_t node_id, uint16_t sequence_number) {
    for(int i = 0; i < number_of_neigbors; i++) {
        if(neighbors[i].node_id == node_id) {
            neighbors[i].last_sequence_number = sequence_number;
            return 0;
        }
    }
    if(number_of_neigbors < MAX_NEIGHBORS) {
        neighbors[number_of_neigbors].node_id = node_id;
        neighbors[number_of_neigbors].last_sequence_number = sequence_number;
        number_of_neigbors++;
        LOG_INFO("Added new neighbor %u, total neighbors: %d\n", node_id, number_of_neigbors);
        return 1;
    } else {
        LOG_WARN("Neighbor list full, cannot add new neighbor %u\n", node_id);
        return 0;
    }
}

int remove_neighbor(int sequence) {
    for(int i = 0; i < number_of_neigbors; i++) {
        if(abs(neighbors[i].last_sequence_number - sequence) > MAX_LAST_SEQUENCE_NUMBER) { // Example condition
            LOG_INFO("Removing neighbor %u\n", neighbors[i].node_id);
            neighbors[i] = neighbors[number_of_neigbors - 1]; // Replace with last
            number_of_neigbors--;
            return 1;
        }
    }
    return 0;
}

void generate_neighbor_broadcast(neighbor_broadcast_t *nb_msg, uint16_t sequence_number) {
    nb_msg->node_id = (linkaddr_node_addr.u8[0] << 8) | linkaddr_node_addr.u8[1];
    nb_msg->sequence_number = sequence_number; // Random sequence number
    nb_msg->neighbor_count = number_of_neigbors;
    for(int i = 0; i < number_of_neigbors; i++) {
        nb_msg->neighbor_ids[i] = neighbors[i].node_id;
    }
}



/*---------------------------------------------------------------------------*/
/* Callback function for received packets */
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest) {
  
  if(len == sizeof(broadcast_message_t)) {
    broadcast_message_t received_msg;
    memcpy(&received_msg, data, sizeof(received_msg));
    
    LOG_INFO("Received from [%02x%02x] broadcast: Seq=%u, Msg='%s'\n",
             src->u8[0], src->u8[1],
             received_msg.sequence_number,
             received_msg.message);
    add_or_update_sorted_neighbor(received_msg.node_id, received_msg.sequence_number);
  } else if (len == sizeof(unicast_message_t)) {
    unicast_message_t received_msg;
    memcpy(&received_msg, data, sizeof(received_msg));
    if (received_msg.dest_id != ((linkaddr_node_addr.u8[0] << 8) | linkaddr_node_addr.u8[1])) {
        LOG_INFO("Received unicast not for me, ignoring: Dest=%u, MyID=%u\n",
                 received_msg.dest_id,
                 (linkaddr_node_addr.u8[0] << 8) | linkaddr_node_addr.u8[1]);
        add_or_update_sorted_neighbor(received_msg.src_id, received_msg.sequence_number);
        return;
    }
    LOG_INFO("Received unicast from %u to %u: Seq=%u, Msg='%s'\n",
             received_msg.src_id,
             received_msg.dest_id,
             received_msg.sequence_number,
             received_msg.message);
    add_or_update_sorted_neighbor(received_msg.src_id, received_msg.sequence_number);
  } else if (len == sizeof(neighbor_broadcast_t)) {
    neighbor_broadcast_t received_msg;
    memcpy(&received_msg, data, sizeof(received_msg));
    
    LOG_INFO("Received from [%02x%02x] neighbor broadcast: Seq=%u, Neighbors=%u\n",
             src->u8[0], src->u8[1],
             received_msg.sequence_number,
             received_msg.neighbor_count);
    add_or_update_sorted_neighbor(received_msg.node_id, received_msg.sequence_number);
    for(int i = 0; i < received_msg.neighbor_count; i++) {
        add_two_hop_neighbor(received_msg.neighbor_ids[i]);
    }
  } else{
    // LOG_INFO("Received %u bytes from [%02x%02x]\n", 
    //          len, src->u8[0], src->u8[1]);
    LOG_INFO("Porco dio che è sta merda?!\n");
  }
}
/*---------------------------------------------------------------------------*/
/* Generate a random message */
void generate_random_message(char *msg) {
  const char *messages[] = {
    "Hello World!", "Test broadcast", "MAC layer demo",
    "Contiki-NG rocks!", "Wireless sensor", "Network test",
    "Random message", "Broadcast packet"
  };
  
  int msg_index = rand() % (sizeof(messages) / sizeof(messages[0]));
  strncpy(msg, messages[msg_index], sizeof(msg) - 1);
  msg[sizeof(msg) - 1] = '\0';
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(simple_mac_process, ev, data) {
  static struct etimer broadcast_timer; // , unicast_timer;
  static neighbor_broadcast_t tx_message;
  //static unicast_message_t tx_unicast_message;
  static uint16_t sequence = 0;
  
  PROCESS_BEGIN();
  
//   LOG_INFO("Starting Simple MAC Broadcast App\n");
//   LOG_INFO("My MAC address: %02x%02x\n", 
//            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  
  /* Initialize NullNet */
  nullnet_set_input_callback(input_callback);
  
  /* Generate node ID from MAC address */
  tx_message.node_id = (linkaddr_node_addr.u8[0] << 8) | linkaddr_node_addr.u8[1];

  NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 18);

  // tx_unicast_message.src_id = tx_message.node_id;
  
  /* Start periodic broadcasting */
  while(1) {

    // while(!number_of_neigbors){
    //     int rand_num = rand()%PERCENTAGE_CHANCE_BROADCAST;
    //     if(!rand_num){
    //         uint32_t interval = BROADCAST_INTERVAL_MIN + 
    //                    (rand() % (BROADCAST_INTERVAL_MAX - BROADCAST_INTERVAL_MIN));
    
    //         etimer_set(&broadcast_timer, interval);
    //         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&broadcast_timer));
            
    //         /* Prepare the message */
    //         tx_message.sequence_number = sequence++;
    //         generate_random_message(tx_message.message);

    //         //nullnet_set_dest_addr(NULL);  // Explicitly set broadcast
            
    //         /* Set NullNet buffer and send */
    //         nullnet_buf = (uint8_t *)&tx_message;
    //         nullnet_len = sizeof(tx_message);
            
    //         LOG_INFO("Broadcasting: Seq=%u, Msg='%s'\n", 
    //                 tx_message.sequence_number, tx_message.message);
            
    //         /* Send the broadcast */
    //         NETSTACK_NETWORK.output(NULL);
    //     }
    // }

    // for(int i=0; i<number_of_neigbors; i++){
    //     int rand_num = rand()%PERCENTAGE_CHANCE_BROADCAST;
    //     if(!rand_num){
    //         uint32_t interval = BROADCAST_INTERVAL_MIN + 
    //                    (rand() % (BROADCAST_INTERVAL_MAX - BROADCAST_INTERVAL_MIN));
    
    //         etimer_set(&unicast_timer, interval);
    //         PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&unicast_timer));
            
    //         /* Prepare the unicast message */
    //         tx_unicast_message.dest_id = neighbors[i].node_id;
    //         // tx_unicast_message.src_id = tx_unicast_message.dest_id;
    //         tx_unicast_message.sequence_number = sequence++;
    //         generate_random_message(tx_unicast_message.message);

    //         // /* SET THE DESTINATION ADDRESS FOR UNICAST */
    //         // linkaddr_t dest_addr;
    //         // /* Convert node ID to linkaddr_t */
    //         // dest_addr.u8[0] = (neighbors[i].node_id >> 8) & 0xFF;
    //         // dest_addr.u8[1] = neighbors[i].node_id & 0xFF;
    //         //nullnet_set_dest_addr(&dest_addr);

    //         /* Convert to linkaddr_t for unicast */
    //         linkaddr_t dest_addr;
    //         // Create address from node ID (e.g., node 1 -> 0100.0000.0000.0000)
    //         memset(&dest_addr, 0, sizeof(dest_addr));
    //         dest_addr.u8[0] = neighbors[i].node_id;  // First byte is the node ID
            
    //         /* Set NullNet buffer and send */
    //         nullnet_buf = (uint8_t *)&tx_unicast_message;
    //         nullnet_len = sizeof(tx_unicast_message);
            
    //         LOG_INFO("Unicasting to %u: Seq=%u, Msg='%s'\n", 
    //                 tx_unicast_message.dest_id, tx_unicast_message.sequence_number, tx_unicast_message.message);
            
    //         /* Send the unicast */
    //         NETSTACK_NETWORK.output(&dest_addr);
    //     }
    // }

    int rand_num = rand()%PERCENTAGE_CHANCE_BROADCAST;
    if(!rand_num){
        // uint32_t interval = BROADCAST_INTERVAL_MIN + 
        //            (rand() % (BROADCAST_INTERVAL_MAX - BROADCAST_INTERVAL_MIN));
        generate_neighbor_broadcast(&tx_message, sequence++);
        // etimer_set(&broadcast_timer, interval);
        etimer_set(&broadcast_timer, CLOCK_SECOND/3);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&broadcast_timer));
        /* Set NullNet buffer and send */
        nullnet_buf = (uint8_t *)&tx_message;
        nullnet_len = sizeof(tx_message);
        LOG_INFO("Broadcasting neighbor info: Seq=%u, Neighbors=%u\n", 
                 tx_message.sequence_number, tx_message.neighbor_count);
        NETSTACK_NETWORK.output(NULL);
    }
    remove_neighbor(sequence);
    /* Set random interval between broadcasts */
    
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&broadcast_timer));
  } 
  
  PROCESS_END();
}