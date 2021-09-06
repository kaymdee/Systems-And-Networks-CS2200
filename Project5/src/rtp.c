#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "queue.h"
#include "network.h"
#include "rtp.h"

typedef struct message {
    char *buffer;
    int length;
} message_t;

/* ================================================================ */
/*                  H E L P E R    F U N C T I O N S                */
/* ================================================================ */

/*
 *  Returns a number computed based on the data in the buffer.
 */
int checksum(char *buffer, int length) {


    /*  ----  FIXME  ----
     *
     *  The goal is to return a number that is determined by the contents
     *  of the buffer passed in as a parameter.  There a multitude of ways
     *  to implement this function.  For simplicity, the checksum value is
     *  the sum of each char value times its index (starting at 1). 
     *  (i.e. checksum = first char * 1 + second char * 2 
     *                   + third char * 3 ... + nth char * n )
     */
    int sum = buffer[0];
    for (int i = 1; i < length; i++) {
        sum += (i + 1) * buffer[i];
    }
    return sum;
}

/*
 *  Converts the given buffer into an array of PACKETs and returns
 *  the array.  The value of (*count) should be updated so that it 
 *  contains the length of the array created.
 */
packet_t *packetize(char *buffer, int length, int *count) {

    /*  ----  FIXME  ----
     *
     *  The goal is to turn the buffer into an array of packets.
     *  You should allocate the space for an array of packets and
     *  return a pointer to the first element in that array.  The
     *  integer pointed to by 'count' should be updated to indicate
     *  the number of packets in the array.
     */
    packet_t *packets;
    int total_packets = length / MAX_PAYLOAD_LENGTH;
    int len = MAX_PAYLOAD_LENGTH;
    if (length % MAX_PAYLOAD_LENGTH != 0) {
        total_packets++;
        len = length % MAX_PAYLOAD_LENGTH;
    }
    packets = malloc(sizeof(packet_t) * total_packets);
    for (int i = 0; i < total_packets - 1; i++) {
        packets[i].type = DATA;
        packets[i].payload_length = MAX_PAYLOAD_LENGTH;
        for (int j = 0; j < MAX_PAYLOAD_LENGTH; j++) {
            packets[i].payload[j] = buffer[i * MAX_PAYLOAD_LENGTH + j];
        }
        packets[i].checksum = checksum(packets[i].payload, MAX_PAYLOAD_LENGTH);
    }
    packets[total_packets - 1].type = LAST_DATA;
    packets[total_packets - 1].payload_length = len;
    for (int j = 0; j < len; j++) {
            packets[total_packets - 1].payload[j] = buffer[(total_packets - 1) * MAX_PAYLOAD_LENGTH + j];
    }
    packets[total_packets - 1].checksum = checksum(packets[total_packets - 1].payload, len);
    *count = total_packets;
    return packets;
}

/* ================================================================ */
/*                      R T P       T H R E A D S                   */
/* ================================================================ */

