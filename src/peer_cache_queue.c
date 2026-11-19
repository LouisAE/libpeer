#include "peer_cache_queue.h"
#include "utils.h"
#include "rtp.h"

#include "config.h"
#if CONFIG_USE_LWIP
#include <lwip/sockets.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include <stddef.h>
#include <string.h>

#define PEER_QUEUE_PTR_INCR(p) ((p) = ((p) + 1) % PEER_CACHE_QUEUE_SIZE)

void peer_cache_queue_init(PeerCacheQueue* q, uint8_t* buf_ptr, uint32_t buf_size)
{
    memset(q, 0, sizeof(PeerCacheQueue));
    q->buf = buf_ptr;
    q->buf_size = buf_size;
}
void peer_cache_queue_put(PeerCacheQueue* q, uint16_t seq, uint8_t* data, uint16_t size)
{
    uint8_t pos = 0;


    if (q->buf == NULL || size > q->buf_size)
    {
        return;
    }

    // buffer is full, reset the pointer
    if (q->buf_head + size >= q->buf_size)
    {
        q->buf_head = 0;
    }

    memcpy((q->buf) + (q->buf_head), data, size);

    // handle the replaced packet, pop their pointers from the queue
    for (pos = q->tail; pos != q->head; PEER_QUEUE_PTR_INCR(pos))
    {
        if (q->ptrs[pos] >= ((q->buf) + (q->buf_head)) && q->ptrs[pos] < ((q->buf) + (q->buf_head)) + size)
        {
            q->tail = pos;
            PEER_QUEUE_PTR_INCR(q->tail);
        }
        //q->ptrs[q->tail] = NULL;
    }

    // update the queue
    q->ptrs[q->head] = q->buf + q->buf_head;
    q->seq_max = seq;
    q->packet_lens[q->head] = size;
    q->buf_head += size;

    PEER_QUEUE_PTR_INCR(q->head);
    if (q->head == q->tail)
    {
        PEER_QUEUE_PTR_INCR(q->tail);
    }
}

uint16_t peer_cache_queue_get(PeerCacheQueue* q, uint16_t seq, uint8_t** data)
{
    int pos = 0;
    uint16_t seq_min = 0; /*minimun seq in the queue*/
    RtpHeader* h = NULL;

    if (q->buf == NULL || q->head == q->tail) /* the queue is empty */
    {
        goto empty;
    }

    // (a + k - b) % k means the amount of members between a and b, 
    // k is the size of the queue
    // we do not consider sequence number rollback for now
    seq_min = q->seq_max - ((q->head + PEER_CACHE_QUEUE_SIZE - q->tail - 1) % PEER_CACHE_QUEUE_SIZE);
    if (seq_min > seq || seq > q->seq_max)
    {
        LOGD("Cache Miss %d, Older: %d, Newer: %d", seq, seq_min, q->seq_max);
        goto empty;
    }

    // we assume that sequence numbers are continuous
    
    pos = (q->tail + (seq - seq_min)) % PEER_CACHE_QUEUE_SIZE;
    *data = q->ptrs[pos];
    h = (RtpHeader*)(*data);
    if (ntohs(h->seq_number) != seq)
    {
        LOGD("Hit buf fault %d, Older: %d, Newer: %d", seq, ((RtpHeader*)(q->ptrs[q->tail]))->seq_number, q->seq_max);
        goto empty;
    }
    LOGD("Get:version=%d\n, type=%d\n, seq_number=%d\n, timestamp=%d\n", 
    h->version, h->type, ntohs(h->seq_number), ntohl(h->timestamp));
    return q->packet_lens[pos];
    // for (pos = q->tail; pos != q->head; PEER_QUEUE_PTR_INCR(pos))
    // {
    //     h = (RtpHeader*)(q->ptrs[pos]);
    //     if (ntohs(h->seq_number) == seq)
    //     {
    //         LOGD("Get: version=%d\n, type=%d\n, seq_number=%d\n, timestamp=%d\n", 
    //         h->version, h->type, ntohs(h->seq_number), ntohl(h->timestamp));
    //         *data = (uint8_t*)h;
    //         return q->packet_lens[pos];
    //     }
    // }
    // LOGD("Cache Miss %d, Older: %d, Newer: %d\n", seq, ntohs(((RtpHeader*)(q->ptrs[q->tail]))->seq_number), q->seq_max);

empty:
    *data = NULL;
    return 0;
}