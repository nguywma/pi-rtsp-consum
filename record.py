import pika
import subprocess
import time
# Define the function to execute a command
def execute_command(command):
    subprocess.call(command, shell=True)

# Define the callback function to process messages
def callback(ch, method, properties, body):
    message = body.decode('utf-8')
    # Check if the message contains a specific substring
    if "Male" in message:
        # Execute the command if the substring is found
        execute_command("ls")
    time.sleep(30)

# Establish connection with RabbitMQ server
connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
channel = connection.channel()

# Declare the queue to consume messages from with durable=False
#channel.queue_declare(queue='message', durable=False)

# Specify the callback function to consume messages
channel.basic_consume(queue='message', on_message_callback=callback, auto_ack=True)

# Start consuming messages
print('Waiting for messages...')
channel.start_consuming()

