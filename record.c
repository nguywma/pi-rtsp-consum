#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#define HOSTNAME "localhost"
#define PORT 5672
#define QUEUE_NAME "my_queue"

void executeCommand(const char *message) {
    // Example condition: execute command if message contains "execute"
    if (strstr(message, "execute") != NULL) {
        // Replace the command with your desired command
        system("your_command_here");
    }
}

int main() {
    amqp_connection_state_t conn;
    amqp_socket_t *socket = NULL;
    amqp_rpc_reply_t reply;
    amqp_frame_t frame;
    amqp_bytes_t queuename;
    amqp_basic_deliver_t *delivery;
    int res;

    conn = amqp_new_connection();

    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        fprintf(stderr, "Error creating TCP socket\n");
        return 1;
    }

    res = amqp_socket_open(socket, HOSTNAME, PORT);
    if (res) {
        fprintf(stderr, "Error opening socket\n");
        return 1;
    }

    amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
    amqp_channel_open(conn, 1);
    reply = amqp_get_rpc_reply(conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Error opening channel\n");
        return 1;
    }

    queuename = amqp_cstring_bytes(QUEUE_NAME);
    amqp_basic_consume(conn, 1, queuename, amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Error consuming queue\n");
        return 1;
    }

    while (1) {
        amqp_maybe_release_buffers(conn);
        reply = amqp_simple_wait_frame(conn, &frame);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
            fprintf(stderr, "Error waiting for frame\n");
            return 1;
        }

        if (frame.frame_type != AMQP_FRAME_METHOD) {
            continue;
        }

        if (frame.payload.method.id == AMQP_BASIC_DELIVER_METHOD) {
            delivery = (amqp_basic_deliver_t *)frame.payload.method.decoded;
            amqp_bytes_t body_bytes;
            reply = amqp_simple_wait_frame(conn, &frame);
            if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
                fprintf(stderr, "Error waiting for frame\n");
                return 1;
            }
            if (frame.frame_type != AMQP_FRAME_HEADER) {
                fprintf(stderr, "Expected header frame\n");
                return 1;
            }

            body_bytes = amqp_data_in_buffer(&frame.payload.properties.body);

            char *message = malloc(body_bytes.len + 1);
            memcpy(message, frame.payload.properties.body.bytes, body_bytes.len);
            message[body_bytes.len] = '\0';

            printf("Received message: %s\n", message);

            executeCommand(message); // Execute command based on message content

            free(message);

            amqp_basic_ack(conn, 1, delivery->delivery_tag, 0);
        }
    }

    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);

    return 0;
}
