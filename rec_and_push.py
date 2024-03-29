import pika
import subprocess

# Define the function to execute a command
def execute_command(command):
    subprocess.call(command, shell=True)

# Define the callback function to process messages
def callback(ch, method, properties, body):
    message = body.decode('utf-8')
    # Check if the message contains a specific substring
    if "substring_to_check" in message:
        # Execute the command if the substring is found
        execute_command("your_command_to_execute")
    # Forward the message to another RabbitMQ server
    forward_message(body)

# Function to forward message to another RabbitMQ server
def forward_message(body):
    # Establish connection to the second RabbitMQ server
    connection = pika.BlockingConnection(pika.ConnectionParameters('second_server_ip'))
    channel = connection.channel()

    # Declare a queue to publish forwarded messages
    channel.queue_declare(queue='forwarded_queue_name')

    # Publish the message to the second server
    channel.basic_publish(exchange='',
                          routing_key='forwarded_queue_name',
                          body=body)

    print("Message forwarded to the second RabbitMQ server")

    # Close connection
    connection.close()

# Establish connection with the first RabbitMQ server
connection = pika.BlockingConnection(pika.ConnectionParameters('first_server_ip'))
channel = connection.channel()

# Declare the queue to consume messages from with durable=False
channel.queue_declare(queue='your_queue_name', durable=False)

# Specify the callback function to consume messages
channel.basic_consume(queue='your_queue_name', on_message_callback=callback, auto_ack=True)

# Start consuming messages
print('Waiting for messages...')
channel.start_consuming()
