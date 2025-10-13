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
#define BROADCAST_INTERVAL_MAX (10 * CLOCK_SECOND)
#define MAX_MESSAGE_LENGTH 50
#define MAX_NEIGHBORS 20
#define MAX_TWO_HOP_NEIGHBORS 200
#define BYTES_PER_ADDRESS 2
#define PERCENTAGE_CHANCE_BROADCAST 10
#define MAX_LAST_SEQUENCE_NUMBER 100
#define NUMBER_OF_SLOTS_PER_FRAME 16
#define COORDNATOR_PROBABILITY 10 // 10% chance to become coordinator
#define DISCOVERY_TIME (40 * CLOCK_SECOND)

/* Distributed TDMA Configuration */
#define TDMA_SLOT_DURATION      (2 * CLOCK_SECOND)
#define TDMA_FRAME_DURATION     (TDMA_SLOT_DURATION * NUMBER_OF_SLOTS_PER_FRAME)
#define TDMA_BEACON_INTERVAL    (5 * CLOCK_SECOND)
#define TDMA_DISCOVERY_TIME     (10 * CLOCK_SECOND)
#define TDMA_MAX_RETRIES        3
#define TDMA_BACKOFF        (CLOCK_SECOND)
#define TDMA_BACKOFF_MIN        (CLOCK_SECOND)
#define TDMA_BACKOFF_MAX        (8 * CLOCK_SECOND)


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

typedef struct {
    uint16_t node_id;
    uint16_t sequence_number;
    uint16_t slots[NUMBER_OF_SLOTS_PER_FRAME];
} slots_broadcast_t;

typedef struct {
    uint16_t node_id;
    uint16_t dest_id;
    uint16_t sequence_number;
    uint8_t slot;
} slot_request_t;

typedef struct {
    uint16_t node_id;
    uint16_t sequence_number;
    uint8_t slot;
    uint8_t granted; // 1 = granted, 0 = denied
} slot_response_t;

int number_of_neigbors=0;

neighbor_t neighbors[MAX_NEIGHBORS];

uint16_t twoHopNeighbors[MAX_TWO_HOP_NEIGHBORS];

int number_of_two_hop_neighbors = 0;

int current_slot = -1;

uint16_t slot_allocation_table[NUMBER_OF_SLOTS_PER_FRAME];

uint16_t id;

static uint16_t sequence = 0;

int request_slot = 0;
int requested_slot = 0;

