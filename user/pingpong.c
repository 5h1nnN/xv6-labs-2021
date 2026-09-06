#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int p2c[2];
  int c2p[2];
  char buf[1];

  if(pipe(p2c) < 0 || pipe(c2p) < 0){
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }

  if(fork() == 0){
    // child: wait for the byte, then send it back
    close(p2c[1]);
    close(c2p[0]);

    if(read(p2c[0], buf, 1) != 1){
      fprintf(2, "pingpong: read failed\n");
      exit(1);
    }
    printf("%d: received ping\n", getpid());
    write(c2p[1], buf, 1);

    close(p2c[0]);
    close(c2p[1]);
    exit(0);
  } else {
    // parent: send a byte, then wait for the reply
    close(p2c[0]);
    close(c2p[1]);

    buf[0] = 'x';
    write(p2c[1], buf, 1);
    if(read(c2p[0], buf, 1) != 1){
      fprintf(2, "pingpong: read failed\n");
      exit(1);
    }
    printf("%d: received pong\n", getpid());

    close(p2c[1]);
    close(c2p[0]);
    wait(0);
    exit(0);
  }
}
