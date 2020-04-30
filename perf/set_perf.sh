#!/bin/bash

sudo perf probe -a '__x64_sys_read%return $retval'
sudo perf probe -a '__x64_sys_write%return $retval'
sudo perf probe -a 'tcp_v4_do_rcv'
sudo perf probe -a 'tcp_rcv_established:4 th->seq'
sudo perf probe -a '__tcp_transmit_skb:79 th->seq'