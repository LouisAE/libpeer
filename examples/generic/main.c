#include <opus/opus.h>

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>

#include "peer.h"
#include "reader.h"

int g_interrupted = 0;
PeerConnection* g_pc = NULL;
PeerConnectionState g_state;

void peer_log(char* level_tag, const char* file_name, int line_number, const char* fmt, ...)
{
  uint32_t len = strlen(file_name);
  int i = 0;
  const char* pos = file_name;
  va_list args;

  for (i = 0; i < len; i++)
  {
    if (file_name[i] == '/')
    {
      pos = file_name + i + 1;
    }
  }

  printf("[%s] %s:%d ", level_tag, pos, line_number);

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  printf("\n");
}

static void onconnectionstatechange(PeerConnectionState state, void* data) {
  printf("state is changed: %s\n", peer_connection_state_to_string(state));
  g_state = state;
}

static void onopen(void* user_data) {
}

static void onclose(void* user_data) {
}

static void onmessage(char* msg, size_t len, void* user_data, uint16_t sid) {
  printf("on message: %d %.*s", sid, (int)len, msg);

  if (strncmp(msg, "ping", 4) == 0) {
    printf(", send pong\n");
    peer_connection_datachannel_send(g_pc, "pong", 4);
  }
}

static void signal_handler(int signal) {
  g_interrupted = 1;
}

static void* peer_singaling_task(void* data) {
  while (!g_interrupted) {
    peer_signaling_loop();
    usleep(1000);
  }

  pthread_exit(NULL);
}

static void* peer_connection_task(void* data) {
  while (!g_interrupted) {
    peer_connection_loop(g_pc);
    usleep(1000);
  }

  pthread_exit(NULL);
}

static uint64_t get_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void print_usage(const char* prog_name) {
  printf("Usage: %s -u <url> [-t <token>]\n", prog_name);
}

void parse_arguments(int argc, char* argv[], const char** url, const char** token) {
  *token = NULL;
  *url = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-u") == 0 && (i + 1) < argc) {
      *url = argv[++i];
    } else if (strcmp(argv[i], "-t") == 0 && (i + 1) < argc) {
      *token = argv[++i];
    } else {
      print_usage(argv[0]);
      exit(1);
    }
  }

  if (*url == NULL) {
    print_usage(argv[0]);
    exit(1);
  }
}

static int16_t alaw2linear(uint8_t a_val) {
  int t;
  int seg;

  a_val ^= 0x55;
  t = (a_val & 0x0f) << 4;
  seg = ((int)(a_val & 0x70)) >> 4;
  switch (seg) {
    case 0:
      t += 8;
      break;
    case 1:
      t += 0x108;
      break;
    default:
      t += 0x108;
      t <<= seg - 1;
  }
  return ((a_val & 0x80) ? t : -t);
}

int main(int argc, char* argv[]) {
  uint64_t curr_time, video_time, audio_time;
  uint8_t* buf = NULL;
  const char* url = NULL;
  const char* token = NULL;
  int size;
  int opus_err = 0;
  OpusEncoder* enc = NULL;
  unsigned char opus_buf[1024]; // Increased buffer size and changed to unsigned char
  opus_int32 opus_len = 0;

  pthread_t peer_singaling_thread;
  pthread_t peer_connection_thread;

  parse_arguments(argc, argv, &url, &token);

  signal(SIGINT, signal_handler);

  PeerConfiguration config = {
      .ice_servers = {
          {.urls = "stun:stun.l.google.com:19302"},
      },
      .datachannel = DATA_CHANNEL_STRING,
      .video_codec = CODEC_H264,
      .audio_codec = CODEC_OPUS};

  printf("=========== Parsed Arguments ===========\n");
  printf(" %-5s : %s\n", "URL", url);
  printf(" %-5s : %s\n", "Token", token ? token : "");
  printf("========================================\n");

  enc = opus_encoder_create(8000, 1, OPUS_APPLICATION_AUDIO, &opus_err);
  if (opus_err != OPUS_OK)
  {
    printf("Failed to create opus encoder: %s\n", opus_strerror(opus_err));
    return -1;
  }

  opus_encoder_ctl(enc, OPUS_SET_VBR(0));//0:CBR, 1:VBR
  opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(1));
  opus_encoder_ctl(enc, OPUS_SET_BITRATE(48000));
  opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(8));//8    0~10
  opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(16));//每个采样16个bit，2个byte
  opus_encoder_ctl(enc, OPUS_SET_DTX(0));
  opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));

  peer_init();
  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, onconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);

  peer_signaling_connect(url, token, g_pc);

  pthread_create(&peer_connection_thread, NULL, peer_connection_task, NULL);
  pthread_create(&peer_singaling_thread, NULL, peer_singaling_task, NULL);

  reader_init();

  while (!g_interrupted) {
    if (g_state == PEER_CONNECTION_COMPLETED) {
      curr_time = get_timestamp();
      // FPS 25
      if (curr_time - video_time > 40) {
        video_time = curr_time;
        if ((buf = reader_get_video_frame(&size)) != NULL) {

          peer_connection_send_video(g_pc, buf, size);
          // need to free the buffer
          free(buf);
          buf = NULL;
        }
      }

      if (curr_time - audio_time > 20) {
        if ((buf = reader_get_audio_frame(&size)) != NULL) {
          int16_t pcm_buf[320]; // 160 samples * sizeof(int16) is safe, make it larger

          for(int i=0; i < size; i++) {
             pcm_buf[i] = alaw2linear(buf[i]);
          }

          // 8000Hz * 20ms / 1000 = 160 samples
          opus_len = opus_encode(enc, pcm_buf, size, opus_buf, sizeof(opus_buf));
          if (opus_len < 0)
          {
            printf("opus encode failed: %s\n", opus_strerror(opus_len));
          } else {
            printf("size:%d\n", opus_len);
            peer_connection_send_audio(g_pc, opus_buf, opus_len);
          }
          buf = NULL;
        }
        audio_time = curr_time;
      }
    }
    usleep(1000);
  }

  pthread_join(peer_singaling_thread, NULL);
  pthread_join(peer_connection_thread, NULL);

  reader_deinit();

  peer_signaling_disconnect();
  peer_connection_destroy(g_pc);
  peer_deinit();

  return 0;
}
