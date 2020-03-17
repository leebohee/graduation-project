#include <arpa/inet.h>
#include <linux/tcp.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <mutex>
#include <queue>
#include <vector>

#define BUF_LEN 60000

using namespace std;

struct sendInfo {
  uint64_t total_bytes;
  clock_t send_time;
  sendInfo(uint64_t bytes, clock_t t) : total_bytes(bytes), send_time(t){};
};
queue<struct sendInfo> send_info;

int sock;       // socket
clock_t start;  // start time
mutex m;
pthread_t tid[2];
FILE* fp = NULL;  // output file

void sig_handler(int sig) {
  printf("Received SIGINT\n");
  pthread_cancel(tid[0]);
  pthread_cancel(tid[1]);
  close(sock);
  fclose(fp);
  exit(0);
}

void* sender(void* arg) {
  char* buf = (char*)malloc(sizeof(char) * BUF_LEN);
  memset(buf, '1', sizeof(char) * BUF_LEN);
  uint64_t seq = 0;  // cumulative # of bytes sent at application layer
  int n;

  while (1) {
    m.lock();
    n = write(sock, buf, strlen(buf));
    seq += n;
    struct sendInfo entry(seq, clock());
    send_info.push(entry);
    m.unlock();
    fprintf(fp, "[ SENDER ] elapsed time = %lf, total bytes sent = %llu\n",
            (double)(send_info.back().send_time - start) / CLOCKS_PER_SEC,
            send_info.back().total_bytes);
  }
  return NULL;
}

void* tracker(void* arg) {
  struct tcp_info info;
  uint64_t bytes_sent;
  socklen_t len = sizeof(info);
  double cur, D;
  while (1) {
    // get socket data
    if (getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, &len)) {
      printf("getsockopt() failed: Can't get TCP information\n");
      return NULL;
    }

    bytes_sent =
        info.tcpi_bytes_acked + (info.tcpi_unacked * info.tcpi_snd_mss);
    m.lock();
    // find matching sendInfo
    while (!send_info.empty()) {
      if (send_info.front().total_bytes > bytes_sent) {
        break;
      } else {
        D = (double)(clock() - send_info.front().send_time) / CLOCKS_PER_SEC;
        send_info.pop();
      }
    }
    // print information
    if (!send_info.empty()) {
      cur = (double)(clock() - start) / CLOCKS_PER_SEC;
      fprintf(
          fp,
          "[ TRACKER ] qsize = %d, total bytes = %llu, elapsed time = %lfs, "
          "buffer delay = %lfs, "
          "congestion "
          "window size = %d, threshold = %d, rtt = %lfs\n",
          send_info.size(), bytes_sent, cur, D, info.tcpi_snd_cwnd,
          info.tcpi_snd_ssthresh, (double)info.tcpi_rtt / 1000);
    }
    m.unlock();

    // sleep for 10msec
    usleep(10000);
  }
  return NULL;
}

int main(int argc, char* argv[]) {
  int n, result;
  char* haddr;
  struct sockaddr_in server_addr;

  fp = fopen("output.txt", "w");
  signal(SIGINT, sig_handler);  // Ctrl+c

  if (argc != 2) {
    printf("usage : Need destination address as an argument\n");
    return -1;
  }
  haddr = argv[1];

  if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    printf("socket() failed: Can't create socket\n");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(argv[1]);
  server_addr.sin_port = htons(5001);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    printf("connect() failed: Can't connect.\n");
    return -1;
  }

  start = clock();
  pthread_create(&tid[0], NULL, sender, NULL);
  pthread_create(&tid[1], NULL, tracker, NULL);
  pthread_join(tid[0], (void**)&result);
  pthread_join(tid[1], (void**)&result);
  return 0;
}