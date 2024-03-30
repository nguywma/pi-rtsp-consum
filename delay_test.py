import pika
import subprocess
import time
import json
from datetime import datetime
# Define the function to execute a command

def execute_command(command):
    subprocess.call(command, shell=True)


# Define the callback function to process messages
def callback(ch, method, properties, body):
    message = body.decode('utf-8')
    # Check if the message contains a specific substring
    if "Male" in message:
        # Execute the command if the substring is found
        current_timestamp = datetime.now()
        formatted_timestamp = current_timestamp.strftime('%Y-%m-%d %H:%M:%S.%f')
        print(formatted_timestamp)
        print(message)
        message_dict = json.loads(message)
        a = message_dict['@timestamp']
        element = datetime.strptime(a,"%Y-%m-%dT%H:%M:%S.%fZ")
        print(current_timestamp - element)
        execute_command("ffmpeg -rtsp_transport tcp -i rtsp://admin:ivsr2019@192.168.0.103 -t 10 -c copy output.mp4")
        after_exec = datetime.now()
        after_exec_formatted = after_exec.strftime('%Y-%m-%d %H:%M:%S.%f')
        print(after_exec_formatted)


        
    #time.sleep(2)

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

