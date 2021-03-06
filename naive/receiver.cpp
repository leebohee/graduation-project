#include <arpa/inet.h>
#include <linux/tcp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>

#define BUF_LEN 4096
#define MICRO 1000000

using namespace std;

struct recvInfo {
  uint64_t total_bytes;
  chrono::system_clock::time_point recv_time;
  recvInfo(uint64_t bytes, chrono::system_clock::time_point t)
      : total_bytes(bytes), recv_time(t){};
};
queue<struct recvInfo> recv_info;

int conn_sock;  // socket
chrono::system_clock::time_point start;
mutex m;
pthread_t tid[2];
ofstream fout("output_receiver.txt");  // output file
chrono::duration<double> buf_delay;

void sig_handler(int sig) {
  printf("Received SIGINT\n");
  pthread_cancel(tid[0]);
  pthread_cancel(tid[1]);
  close(conn_sock);
  fout.close();
  exit(0);
}

void* receiver(void* arg) {
  char* buf = (char*)malloc(sizeof(char) * BUF_LEN);
  uint64_t seq = 0;  // cumulative # of bytes received at application layer
  int n;
  chrono::duration<double> cur, D;

  while (1) {
    n = read(conn_sock, buf, BUF_LEN);
    if (n <= 0) return NULL;
    seq += n;
    m.lock();
    // find matching recvInfo
    while (!recv_info.empty()) {
      if (recv_info.front().total_bytes > seq) {
        buf_delay = chrono::system_clock::now() - recv_info.front().recv_time;
        break;
      } else {
        recv_info.pop();
      }
    }
    m.unlock();
  }
  return NULL;
}

void* tracker(void* arg) {
  struct tcp_info info;
  uint64_t bytes_recv, prev_bytes_recv = 0;
  socklen_t len = sizeof(info);
  chrono::duration<double> cur;
  while (1) {
    m.lock();
    // get socket data
    if (getsockopt(conn_sock, IPPROTO_TCP, TCP_INFO, &info, &len)) {
      printf("getsockopt() failed: Can't get TCP information\n");
      return NULL;
    }
    bytes_recv = info.tcpi_segs_in * info.tcpi_rcv_mss;
    if (bytes_recv > prev_bytes_recv) {
      prev_bytes_recv = bytes_recv;
      // add entry
      struct recvInfo entry(bytes_recv, chrono::system_clock::now());
      recv_info.push(entry);
      cur = chrono::system_clock::now() - start;
      fout << "[ ELEMENT ] " << cur.count()
           << " : buffer delay = " << buf_delay.count() << " sec"
           << " rtt = " << (float)info.tcpi_rtt / MICRO << " sec\n";
    }
    m.unlock();
    // sleep for 10msec
    usleep(10000);
  }
  return NULL;
}

int main(int argc, char* argv[]) {
  int result, caddr_len, sock;
  struct sockaddr_in server_addr, client_addr;

  signal(SIGINT, sig_handler);  // Ctrl+c

  if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    printf("socket() failed: Can't create socket\n");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(5001);

  if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    printf("bind() failed: Can't bind.\n");
    return -1;
  }

  if (listen(sock, 5) < 0) {
    printf("listen() failed: Can't wait for upcoming socket request.\n");
    return -1;
  }

  caddr_len = sizeof(client_addr);
  if ((conn_sock = accept(sock, (struct sockaddr*)&client_addr,
                          (unsigned int*)&caddr_len)) < 0) {
    printf("accept() failed: Can't connect client.\n");
    return -1;
  }
  printf("Server connected.\n");
  start = chrono::system_clock::now();
  pthread_create(&tid[0], NULL, receiver, NULL);
  pthread_create(&tid[1], NULL, tracker, NULL);
  pthread_join(tid[0], (void**)&result);
  pthread_join(tid[1], (void**)&result);
  return 0;
}
