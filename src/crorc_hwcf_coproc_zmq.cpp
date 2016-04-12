/**
 *  crorc_hwcf_coproc_zmq.cpp
 *  Copyright (C) 2016 Heiko Engel <hengel@cern.ch>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <errno.h>
#include <zmq.h>
#include <list>
#include "crorc_hwcf_coproc_handler.cpp"

#define HELP_TEXT                                                              \
  "usage: crorc_hwcf_coproc [parameters]\n"                                    \
  "    -n [Id]          Device ID, default:0\n"                                \
  "    -c [Id]          Channel ID, default:0\n"                               \
  "    -p [tpcPatch]    TPC Patch ID, default:0\n"                             \
  "    -r [rcuVersion]  TPC RCU version, default:1\n"                          \
  "    -m [mappingfile] Path to AliRoot TPC Row Mapping File\n"

#define ES2HOST_EB_ID 0
#define ES2DEV_EB_ID 2
#define EB_SIZE 0x40000000 // 1GB
//#define EB_SIZE 0x800000 // 8MB

#define ZMQ_BASE_PORT 5555
#define ZMQ_BUF_SIZE (3 * 4096)

using namespace std;

/**
 * Prototypes
 **/
int configureFcf(librorc::fastclusterfinder *fcf, char *mapping);
std::vector<librorc::ScatterGatherEntry> eventToBuffer(const char *filename,
                                                       librorc::buffer *buffer);
int eventToDisk(const char *filename, librorc::EventDescriptor *report,
                const uint32_t *event);
int compareToRef(const char *filename, librorc::EventDescriptor *report,
                 const uint32_t *event);
void cleanup(crorc_hwcf_coproc_handler **stream, librorc::bar **bar,
             librorc::device **dev, void **zmq_ctx, void **zmq_skt);
void checkHwcfFlags(librorc::EventDescriptor *report,
                    const char *outputFileName);

inline long long timediff_us(struct timeval from, struct timeval to) {
  return ((long long)(to.tv_sec - from.tv_sec) * 1000000LL +
          (long long)(to.tv_usec - from.tv_usec));
}

bool done = false;
// Signal handler
void abort_handler(int s) {
  cerr << "Caught signal " << s << endl;
  if (done == true) {
    exit(-1);
  } else {
    done = true;
  }
}

/**
 * main
 **/
