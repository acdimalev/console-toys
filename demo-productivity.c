//  A simple hack to feel 100% more productive.
//
//  To use...
//
//      $ make demo-productivity
//      $ ./demo-productivity demo-productivity.c
//
//  Press ctrl+c to terminate the application.


//  Copyright (c) 2022 Jamin <acdimalev@gmail.com>
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


#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


#define len(x) (sizeof(x) / sizeof(*x))
#define for_each(x, xs) \
  for \
  ( typeof(*xs) *x = xs \
  ; x < xs + len(xs) \
  ; x++ \
  )


// sampling functions for screen rastering
// output encoded as UTF8, little-endian, and null-padded

// sampling for panel frames

// https://en.wikipedia.org/wiki/Box_Drawing
//
// takes a pre-computed "border intersection" as input
//
//   input                 output
// 0b0101 0b0100 0b0110  U+250C U+2500 U+2510
// 0b0001        0b0010  U+2502        U+2502
// 0b1001 0b1000 0b1010  U+2514 U+2500 U+2518

static inline \
uint32_t sample_frame(int border)
{ uint32_t table[]
  = {      ' ', 0x8294e2, 0x8294e2, 0x8294e2  // 0, 1, 2, 3
    , 0x8094e2, 0x8c94e2, 0x9094e2, 0x8294e2  // 4, 5, 6, 7
    , 0x8094e2, 0x9494e2, 0x9894e2, 0x8294e2  // 8, 9, a, b
    ,      '+',      '+',      '+',      '+'  // c, d, e, f
    };
  return table[border & 15];
}

// sampling for scrolling text

char text[1024 * 8];
int n_text;

struct
{ int i, n;
} text_lines[1024];
int n_text_lines;

static inline \
bool sample_text(int u, int v)
{ v = v % n_text_lines;
  return
    ((0 <= v) & (v < n_text_lines))
    ? ((0 <= u) & (u < text_lines[v].n))
      ? (' ' != text[text_lines[v].i + u])
      : false
    : false
  ;
}

// sampling based on psf-intensity-map
// half-intensity for better contrast
//
// ^^rr>>
// '';;ii
//   ,,__

static inline \
uint32_t sample_smol_text(int u, int v)
{ int x
  = ( sample_text(0 + u, 0 + v) << 2
    | sample_text(1 + u, 0 + v) << 3
    | sample_text(0 + u, 1 + v) << 0
    | sample_text(1 + u, 1 + v) << 1
    );
  return " ,,_';;i';;i^rr>"[x & 15];
}

// sampling for panels

struct bounds { int x1, x2, y1, y2; };

static inline \
uint32_t sample_panel(struct bounds *bounds, int scroll, int x, int y)
{ int border
  = ( bounds->x1 == x ) << 0
  | ( bounds->x2 == x ) << 1
  | ( bounds->y1 == y ) << 2
  | ( bounds->y2 == y ) << 3
  ;
  if (border)
    return sample_frame(border);
  else
    return sample_smol_text
    ( (x - bounds->x1 - 1) << 1
    , ((y - bounds->y1 - 1) << 1) + scroll
    );
}


int main(int argc, char **argv)
{ if (2 != argc) return -1;
  char *source_file = argv[1];

  int rows, cols;
  int fps = 30;

  { // read in source file
    int f = open(source_file, O_RDONLY);
    if (-1 == f) return -1;
    n_text = read(f, text, sizeof(text));
  }

  { // translate into lines
    int u = 0;
    text_lines[u].i = 0;
    for (int i = 0; i < n_text; i++)
      if ('\n' == text[i])
      { text_lines[u].n = i - text_lines[u].i;
        u++; if (len(text_lines) < u) return -1;
        text_lines[u].i = i + 1;
      }
    n_text_lines = u;
  }

  { // get screen size
    struct winsize winsize;
    ioctl(1, TIOCGWINSZ, &winsize);
    cols = winsize.ws_col;
    rows = winsize.ws_row;
  }

  int n_panels = 20;
  struct panel
  { int x, y, w, h, scroll, phase, delay;
  } panels[n_panels];
  for (int i = 0; i < n_panels; i++)
    panels[i].phase = 0;

  while (true)
  { // animation
    for_each (panel, panels)
    switch (panel->phase)
    { case 0:
        panel->h = -1;
        panel->delay = 1 * fps + rand() % (9 * fps);
        panel->phase++;
      case 1:
        panel->delay--;
        if (0 < panel->delay)
          break;
        panel->phase++;
      case 2:
        panel->x = rand() % (cols - 40) + 40 / 2;
        panel->y = rand() % (rows - 12) + 12 / 2;
        panel->w = panel->h = 0;
        panel->scroll = rand() % n_text_lines;
        panel->phase++;
        break;
      case 3:
        panel->h += 4; if (12 <= panel->h) panel->h = 12, panel->phase++;
        break;
      case 4:
        panel->w += 8; if (40 <= panel->w)
          panel->w = 40, panel->phase++, panel->delay = 0;
        break;
      case 5:
        panel->scroll += 2;
        panel->delay++; if (40 <= panel->delay) panel->phase++;
        break;
      case 6:
        panel->w -= 8; if (0 >= panel->w) panel->w = 0, panel->phase++;
        break;
      case 7:
        panel->h -= 4; if (0 > panel->h) panel->phase = 0;
        break;
    }

    { // raster
      uint32_t screen[(cols + 1) * rows - 1];
      for (int y = 1; y < rows; y++)
        screen[(cols + 1) * y - 1] = '\n';

      // calculate bounds for panels
      struct bounds bounds[n_panels];
      for (int i = 0; i < n_panels; i++)
      { bounds[i].x1 = panels[i].x - (panels[i].w >> 1);
        bounds[i].x2 = panels[i].x + (panels[i].w >> 1);
        bounds[i].y1 = panels[i].y - (panels[i].h >> 1);
        bounds[i].y2 = panels[i].y + (panels[i].h >> 1);
      }

      int i = 0;
      for (int y = 0; y < rows; y++, i++)
      for (int x = 0; x < cols; x++, i++)
      { // select panel to render
        int n;
        for (n = 0; n < n_panels; n++)
          if
          ( (bounds[n].x1 <= x) & (bounds[n].x2 >= x)
          & (bounds[n].y1 <= y) & (bounds[n].y2 >= y)
          ) break;

        if (n_panels > n)
          screen[i] = sample_panel(&bounds[n], panels[n].scroll, x, y);
        else
          screen[i] = ' ';
      }

      write(1, "\33[H", 3);
      write(1, screen, sizeof(screen));
    }
    nanosleep(&(struct timespec){
      0, 1000000000 / 30,
    }, 0);
  }

  return 0;
}
