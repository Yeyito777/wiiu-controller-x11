#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct report_buttons
{
  int left:1;
  int right:1;
  int down:1;
  int up:1;
  int plus:1;
  int unused0:3;

  int two:1;
  int one:1;
  int b:1;
  int a:1;
  int minus:1;
  int unused1:2;
  int home:1;
} __attribute__((packed));

int main (int argc, char *argv[])
{
  struct report_buttons rpt;

  memset((void *)&rpt, 0, sizeof(rpt));

  rpt.right = 1;
  rpt.up = 1;
  rpt.two = 1;
  rpt.b = 1;

  unsigned char buf[2];

  buf[0] = (rpt.left & 0x1) |
  ((rpt.right & 0x1) << 1) |
  ((rpt.down & 0x1) << 2) |
  ((rpt.up & 0x1) << 3) |
  ((rpt.plus & 0x1) << 4);
  buf[1] = (rpt.two & 0x1) |
  ((rpt.one & 0x1) << 1) |
  ((rpt.b & 0x1) << 2) |
  ((rpt.a & 0x1) << 3) |
  ((rpt.minus & 0x1) << 4) |
  ((rpt.home & 0x1) << 7);

  unsigned char * bytes = (unsigned char *)&rpt;

  printf("Expecting %02x %02x: %02x %02x\n", buf[0], buf[1], bytes[0], bytes[1]);

  memset((void *)&rpt, 0, sizeof(rpt));

  rpt.plus = 1;
  rpt.left = 1;
  rpt.a = 1;
  rpt.home = 1;

  buf[0] = (rpt.left & 0x1) |
  ((rpt.right & 0x1) << 1) |
  ((rpt.down & 0x1) << 2) |
  ((rpt.up & 0x1) << 3) |
  ((rpt.plus & 0x1) << 4);
  buf[1] = (rpt.two & 0x1) |
  ((rpt.one & 0x1) << 1) |
  ((rpt.b & 0x1) << 2) |
  ((rpt.a & 0x1) << 3) |
  ((rpt.minus & 0x1) << 4) |
  ((rpt.home & 0x1) << 7);

  printf("Expecting %02x %02x: %02x %02x\n", buf[0], buf[1], bytes[0], bytes[1]);
}
