# SKKU Graduation Project

## Index
1. [Project Title](#project-title)
2. [Background Knowledge](#background)
3. [Naive ELEMENT](#naive-element)
4. [ELEMENT](#element)

<a name="project-title"/>

## Project Title
Implement [ELEMENT](https://netstech.org/wp-content/uploads/2019/06/element-eurosys19.pdf), a end-to-end latency diagnosis framework.

<a name="background"/>

## Background Knowledge
#### ELEMENT
#### tcp_info
We can get basic TCP statistics using ```getsockopt()``` with TCP_INFO option. After the function returns successfully, we can access the information in ```tcp_info```. It enables the ELEMENT runs in the user-space.

```tcp_info``` is defined in **_/lib/modules/'uname -r'/build/include/uapi/linux/tcp.h_**.

```
struct tcp_info {
	__u8	tcpi_state;
	__u8	tcpi_ca_state;
	__u8	tcpi_retransmits;
	__u8	tcpi_probes;
	__u8	tcpi_backoff;
	__u8	tcpi_options;
	__u8	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;
	__u8	tcpi_delivery_rate_app_limited:1;

	__u32	tcpi_rto;
	__u32	tcpi_ato;
	__u32	tcpi_snd_mss;
	__u32	tcpi_rcv_mss;

	__u32	tcpi_unacked;
	__u32	tcpi_sacked;
	__u32	tcpi_lost;
	__u32	tcpi_retrans;
	__u32	tcpi_fackets;

	/* Times. */
	__u32	tcpi_last_data_sent;
	__u32	tcpi_last_ack_sent;     /* Not remembered, sorry. */
	__u32	tcpi_last_data_recv;
	__u32	tcpi_last_ack_recv;

	/* Metrics. */
	__u32	tcpi_pmtu;
	__u32	tcpi_rcv_ssthresh;
	__u32	tcpi_rtt;
	__u32	tcpi_rttvar;
	__u32	tcpi_snd_ssthresh;
	__u32	tcpi_snd_cwnd;
	__u32	tcpi_advmss;
	__u32	tcpi_reordering;

	__u32	tcpi_rcv_rtt;
	__u32	tcpi_rcv_space;

	__u32	tcpi_total_retrans;

	__u64	tcpi_pacing_rate;
	__u64	tcpi_max_pacing_rate;
	__u64	tcpi_bytes_acked;    /* RFC4898 tcpEStatsAppHCThruOctetsAcked */
	__u64	tcpi_bytes_received; /* RFC4898 tcpEStatsAppHCThruOctetsReceived */
	__u32	tcpi_segs_out;	     /* RFC4898 tcpEStatsPerfSegsOut */
	__u32	tcpi_segs_in;	     /* RFC4898 tcpEStatsPerfSegsIn */

	__u32	tcpi_notsent_bytes;
	__u32	tcpi_min_rtt;
	__u32	tcpi_data_segs_in;	/* RFC4898 tcpEStatsDataSegsIn */
	__u32	tcpi_data_segs_out;	/* RFC4898 tcpEStatsDataSegsOut */

	__u64   tcpi_delivery_rate;

	__u64	tcpi_busy_time;      /* Time (usec) busy sending data */
	__u64	tcpi_rwnd_limited;   /* Time (usec) limited by receive window */
	__u64	tcpi_sndbuf_limited; /* Time (usec) limited by send buffer */

	__u32	tcpi_delivered;
	__u32	tcpi_delivered_ce;

	__u64	tcpi_bytes_sent;     /* RFC4898 tcpEStatsPerfHCDataOctetsOut */
	__u64	tcpi_bytes_retrans;  /* RFC4898 tcpEStatsPerfOctetsRetrans */
	__u32	tcpi_dsack_dups;     /* RFC4898 tcpEStatsStackDSACKDups */
	__u32	tcpi_reord_seen;     /* reordering events seen */
};
```
Among these, we need only a few information. 
* ```tcpi_bytes_acked``` : How many bytes were acked
* ```tcpi_unacked``` : Total number of segments unacknowledged
* ```tcpi_snd_mss``` : Maximum Segment Size (MSS) when sending
* ```tcpi_segs_in``` : Total number of segements received
* ```tcpi_rcv_mss``` : Maximum Segment Size (MSS) when receiving
* ```tcpi_rtt``` : Round Trip Time (RTT) in microseconds
* ```tcpi_snd_cwnd``` : Sending congestion window
* ```tcpi_snd_ssthresh``` : Slow start size threshold

<a name="naive-element"/>

## Naive ELEMENT
I implement a sender/receiver with naive ELEMENT in [naive/](https://github.com/leebohee/graduation-project/tree/master/naive) directory as described in the paper. They just calculate buffer delay while sending/receiving data. 
#### Sender
It connects to the host with the given IP address and runs two threads, sender and tracker. These two threads share a **queue** which stores information used for calculating a buffer delay. The sender thread sends data to the host and store an entry with total bytes sent and timestamp in the queue. The tracker thread periodically gets TCP information and estimates bytes sent at TCP layer using this information. Then it calculates the buffer delay by matching estimated bytes with total bytes of an entry in the queue. The calculated delay is recorded in output file. This program terminates when receiving ```Ctrl+C```. 
#### Receiver
It creates connection with any client and run two threads, receiver and tracker. Like the sender program, they have a shared **queue** and work similarly. What is different is, the tracker stores entry in the queue and the receiver calculates a buffer delay. The tracker thread periodically gets TCP information and estimates total received bytes. It stores estimated bytes and timestamp in the queue only when the process receives more data than the previous. After the receiver thread receives data, it matches total bytes received at the application layer with total bytes of an entry in the queue. If it finds the proper entry, it calculates the buffer delay and records it in the output file. This program also terminates with ```Ctrl+C``` signal.

<a name="element"/>

## ELEMENT
I implement some APIs to utilize ELEMENT in [element/](https://github.com/leebohee/graduation-project/tree/master/element) directory. There are wrapper functions for ```write()``` and ```read()```. They return an instance of ```RetInfo```, which has size of written/read data, buffer delay, RTT, throughput and congestion window size measured by ELEMENT. You can use these APIs by including the [header file](https://github.com/leebohee/graduation-project/tree/master/element/element.h).