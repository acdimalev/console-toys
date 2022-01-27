//  A simple demonstration of interactively dealing with terminal size.
//
//  To use...
//
//      $ make demo-terminal-size
//      $ ./demo-terminal-size
//
//  Press ctrl+c to terminate the application.


//  Copyright (c) 2021 Jamin <acdimalev@gmail.com>
//
//  Permission to use, copy, modify, and/or distribute this software for any
//  purpose with or without fee is hereby granted, provided that the above
//  copyright notice and this permission notice appear in all copies.
//
//  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
//  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
//  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
//  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
//  OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


bool winch = true;

void sigwinch_handler(int signal)
{ winch = true; }


int main(int argc, char **argv)
{ // handle notifications that terminal size has changed
  sigaction(SIGWINCH, &(struct sigaction)
    { .sa_handler = sigwinch_handler
    }, 0);

  bool running = true;

  uint16_t cols, rows;

  char text[16];
  uint8_t text_n;

  while (running)
  { if (winch)
    { winch = false;

      // get terminal size
      { struct winsize winsize;
        ioctl(1, TIOCGWINSZ, &winsize);
        cols = winsize.ws_col;
        rows = winsize.ws_row;
      }

      snprintf(text, sizeof(text), "%d x %d", cols, rows);
      text_n = strnlen(text, sizeof(text));
    }

    { // render screen

      int screen_n = cols * rows;
      char screen[cols * rows];

      for (int y = 0; y < rows; y++)
      for (int x = 0; x < cols; x++)
      { int u = (rows / 2 == y) ? (x + (text_n - cols) / 2) : -1;
        screen[y * cols + x] =
          (u >= 0) & (u < text_n) ? text[u] : ' ';
      }

      write(1, "\r", 1);
      write(1, screen, screen_n);
    }

    sleep(1);
  }

  return 0;
}