static void *rtp_recv_thread(void *void_ptr) {

    rtp_connection_t *connection = (rtp_connection_t *) void_ptr;

    do {
        message_t *message;
        int buffer_length = 0;
        char *buffer = NULL;
        packet_t packet;

        /* Put messages in buffer until the last packet is received  */
        do {
            if (net_recv_packet(connection->net_connection_handle, &packet) <= 0 || packet.type == TERM) {
                /* remote side has disconnected */
                connection->alive = 0;
                pthread_cond_signal(&connection->recv_cond);
                pthread_cond_signal(&connection->send_cond);
                break;
            }

            /*  ----  FIXME  ----
            *
            * 1. check to make sure payload of packet is correct
            * 2. send an ACK or a NACK, whichever is appropriate
            * 3. if this is the last packet in a sequence of packets
            *    and the payload was corrupted, make sure the loop
            *    does not terminate
            * 4. if the payload matches, add the payload to the buffer
            */
            //check sum
            if (packet.type == DATA || packet.type == LAST_DATA) {
                if (checksum(packet.payload, packet.payload_length) == packet.checksum) {
                    packet_t *ACK_pckt = malloc(sizeof(packet_t));
                    ACK_pckt -> type = ACK;
                    net_send_packet(connection -> net_connection_handle, ACK_pckt);
                    free(ACK_pckt);
                    if (buffer == NULL) {
                        buffer = malloc(packet.payload_length);
                    } else {
                        buffer = realloc(buffer, buffer_length + packet.payload_length);
                    }
                    int limit = packet.payload_length;
                    int i = 0;
                    while (i < limit) {
                        buffer[buffer_length+i] = packet.payload[i];
                        i++;
                    }
                    buffer_length += limit;
                } else {
                    packet_t *NACK_pckt = malloc(sizeof(packet_t));
                    NACK_pckt -> type = NACK;
                    net_send_packet(connection -> net_connection_handle, NACK_pckt);
                    free(NACK_pckt);
                    if (packet.type == LAST_DATA) {
                        packet.type = DATA;
                    }
                }
            }

            /*
            * 
            *  What if the packet received is not a data packet?
            *  If it is a NACK or an ACK, the sending thread should
            *  be notified so that it can finish sending the message.
            *   
            *  1. add the necessary fields to the CONNECTION data structure
            *     in rtp.h so that the sending thread has a way to determine
            *     whether a NACK or an ACK was received
            *  2. signal the sending thread that an ACK or a NACK has been
            *     received.
            */
           else if (packet.type == NACK) {
               connection->ack = 2;
               pthread_cond_signal(&connection->ack_cond);
           } else if (packet.type == ACK) {
               connection->ack = 1;
               pthread_cond_signal(&connection->ack_cond);
           }


        } while (packet.type != LAST_DATA);

        if (connection->alive == 1) {
            /*  ----  FIXME  ----
            *
            * Now that an entire message has been received, we need to
            * add it to the queue to provide to the rtp client.
            *
            * 1. Add message to the received queue.
            * 2. Signal the client thread that a message has been received.
            */
            message = malloc(sizeof(message_t));
            message->buffer = buffer;
            message->length = buffer_length;
            pthread_mutex_lock(&connection->recv_mutex);
            queue_add(&connection->recv_queue, message);
            pthread_mutex_unlock(&connection->recv_mutex);
            pthread_cond_signal(&connection->recv_cond);

        } else free(buffer);

    } while (connection->alive == 1);

    return NULL;

}

static void *rtp_send_thread(void *void_ptr) {

    rtp_connection_t *connection = (rtp_connection_t *) void_ptr;
    message_t *message;
    int array_length = 0;
    int i;
    packet_t *packet_array;

    do {
        /* Extract the next message from the send queue */
        pthread_mutex_lock(&connection->send_mutex);
        while (queue_size(&connection->send_queue) == 0 && connection->alive == 1) {
            pthread_cond_wait(&connection->send_cond, &connection->send_mutex);
        }

        if (connection->alive == 0) break;

        message = queue_extract(&connection->send_queue);

        pthread_mutex_unlock(&connection->send_mutex);

        /* Packetize the message and send it */
        packet_array = packetize(message->buffer, message->length, &array_length);

        for (i = 0; i < array_length; i++) {

            /* Start sending the packetized messages */
            if (net_send_packet(connection->net_connection_handle, &packet_array[i]) <= 0) {
                /* remote side has disconnected */
                connection->alive = 0;
                break;
            }

            /*  ----FIX ME ---- 
             * 
             *  1. wait for the recv thread to notify you of when a NACK or
             *     an ACK has been received
             *  2. check the data structure for this connection to determine
             *     if an ACK or NACK was received.  (You'll have to add the
             *     necessary fields yourself)
             *  3. If it was an ACK, continue sending the packets.
             *  4. If it was a NACK, resend the last packet
             */
            pthread_mutex_lock(&connection->ack_mutex);
            while(connection->ack == 0) pthread_cond_wait(&connection->ack_cond, &connection->ack_mutex);
            if (connection -> ack == 2) i--;
            connection->ack = 0;
            pthread_mutex_unlock(&connection->ack_mutex);

        }

        free(packet_array);
        free(message->buffer);
        free(message);
    } while (connection->alive == 1);
    return NULL;


}

