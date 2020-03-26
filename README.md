# SKKU Graduation Project

## Index
1. [Project Title](#project-title)
2. [Background Knowledge](#background)
3. [Naive ELEMENT](#naive-element)

<a name="project-title"/>

## Project Title
Implement [ELEMENT](https://netstech.org/wp-content/uploads/2019/06/element-eurosys19.pdf), a end-to-end latency diagnosis framework.

<a name="background"/>

## Background Knowledge
#### ELEMENT
#### tcp_info

<a name="naive-element"/>

## Naive ELEMENT
I implement a sender/receiver with naive ELEMENT in [naive/](https://github.com/leebohee/graduation-project/tree/master/naive) directory as described in the paper. They just calculate buffer delay while sending/receiving data. 
#### Sender
It connects to the host with the given IP address and runs two threads, sender and tracker. These two threads share a **queue** which stores information used for calculating a buffer delay. The sender thread sends data to the host and store an entry with total bytes sent and timestamp in the queue. The tracker thread periodically gets TCP information and estimates bytes sent at TCP layer using this information. Then it calculates the buffer delay by matching estimated bytes with total bytes of an entry in the queue. The calculated delay is recorded in output file. This program terminates when receiving ```Ctrl+C```. 
#### Receiver
It creates connection with any client and run two threads, receiver and tracker. Like the sender program, they have a shared **queue** and work similarly. What is different is, the tracker stores entry in the queue and the receiver calculates a buffer delay. The tracker thread periodically gets TCP information and estimates total received bytes. It stores estimated bytes and timestamp in the queue only when the process receives more data than the previous. After the receiver thread receives data, it matches total bytes received at the application layer with total bytes of an entry in the queue. If it finds the proper entry, it calculates the buffer delay and records it in the output file. This program also terminates with ```Ctrl+C``` signal.