slot_request_t pending_request;
slot_response_t pending_response;


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

    LOG_INFO("Input callback triggered with %u bytes\n", len);
  
  if (len == sizeof(neighbor_broadcast_t)) {
    neighbor_broadcast_t received_msg;
    memcpy(&received_msg, data, sizeof(received_msg));
    
    LOG_INFO("Received from [%02x%02x] neighbor broadcast: Seq=%u, Neighbors=%u\n",
             src->u8[0], src->u8[1],
             received_msg.sequence_number,
             received_msg.neighbor_count);
    add_or_update_sorted_neighbor(received_msg.node_id, received_msg.sequence_number);
    for(int i = 0; i < received_msg.neighbor_count; i++) {
        if(received_msg.neighbor_ids[i] != id ) // Avoid adding self
        add_two_hop_neighbor(received_msg.neighbor_ids[i]);
    }
  } else if (len == sizeof(slots_broadcast_t)) {
    slots_broadcast_t received_msg;
    memcpy(&received_msg, data, sizeof(received_msg));
    
    LOG_INFO("Received from [%02x%02x] slots broadcast: Seq=%u\n",
             src->u8[0], src->u8[1],
             received_msg.sequence_number);
    add_or_update_sorted_neighbor(received_msg.node_id, received_msg.sequence_number);
    int free_slots = NUMBER_OF_SLOTS_PER_FRAME;
    for(int i = 0; i < NUMBER_OF_SLOTS_PER_FRAME; i++) {
        if(received_msg.slots[i]) { // If slot is assigned
            slot_allocation_table[i] = received_msg.slots[i];
            free_slots--;
        }
    }
    if(!free_slots) {
        LOG_INFO("No free slots available\n");
        // implement CSMA
        return;
    }
    LOG_INFO("Updated slot allocation table from node %u\n", received_msg.node_id);
    int slot = rand() % free_slots;
    while(slot_allocation_table[slot]) {
        slot = (slot + 1) % NUMBER_OF_SLOTS_PER_FRAME;
    }
    pending_request.node_id = id;
    pending_request.dest_id = received_msg.node_id;
    pending_request.sequence_number = sequence++;
    pending_request.slot = slot;
    request_slot = 1;

  } else if(len == sizeof(slot_response_t)) {
    slot_response_t received_msg;
    memcpy(&received_msg, data, sizeof(received_msg));
    if(received_msg.node_id != id) {
        LOG_INFO("Received slot response not for me, ignoring: NodeID=%u, MyID=%u\n",
                 received_msg.node_id,
                 id);
        return;
    }
    if(received_msg.granted) {
        current_slot = received_msg.slot;
        slot_allocation_table[current_slot] = id; // Mark slot as used
        LOG_INFO("Slot %u granted to me!\n", current_slot);
    } else {
        LOG_INFO("Slot %u request denied\n", received_msg.slot);
    }
  } else if (len == sizeof(slot_request_t)) {
    slot_request_t received_msg;
    memcpy(&received_msg, data, sizeof(received_msg));
    if(received_msg.dest_id != id) {
        LOG_INFO("Received slot request not for me, ignoring: DestID=%u, MyID=%u\n",
                 received_msg.dest_id,
                 id);
        return;
    }
    LOG_INFO("Received slot request from node %u for slot %u\n",
             received_msg.node_id,
             received_msg.slot);
    requested_slot = 1;
    pending_response.node_id = received_msg.node_id;
    pending_response.sequence_number = sequence++;
    pending_response.slot = received_msg.slot;
    pending_response.granted = !slot_allocation_table[received_msg.slot]; // Default to denied

    }
   else{
    LOG_INFO("Received %u bytes from [%02x%02x]\n", 
             len, src->u8[0], src->u8[1]);
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
  static struct etimer 
    slot_timer, 
    // backoff_timer, 
    // time_frame_timer, 
    discovery_timer, 
    broadcast_timer
    // unicast_timer
    ;
  static neighbor_broadcast_t tx_message;
  static slots_broadcast_t sb_msg;
  //static unicast_message_t tx_unicast_message;

  memset(slot_allocation_table, 0, sizeof(slot_allocation_table));

//   etimer_set(&slot_timer, TDMA_SLOT_DURATION);
//   etimer_set(&backoff_timer, TDMA_BACKOFF);
  
  PROCESS_BEGIN();

  
//   LOG_INFO("Starting Simple MAC Broadcast App\n");
//   LOG_INFO("My MAC address: %02x%02x\n", 
//            linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
  
  /* Initialize NullNet */
  nullnet_set_input_callback(input_callback);
  
  /* Generate node ID from MAC address */
  id = linkaddr_node_addr.u8[0] << 8 | linkaddr_node_addr.u8[1];
  tx_message.node_id = id;

  etimer_set(&discovery_timer, DISCOVERY_TIME);

  // tx_unicast_message.src_id = tx_message.node_id;
  
  /* Start periodic broadcasting */
  while(1) {

    while(!etimer_expired(&discovery_timer)){
        int rand_num = rand()%PERCENTAGE_CHANCE_BROADCAST;
            if(!rand_num){
                uint32_t interval = BROADCAST_INTERVAL_MIN + 
                        (rand() % (BROADCAST_INTERVAL_MAX - BROADCAST_INTERVAL_MIN));
        
                generate_neighbor_broadcast(&tx_message, sequence++);
                etimer_set(&broadcast_timer, interval);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&broadcast_timer));
                
                /* Prepare the message */
                tx_message.sequence_number = sequence++;

                //nullnet_set_dest_addr(NULL);  // Explicitly set broadcast
                
                /* Set NullNet buffer and send */
                nullnet_buf = (uint8_t *)&tx_message;
                nullnet_len = sizeof(tx_message);
                
                LOG_INFO("Broadcasting: Seq=%u\n", 
                        tx_message.sequence_number);
                
                /* Send the broadcast */
                NETSTACK_NETWORK.output(NULL);
            }
    }

    if(requested_slot){
                    requested_slot = 0;
                    /* SET THE DESTINATION ADDRESS FOR UNICAST */
                    linkaddr_t dest_addr;
                    /* Convert node ID to linkaddr_t */
                    dest_addr.u8[0] = (pending_request.node_id >> 8) & 0xFF;
                    dest_addr.u8[1] = pending_request.node_id & 0xFF;
                    // nullnet_set_dest_addr(&dest_addr);

                    slot_response_t resp_msg;
                    resp_msg.node_id = pending_request.node_id;
                    resp_msg.sequence_number = sequence++;
                    resp_msg.slot = pending_request.slot;
                    if(!slot_allocation_table[resp_msg.slot]) {
                        resp_msg.granted = 1;
                        slot_allocation_table[resp_msg.slot] = pending_request.node_id; // Mark slot as used
                        LOG_INFO("Granting slot %u to node %u\n", resp_msg.slot, pending_request.node_id);
                    } else {
                        resp_msg.granted = 0;
                        LOG_INFO("Denying slot %u to node %u (already taken)\n", resp_msg.slot, pending_request.node_id);
                    }
                    nullnet_buf = (uint8_t *)&resp_msg;
                    nullnet_len = sizeof(resp_msg);
                    NETSTACK_NETWORK.output(&dest_addr);
    }
    if(request_slot){
                    request_slot = 0;
                    LOG_INFO("Sending request for slot %u\n", pending_response.slot);
                    struct etimer slot_timer;
                    etimer_set(&slot_timer, TDMA_BACKOFF);
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&slot_timer));

                    /* SET THE DESTINATION ADDRESS FOR UNICAST */
                    linkaddr_t dest_addr;
                    /* Convert node ID to linkaddr_t */
                    dest_addr.u8[0] = (pending_response.node_id >> 8) & 0xFF;
                    dest_addr.u8[1] = pending_response.node_id & 0xFF;
                    // nullnet_set_dest_addr(&dest_addr);

                    slot_request_t sr_msg;
                    sr_msg.node_id = id;
                    sr_msg.dest_id = pending_response.node_id;
                    sr_msg.sequence_number = sequence++;
                    sr_msg.slot = pending_request.slot;
                    nullnet_buf = (uint8_t *)&sr_msg;
                    nullnet_len = sizeof(sr_msg);
                    LOG_INFO("Requesting slot %u: Seq=%u\n", sr_msg.slot, sr_msg.sequence_number);
                    NETSTACK_NETWORK.output(&dest_addr);
                }


    if (current_slot<0) {
        if (!(rand()%COORDNATOR_PROBABILITY)){
            LOG_INFO("Sending frames\n");
            current_slot = rand()%NUMBER_OF_SLOTS_PER_FRAME;
            slot_allocation_table[current_slot] = id; // Mark slot as used
            etimer_set(&slot_timer, TDMA_SLOT_DURATION * current_slot + TDMA_BACKOFF);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&slot_timer));
            //etimer_set(&slot_timer, TDMA_SLOT_DURATION);

            sb_msg.node_id = id;
            sb_msg.sequence_number = sequence++;
            memcpy(sb_msg.slots, slot_allocation_table, sizeof(slot_allocation_table));
            nullnet_buf = (uint8_t *)&sb_msg;
            nullnet_len = sizeof(sb_msg);
            LOG_INFO("Broadcasting slot allocation: Seq=%u\n", sb_msg.sequence_number);
            NETSTACK_NETWORK.output(NULL);
        //     while(!etimer_expired(&slot_timer)){
        //     }
        // } else {
        //     etimer_set(&time_frame_timer, TDMA_FRAME_DURATION);
        //     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&time_frame_timer));
        //     while(!etimer_expired(&time_frame_timer)){
        //     }
        }
    }

    if(current_slot > 0){
        etimer_set(&slot_timer, TDMA_SLOT_DURATION * current_slot);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&slot_timer));
    }
  } 
  
  PROCESS_END();
}