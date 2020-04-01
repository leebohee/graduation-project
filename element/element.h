#include <cstddef>
#include <cstdio>

class RetInfo {
 public:
  RetInfo(ssize_t size, float buf_delay, float throughput, float rtt, int cwnd)
      : size_(size),
        buf_delay_(buf_delay),
        throughput_(throughput),
        rtt_(rtt),
        cwnd_(cwnd){};
  void print(FILE* fp) {
    fprintf(fp,
            "size = %ld, buffer delay = %f, throughput = %f, rtt = %lf, "
            "congestion window = %d\n",
            size_, buf_delay_, throughput_, rtt_, cwnd_);
  }

  ssize_t get_size() { return size_; }
  float get_buf_delay() { return buf_delay_; }
  float get_throughput() { return throughput_; }
  int get_cwnd() { return cwnd_; }

 private:
  ssize_t size_;      // size of written/read bytes
  float buf_delay_;   // measured buffer delay
  float throughput_;  // throughput at TCP layer
  float rtt_;         // RTT
  int cwnd_;          // congestion window size
};

// Initialize ELEMENT with the given socket
int init_em(int fd);

// Write data stored in the buf up to 'count' bytes into the given file
// descriptor
RetInfo em_write(int fd, void* buf, size_t count);
// Read data up to 'count' bytes from the given file descriptor and store it in
// the buf
RetInfo em_read(int fd, void* buf, size_t count);

// Finalize ELEMENT
void fin_em();