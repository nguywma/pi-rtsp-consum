#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#define SOURCE_HOSTNAME "localhost"
#define SOURCE_PORT 5672
#define SOURCE_QUEUE_NAME "my_queue"
#define DESTINATION_HOSTNAME "second_rabbitmq_server"
#define DESTINATION_PORT 5672
#define DESTINATION_QUEUE_NAME "forwarded_queue"

void executeCommand(const char *message) {
    if (strstr(message, "execute") != NULL) {
        system("your_command_here");
    }
}

void forwardMessage(const char *message, amqp_connection_state_t dest_conn) {
    executeCommand(message); // Execute command based on message content

    amqp_bytes_t message_body = amqp_cstring_bytes(message);
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("text/plain");
    props.delivery_mode = 2; // Persistent delivery mode

    amqp_basic_publish(dest_conn,
                       1,
                       amqp_cstring_bytes(""),
                       amqp_cstring_bytes(DESTINATION_QUEUE_NAME),
                       0,
                       0,
                       &props,
                       message_body);
}

int main() {
    // Source RabbitMQ server connection
    amqp_connection_state_t source_conn;
    // Destination RabbitMQ server connection
    amqp_connection_state_t dest_conn;
    // Socket for source connection
    amqp_socket_t *socket = NULL;
    // Reply object for source connection
    amqp_rpc_reply_t reply;
    // Frame object for source connection
    amqp_frame_t frame;
    // Delivery object for source connection
    amqp_basic_deliver_t *delivery;
    // Result of operations
    int res;

    // Connect to the source RabbitMQ server
    source_conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(source_conn);
    res = amqp_socket_open(socket, SOURCE_HOSTNAME, SOURCE_PORT);
    if (res) {
        fprintf(stderr, "Error opening socket for source connection\n");
        return 1;
    }

    // Establish login and channel on the source RabbitMQ server
    amqp_login(source_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
    amqp_channel_open(source_conn, 1);
    reply = amqp_get_rpc_reply(source_conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Error opening channel on source connection\n");
        return 1;
    }

    // Connect to the destination RabbitMQ server
    dest_conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(dest_conn);
    res = amqp_socket_open(socket, DESTINATION_HOSTNAME, DESTINATION_PORT);
    if (res) {
        fprintf(stderr, "Error opening socket for destination connection\n");
        return 1;
    }

    // Establish login and channel on the destination RabbitMQ server
    amqp_login(dest_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
    amqp_channel_open(dest_conn, 1);
    reply = amqp_get_rpc_reply(dest_conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Error opening channel on destination connection\n");
        return 1;
    }

    // Declare the queue on the destination RabbitMQ server
    amqp_queue_declare(dest_conn, 1, amqp_cstring_bytes(DESTINATION_QUEUE_NAME), 0, 1, 0, 0,
                       amqp_empty_table);
    reply = amqp_get_rpc_reply(dest_conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Error declaring queue on destination connection\n");
        return 1;
    }

    // Main loop to consume messages from the source RabbitMQ server
    while (1) {
        // Wait for a message frame from the source RabbitMQ server
        amqp_maybe_release_buffers(source_conn);
        reply = amqp_simple_wait_frame(source_conn, &frame);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
            fprintf(stderr, "Error waiting for frame from source connection\n");
            return 1;
        }

        // Check if the received frame is a method frame indicating message delivery
        if (frame.frame_type != AMQP_FRAME_METHOD || frame.payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
            continue;
        }

        // Extract delivery information from the frame
        delivery = (amqp_basic_deliver_t *)frame.payload.method.decoded;

        // Wait for the header frame containing the message properties
        reply = amqp_simple_wait_frame(source_conn, &frame);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
            fprintf(stderr, "Error waiting for header frame from source connection\n");
            return 1;
        }

        // Check if the received frame is a header frame
        if (frame.frame_type != AMQP_FRAME_HEADER) {
            fprintf(stderr, "Expected header frame from source connection\n");
            return 1;
        }

        // Extract the message body from the frame
        amqp_bytes_t body_bytes = amqp_data_in_buffer(&frame.payload.properties.body);

        // Allocate memory to store the message body
        char *message = malloc(body_bytes.len + 1);
        memcpy(message, frame.payload.properties.body.bytes, body_bytes.len);
        message[body_bytes.len] = '\0';

        printf("Received message: %s\n", message);

        // Forward the received message to the destination RabbitMQ server
        forwardMessage(message, dest_conn);

        // Free memory allocated for the message body
        free(message);

        // Acknowledge message delivery on the source RabbitMQ server
        amqp_basic_ack(source_conn, 1, delivery->delivery_tag, 0);
    }

    // Close channels and connections
    amqp_channel_close(dest_conn, 1, AMQP_REPLY_SUCCESS);
    amqp_channel_close(source_conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(dest_conn, AMQP_REPLY_SUCCESS);
    amqp_connection_close(source_conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(dest_conn);
    amqp_destroy_connection(source_conn);

    return 0;
}