int main(int argc, char *argv[]) {
  int deviceId = 0;
  int channelId = 0;
  char *mappingfile = NULL;
  uint32_t tpcPatch = 0;
  uint32_t rcuVersion = 1;
  int arg;
  while ((arg = getopt(argc, argv, "hn:c:m:p:r:")) != -1) {
    switch (arg) {
    case 'h':
      cout << HELP_TEXT;
      return 0;
    case 'n':
      deviceId = strtoul(optarg, NULL, 0);
      break;
    case 'c':
      channelId = strtoul(optarg, NULL, 0);
      break;
    case 'p':
      tpcPatch = strtoul(optarg, NULL, 0);
      break;
    case 'r':
      rcuVersion = strtoul(optarg, NULL, 0);
      break;
    case 'm':
      mappingfile = optarg;
      break;
    }
  }

  if (!mappingfile) {
    cerr << "ERROR: no FCF mapping file provided!" << endl;
    cout << HELP_TEXT;
    return -1;
  }

  crorc_hwcf_coproc_handler *stream = NULL;
  librorc::device *dev = NULL;
  librorc::bar *bar = NULL;
  void *zmq_ctx = NULL;
  void *zmq_skt = NULL;

  zmq_ctx = zmq_ctx_new();
  zmq_skt = zmq_socket(zmq_ctx, ZMQ_PULL);
  if (!zmq_skt) {
    perror("zmq_skt");
    cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
    return -1;
  }

  int zmq_port = ZMQ_BASE_PORT + channelId;
  char zmq_bind_addr[1024];
  snprintf(zmq_bind_addr, 1024, "tcp://*:%d", zmq_port);
  if (zmq_bind(zmq_skt, zmq_bind_addr)) {
    perror("zmq_bind");
    cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
    return -1;
  }

  zmq_pollitem_t zmq_pollitem;
  zmq_pollitem.socket = zmq_skt;
  zmq_pollitem.events = ZMQ_POLLIN;

  try {
    dev = new librorc::device(deviceId);
    bar = new librorc::bar(dev, 1);
  } catch (int e) {
    cerr << "ERROR: failed to initialize C-RORC: " << librorc::errMsg(e)
         << endl;
    cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
    return -1;
  }

  try {
    stream = new crorc_hwcf_coproc_handler(dev, bar, channelId);
  } catch (int e) {
    cerr << "ERROR: failed to initialize C-RORC: " << librorc::errMsg(e)
         << endl;
    cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
    return -1;
  }

  int result = stream->initializeDmaToHost(EB_SIZE);
  if (result < 0) {
    cerr << "ERROR: Failed to intialize DMA to Host: "
         << librorc::errMsg(result) << endl;
    cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
    return -1;
  }
  result = stream->initializeDmaToDevice(EB_SIZE);
  if (result < 0) {
    cerr << "ERROR: Failed to intialize DMA to Device: "
         << librorc::errMsg(result) << endl;
    cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
    return -1;
  }
  if (stream->initializeClusterFinder(mappingfile, tpcPatch, rcuVersion)) {
    cerr << "ERROR: Failed to intialize Clusterfinder with mappingfile "
         << mappingfile << endl;
    cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
    return -1;
  }

  list<string> input_file_list, output_file_list, ref_file_list;
  list<string>::iterator input_end, input_iter;
  input_iter = input_file_list.begin();
  input_end = input_file_list.end();

  list<string>::iterator output_end, output_iter;
  output_iter = output_file_list.end();
  output_end = output_file_list.end();

  list<string>::iterator ref_end, ref_iter;
  ref_iter = ref_file_list.end();
  ref_end = ref_file_list.end();

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = abort_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  struct timeval now, last;
  gettimeofday(&now, NULL);
  last = now;

  uint64_t cnt_input_done = 0;
  uint64_t cnt_input_rcvd = 0;
  uint64_t cnt_output_done = 0;

  bool stop = false;

  while (!done) {
    // check ZMQ for new files
    int zmq_ret = zmq_poll(&zmq_pollitem, 1, 0);
    if (zmq_ret < 0) {
      perror("zmq_poll");
      cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
      return -1;
    } else if (zmq_ret > 0) {
      char zmq_buffer[ZMQ_BUF_SIZE];
      memset(zmq_buffer, 0, ZMQ_BUF_SIZE);
      int ret = zmq_recv(zmq_skt, zmq_buffer, ZMQ_BUF_SIZE, 0);
      if (ret == -1) {
        perror("zmq_recv");
        cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
        return -1;
      }
      std::string rcvd = std::string(zmq_buffer);
      if (rcvd.compare(";;;") == 0) {
        stop = true;
        cout << "INFO: stop command received" << endl;
        continue;
      }
      size_t found = rcvd.find_first_of(";");
      if (found == std::string::npos) {
        cerr << "ERROR: Invalid string from ZMQ: %s" << rcvd << endl;
        cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
        return -1;
      }
      std::string inputfile = rcvd.substr(0, found);
      rcvd = rcvd.substr(found + 1, -1);
      found = rcvd.find_first_of(";");
      if (found == std::string::npos) {
        cerr << "ERROR: Invalid string from ZMQ: %s" << rcvd << endl;
        cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
        return -1;
      }
      std::string outputfile = rcvd.substr(0, found);
      std::string reffile = rcvd.substr(found + 1, -1);

      input_file_list.push_back(inputfile);
      input_iter = input_file_list.begin();
      input_end = input_file_list.end();
      cnt_input_rcvd++;

      if (!outputfile.empty()) {
        output_file_list.push_back(outputfile);
        output_iter = output_file_list.begin();
        output_end = output_file_list.end();
      }
      if (!reffile.empty()) {
        ref_file_list.push_back(reffile);
        ref_iter = ref_file_list.begin();
        ref_end = ref_file_list.end();
      }
    }

    if (input_iter != input_end) {
      int result = stream->enqueueEventToDevice(input_iter->c_str());
      if (result && result != EAGAIN) {
        cerr << "ERROR: Failed to enqueue event " << input_iter->c_str()
             << ", failed with: " << strerror(result) << "(" << result << ")"
             << endl;
        delete stream;
        delete bar;
        delete dev;
        return -1;
      } else if (result == 0) {
        input_iter = input_file_list.erase(input_iter);
        cnt_input_done++;
      }
    }

    stream->pollForEventToDeviceCompletion();

    if ((output_iter != output_end) || (ref_iter != ref_end)) {
      librorc::EventDescriptor *report = NULL;
      uint64_t librorcEventReference = 0;
      const uint32_t *event = NULL;
      if (stream->pollForEventToHost(&report, &event, &librorcEventReference)) {
        if (output_iter != output_end) {
          if (event[0] != 0xffffffff) {
            cout << "WARNING: Invalid CDH in " << *output_iter << ", file no. "
                 << cnt_output_done << endl;
          }
          checkHwcfFlags(report, output_iter->c_str());
          eventToDisk(output_iter->c_str(), report, event);
          output_iter = output_file_list.erase(output_iter);
          cnt_output_done++;
        }

        if (ref_iter != ref_end) {
          int result = compareToRef(ref_iter->c_str(), report, event);
          if (result < 0) {
            cerr << "comparing output with " << ref_iter->c_str() << " failed."
                 << endl;
          }
          ref_iter = ref_file_list.erase(ref_iter);
        }
        stream->releaseEventToHost(librorcEventReference);
      }
    }

    if (stop && (input_iter == input_end) && (output_iter == output_end) &&
        (ref_iter == ref_end)) {
      done = true;
    }

    gettimeofday(&now, NULL);
    if (timediff_us(last, now) > 1000000) {
      cout << "Input Received: " << cnt_input_rcvd
           << ", Input Queue: " << input_file_list.size()
           << ", Input done: " << cnt_input_done
           << ", Output done: " << cnt_output_done << endl;
      last = now;
    }
  }

  cleanup(&stream, &bar, &dev, &zmq_ctx, &zmq_skt);
  return 0;
}

