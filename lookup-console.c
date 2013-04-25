#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <termios.h>
#include <stdlib.h>

#include "trie.h"
#include "transitdata.h"
#include "config.h"

static struct termios g_old_kbd_mode;

// did somebody press somthing???
static int kbhit(void){
  struct timeval timeout;
  fd_set read_handles;
  int status;

  // check stdin (fd 0) for activity
  FD_ZERO(&read_handles);
  FD_SET(0, &read_handles);
  timeout.tv_sec = timeout.tv_usec = 0;
  status = select(0 + 1, &read_handles, NULL, NULL, &timeout);
  return status;
}

// put the things as they were befor leave..!!!
static void old_attr(void){
  tcsetattr(0, TCSANOW, &g_old_kbd_mode);
}


// main function
int main(int argc, char *argv[]){
  char ch;
  static char init;
  struct termios new_kbd_mode;

  if (init)
    return -1;
  // put keyboard (stdin, actually) in raw, unbuffered mode
  tcgetattr(0, &g_old_kbd_mode);
  memcpy(&new_kbd_mode, &g_old_kbd_mode, sizeof(struct termios));
 
  new_kbd_mode.c_lflag &= ~(ICANON | ECHO);
  new_kbd_mode.c_cc[VTIME] = 0;
  new_kbd_mode.c_cc[VMIN] = 1;
  tcsetattr(0, TCSANOW, &new_kbd_mode);
  atexit(old_attr);

  int len = 0;
  unsigned index = 0;
  char text[255];
  transit_data_t tdata;
  trie_t *t = trie_init();

  transit_data_load(RRRR_INPUT_FILE, &tdata);
  trie_load(t, &tdata);
 
  while (!kbhit() && len < 255){
    // getting the pressed key...
    ch = getchar();

    if (ch == 26 || ch == 10 || ch == 27) {
        index = trie_complete(t, text);
        //printf("\nResult: %d\n", index);
        //printf("\n");

        // For now ugly
        FILE *fd = fopen("/tmp/stopid", "w");
        fprintf(fd, "%d", index);
        fclose(fd);

        exit(0);
    } else if (ch == 8 || ch == 127) {
        if (len > 0) { 
            ch = 8;
            len--;
            text[len] = '\0';
        }
    } else {
        text[len] = (char) ch;
        len++;
        text[len] = '\0';
    }
    
    putchar(ch);
    index = trie_complete(t, text);
    printf("                                         %d                \r%s", index, text);
  }

  //Cleanup
  trie_free(t);

  return 0;
}

