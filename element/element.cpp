#include "element.h"
#include <linux/tcp.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <queue>

using namespace std;

pthread_t tid;
mutex m;
chrono::system_clock::time_point start;
uint32_t cwnd = 0, rtt = 0;
float throughput = 0.0, throughput1;
chrono::duration<double> snd_buf_delay, rcv_buf_delay;
int sock;

struct sendInfo {
  uint64_t total_bytes;
  chrono::system_clock::time_point send_time;
  sendInfo(uint64_t bytes, chrono::system_clock::time_point t)
      : total_bytes(bytes), send_time(t){};
};
queue<struct sendInfo> send_info;

struct recvInfo {
  uint64_t total_bytes;
  chrono::system_clock::time_point recv_time;
  recvInfo(uint64_t bytes, chrono::system_clock::time_point t)
      : total_bytes(bytes), recv_time(t){};
};
queue<struct recvInfo> recv_info;

void* tracker(void* arg) {
  struct tcp_info info;
  uint64_t bytes_sent, bytes_recv, prev_bytes_recv = 0;
  socklen_t len = sizeof(info);

  while (true) {
    // get socket data
    if (getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, &len)) {
      printf("getsockopt() failed: Can't get TCP information\n");
      return NULL;
    }

    bytes_sent =
        info.tcpi_bytes_acked + (info.tcpi_unacked * info.tcpi_snd_mss);
    bytes_recv = info.tcpi_segs_in * info.tcpi_rcv_mss;

    cwnd = info.tcpi_snd_cwnd;
    rtt = info.tcpi_rtt;
    throughput = ((float)cwnd * 1000 / (float)rtt) * info.tcpi_snd_mss * 8;

    m.lock();
    // find matching sendInfo
    while (!send_info.empty()) {
      if (send_info.front().total_bytes > bytes_sent) {
        break;
      } else {
        snd_buf_delay =
            chrono::system_clock::now() - send_info.front().send_time;
        send_info.pop();
      }
    }
    // add recvInfo entry
    if (bytes_recv > prev_bytes_recv) {
      prev_bytes_recv = bytes_recv;
      struct recvInfo entry(bytes_recv, chrono::system_clock::now());
      recv_info.push(entry);
    }
    m.unlock();

    // sleep for 10msec
    usleep(10000);
  }
  return nullptr;
}

int init_em(int fd) {
  sock = fd;
  start = chrono::system_clock::now();
  pthread_create(&tid, NULL, tracker, NULL);
  pthread_detach(tid);
  return 0;
}

RetInfo em_write(int fd, void* buf, size_t count) {  // sender thread
  static uint64_t seq = 0;  // cumulative # of bytes sent at application layer
  int n;

  // add sendInfo entry
  m.lock();
  n = write(fd, buf, count);
  seq += n;
  struct sendInfo entry(seq, chrono::system_clock::now());
  send_info.push(entry);
  // calculate buffer delay
  RetInfo ret(n, snd_buf_delay.count(), throughput, (float)rtt / 1000, cwnd);
  m.unlock();

  return ret;
}

RetInfo em_read(int fd, void* buf, size_t count) {  // receiver thread
  // cumulative # of bytes received at application layer
  static uint64_t seq = 0;
  int n;

  m.lock();
  n = read(fd, buf, count);
  if (n <= 0) {
    RetInfo ret(0, 0.0, 0, 0.0, 0);
    return ret;
  }
  seq += n;
  // find matching recvInfo
  while (!recv_info.empty()) {
    if (recv_info.front().total_bytes > seq) {
      rcv_buf_delay = chrono::system_clock::now() - recv_info.front().recv_time;
      break;
    } else {
      recv_info.pop();
    }
  }
  m.unlock();
  RetInfo ret(n, rcv_buf_delay.count(), throughput, (float)rtt / 1000, cwnd);
  return ret;
}

void fin_em() {}