static rtp_connection_t *rtp_init_connection(int net_connection_handle) {
    rtp_connection_t *rtp_connection = malloc(sizeof(rtp_connection_t));

    if (rtp_connection == NULL) {
        fprintf(stderr, "Out of memory!\n");
        exit(EXIT_FAILURE);
    }

    rtp_connection->net_connection_handle = net_connection_handle;

    queue_init(&rtp_connection->recv_queue);
    queue_init(&rtp_connection->send_queue);

    pthread_mutex_init(&rtp_connection->ack_mutex, NULL);
    pthread_mutex_init(&rtp_connection->recv_mutex, NULL);
    pthread_mutex_init(&rtp_connection->send_mutex, NULL);
    pthread_cond_init(&rtp_connection->ack_cond, NULL);
    pthread_cond_init(&rtp_connection->recv_cond, NULL);
    pthread_cond_init(&rtp_connection->send_cond, NULL);

    rtp_connection->alive = 1;

    pthread_create(&rtp_connection->recv_thread, NULL, rtp_recv_thread,
                   (void *) rtp_connection);
    pthread_create(&rtp_connection->send_thread, NULL, rtp_send_thread,
                   (void *) rtp_connection);

    return rtp_connection;
}

/* ================================================================ */
/*                           R T P    A P I                         */
/* ================================================================ */

rtp_connection_t *rtp_connect(char *host, int port) {

    int net_connection_handle;

    if ((net_connection_handle = net_connect(host, port)) < 1)
        return NULL;

    return (rtp_init_connection(net_connection_handle));
}

int rtp_disconnect(rtp_connection_t *connection) {

    message_t *message;
    packet_t term;

    term.type = TERM;
    term.payload_length = term.checksum = 0;
    net_send_packet(connection->net_connection_handle, &term);
    connection->alive = 0;

    net_disconnect(connection->net_connection_handle);
    pthread_cond_signal(&connection->send_cond);
    pthread_cond_signal(&connection->recv_cond);
    pthread_join(connection->send_thread, NULL);
    pthread_join(connection->recv_thread, NULL);
    net_release(connection->net_connection_handle);

    /* emtpy recv queue and free allocated memory */
    while ((message = queue_extract(&connection->recv_queue)) != NULL) {
        free(message->buffer);
        free(message);
    }
    queue_release(&connection->recv_queue);

    /* emtpy send queue and free allocated memory */
    while ((message = queue_extract(&connection->send_queue)) != NULL) {
        free(message);
    }
    queue_release(&connection->send_queue);

    free(connection);

    return 1;

}

int rtp_recv_message(rtp_connection_t *connection, char **buffer, int *length) {

    message_t *message;

    if (connection->alive == 0)
        return -1;
    /* lock */
    pthread_mutex_lock(&connection->recv_mutex);
    while (queue_size(&connection->recv_queue) == 0 && connection->alive == 1) {
        pthread_cond_wait(&connection->recv_cond, &connection->recv_mutex);
    }

    if (connection->alive == 0) {
        pthread_mutex_unlock(&connection->recv_mutex);
        return -1;
    }

    /* extract */
    message = queue_extract(&connection->recv_queue);
    *buffer = message->buffer;
    *length = message->length;
    free(message);

    /* unlock */
    pthread_mutex_unlock(&connection->recv_mutex);

    return *length;
}

int rtp_send_message(rtp_connection_t *connection, char *buffer, int length) {

    message_t *message;

    if (connection->alive == 0)
        return -1;

    message = malloc(sizeof(message_t));
    if (message == NULL) {
        return -1;
    }
    message->buffer = malloc((size_t) length);
    message->length = length;

    if (message->buffer == NULL) {
        free(message);
        return -1;
    }

    memcpy(message->buffer, buffer, (size_t) length);

    /* lock */
    pthread_mutex_lock(&connection->send_mutex);

    /* add */
    queue_add(&(connection->send_queue), message);

    /* unlock */
    pthread_mutex_unlock(&connection->send_mutex);
    pthread_cond_signal(&connection->send_cond);
    return 1;

}
