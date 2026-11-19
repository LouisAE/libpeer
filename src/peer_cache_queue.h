#ifndef PEER_CACHE_QUEUE_H_
#define PEER_CACHE_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#ifndef PEER_CACHE_QUEUE_SIZE
#define PEER_CACHE_QUEUE_SIZE 128
#endif 

// circular queue for packet cache
typedef struct PeerCacheQueue {
    uint8_t* buf;       // data buf, can be NULL
    uint32_t buf_head;  // head for buf
    uint8_t head, tail; // head & tail for ptrs
    uint16_t seq_max;    // max sequence number in the queue
    uint32_t buf_size;
    uint8_t* ptrs[PEER_CACHE_QUEUE_SIZE]; // pointers for packets
    uint16_t packet_lens[PEER_CACHE_QUEUE_SIZE];
} PeerCacheQueue;

// initialize an queue
void peer_cache_queue_init(PeerCacheQueue* queue, uint8_t* buf_ptr, uint32_t buf_size);

// put a packet into the queue
void peer_cache_queue_put(PeerCacheQueue* queue, uint16_t seq, uint8_t* data, uint16_t size);

// get a packet with seq and return the length of the packet
// return 0 if the packet dose no exist
uint16_t peer_cache_queue_get(PeerCacheQueue* queue, uint16_t seq, uint8_t** data);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PEER_CACHE_QUEUE_H_ */