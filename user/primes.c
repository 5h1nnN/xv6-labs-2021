#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// One process per prime.  Each process reads numbers from its left
// neighbour, prints the first one (which is prime) and forwards the
// numbers not divisible by that prime to the next process on the right.
void
sieve(int lfd)
{
  int p;

  // No more numbers: the previous process has exited.
  if(read(lfd, &p, sizeof(p)) != sizeof(p)){
    close(lfd);
    exit(0);
  }
  printf("prime %d\n", p);

  int fd[2];
  if(pipe(fd) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  if(fork() == 0){
    // Right-hand process: filter the remaining numbers.
    close(fd[1]);
    close(lfd);
    sieve(fd[0]);
    exit(0);
  }
  close(fd[0]);

  int n;
  while(read(lfd, &n, sizeof(n)) == sizeof(n)){
    if(n % p != 0)
      write(fd[1], &n, sizeof(n));
  }
  close(lfd);
  close(fd[1]);

  // Wait for the whole pipeline to finish before exiting.
  wait(0);
  exit(0);
}

int
main(void)
{
  int fd[2];

  if(pipe(fd) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  if(fork() == 0){
    close(fd[1]);
    sieve(fd[0]);
    exit(0);
  }
  close(fd[0]);

  for(int i = 2; i <= 35; i++)
    write(fd[1], &i, sizeof(i));
  close(fd[1]);

  wait(0);
  exit(0);
}
