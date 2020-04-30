def swap32(x):
    return (((x << 24) & 0xFF000000) |
            ((x <<  8) & 0x00FF0000) |
            ((x >>  8) & 0x0000FF00) |
            ((x >> 24) & 0x000000FF))



perf = open('output.txt', mode='r')
output = open('perf_send_delay.txt', mode='w')

lines = perf.readlines()
perf.close()
n = len(lines)

# Send Buffer Delay
app_data = []
tcp_data = []
for i in range(0, n):
  l = lines[i].lstrip()
  idx = l.find(' ')
  l = l[idx+1:]
  tmp = l.split(':')
  idx = tmp[3].find('=')
  tmp[3] = tmp[3][idx+1:]
  if 'write' in tmp[2]: # __x64_sys_write
    app_data.append([float(tmp[0]), int(tmp[3],16)]) # [timestamp, bytes]
  elif 'transmit' in tmp[2]: # __tcp_transmit_skb
    tmp[3] = swap32(int(tmp[3],16)) # convert big<->little int
    tcp_data.append([float(tmp[0]), tmp[3]]) # [timestamp, seq]
    
start_seq = tcp_data[0][1]
tcp_idx = 0
for d in tcp_data:
  d[1] -= start_seq
  if d[1] == 0:
    tcp_idx += 1
tcp_data = tcp_data[tcp_idx:]

total_buf_delay = 0.0
cnt = 0
app_sent = 0
app_idx = 0
for app_d in app_data:
  app_sent += app_d[1]
  tcp_idx = 0
  while tcp_idx < len(tcp_data):
    tcp_d = tcp_data[tcp_idx]
    if tcp_d[0] > app_d[0] and app_sent > tcp_d[1]:
      # match
      total_buf_delay += (tcp_d[0]-app_d[0])
      cnt += 1
      output.write('{0}\n'.format(tcp_d[0]-app_d[0]))
      tcp_data = tcp_data[tcp_idx:]
      break
    else:
      tcp_idx += 1
  app_idx += 1

output.close()
print("average send buffer delay = {0}".format(total_buf_delay/cnt))

# Receive Buffer Delay
output = open('perf_receive_delay.txt', mode='w')

app_data = []
tcp_data = []
timestamp = 0.0
for i in range(0, n):
  l = lines[i].lstrip()
  idx = l.find(' ')
  l = l[idx+1:]
  tmp = l.split(':')
  idx = tmp[3].find('=')
  if idx >= 0:
    tmp[3] = tmp[3][idx+1:]
  else: # doesn't exist => tcp_v4_do_rcv
    timestamp = float(tmp[0])
    continue
  if 'read' in tmp[2] and len(tcp_data) > 0: # __x64_sys_read
    app_data.append([float(tmp[0]), int(tmp[3],16)]) # [timestamp, bytes]
  elif 'rcv' in tmp[2]: # tcp_rcv_established
    tmp[3] = swap32(int(tmp[3],16)) # convert big<->little int
    tcp_data.append([timestamp, tmp[3]]) # [timestamp, seq]
    data.append([timestamp, 'tcp_v4_do_rcv', tmp[3]])

start_seq = tcp_data[0][1]
new_tcp_data = []
for d in tcp_data:
  d[1] -= start_seq
  if d[1] > 0:
    new_tcp_data.append(d)
tcp_data = new_tcp_data
app_idx = 0
for d in app_data:
  if d[0] <= tcp_data[0][0]:
    app_idx += 1
app_data = app_data[app_idx:]

total_buf_delay = 0.0
cnt = 0
app_read = 0
for app_d in app_data:
  app_read += app_d[1]
  tcp_idx = 0
  while tcp_idx < len(tcp_data):
    tcp_d = tcp_data[tcp_idx]
    if tcp_d[0] > app_d[0]:
      break
    if app_read <= tcp_d[1]:  # app_read < tcp_d[1]
      # match
      total_buf_delay += (app_d[0]-tcp_d[0])
      cnt += 1
      output.write('{0}\n'.format(app_d[0]-tcp_d[0]))
      break
    else:
      tcp_idx += 1
  tcp_data = tcp_data[tcp_idx:]

output.close()
print("average receive buffer delay = {0}".format(total_buf_delay/cnt))