#if 0
int checkEvent(librorc::EventDescriptor *report, const uint32_t *event) {
  uint32_t dmaWords = (report->calc_event_size & 0x3fffffff);
  uint32_t diuWords = (report->reported_event_size & 0x3fffffff);
  uint32_t diuErrorFlag = (report->reported_event_size >> 30) & 1;
  uint32_t rcuErrorFlag = (report->calc_event_size >> 31) & 1;
  return 0;
};
#endif

void checkHwcfFlags(librorc::EventDescriptor *report,
                    const char *outputFileName) {
  uint32_t hwcfflags = (report->calc_event_size >> 30) & 0x3;
  if (hwcfflags & 0x1) {
    printf("WARNING: Found RCU protocol error(s) in %s\n", outputFileName);
  }
  if (hwcfflags & 0x2) {
    printf("WARNING: Found ALTRO channel error flag(s) set in %s\n",
           outputFileName);
  }
}

int eventToDisk(const char *filename, librorc::EventDescriptor *report,
                const uint32_t *event) {
  int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    cerr << "ERROR: Failed to open output file " << filename << ": "
         << strerror(errno) << endl;
    return -1;
  }
  uint32_t dmaWords = (report->calc_event_size & 0x3fffffff);
  ssize_t nbytes = write(fd, event, (dmaWords << 2));
  close(fd);
  if (nbytes == -1) {
    cerr << "ERROR: Failed to write event to file " << filename << " - "
         << strerror(errno) << endl;
    return -1;
  } else if (nbytes != (dmaWords << 2)) {
    cerr << "ERROR: Failed to write event to file " << filename << " - "
         << "expected " << nbytes << " bytes, wrote " << (dmaWords << 2)
         << " bytes." << endl;
    return -1;
  }
  return 0;
}

int compareToRef(const char *filename, librorc::EventDescriptor *report,
                 const uint32_t *event) {
  // open DDL file
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    cerr << "WARNING: Failed to open reference file " << filename
         << " - skipping..." << endl;
    return -1;
  }

  // map reference data
  struct stat fd_stat;
  fstat(fd, &fd_stat);
  void *event_ref = mmap(NULL, fd_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (event_ref == MAP_FAILED) {
    cerr << "WARNING: Failed to mmap DDL file " << filename << " - skipping..."
         << endl;
    return -1;
  }
  uint32_t dmaWords = (report->calc_event_size & 0x3fffffff);
  ssize_t eventSize = (dmaWords << 2);

  if (fd_stat.st_size != eventSize) {
    cerr << "Event Size mismatch, got " << eventSize << " bytes, expected "
         << fd_stat.st_size << " bytes from " << filename << endl;
    munmap(event_ref, fd_stat.st_size);
    return -1;
  }

  int result = memcmp(event, event_ref, eventSize);
  if (result != 0) {
    cerr << "Event does not match reference " << filename << endl;
    munmap(event_ref, fd_stat.st_size);
    return -1;
  }
  munmap(event_ref, fd_stat.st_size);
  return 0;
}

void cleanup(crorc_hwcf_coproc_handler **stream, librorc::bar **bar,
             librorc::device **dev, void **zmq_ctx, void **zmq_skt) {
  if (*stream) {
    delete *stream;
    *stream = NULL;
  }
  if (*bar) {
    delete *bar;
    *bar = NULL;
  }
  if (*dev) {
    delete *dev;
    *dev = NULL;
  }
  if (*zmq_skt) {
    zmq_close(*zmq_skt);
    *zmq_skt = NULL;
  }
  if (*zmq_ctx) {
    zmq_term(*zmq_ctx);
    *zmq_ctx = NULL;
  }
}

