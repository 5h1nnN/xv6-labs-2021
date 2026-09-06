#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
  char *nargv[MAXARG];
  char line[512];
  int narg, i, n;
  char c;

  if(argc < 2){
    fprintf(2, "usage: xargs command [arg ...]\n");
    exit(1);
  }

  // Fixed arguments come from the command line.
  narg = 0;
  for(i = 1; i < argc && narg < MAXARG - 1; i++)
    nargv[narg++] = argv[i];

  // Read the standard input one line at a time and run the
  // command once per line, appending the line's words as arguments.
  n = 0;
  while(read(0, &c, 1) == 1){
    if(c == '\n'){
      if(n > 0){
        line[n] = 0;

        // Split the line into words and append them to the arguments.
        char *p = line;
        while(*p != 0){
          while(*p == ' ' || *p == '\t')
            p++;
          if(*p == 0)
            break;
          nargv[narg++] = p;
          while(*p != 0 && *p != ' ' && *p != '\t')
            p++;
          if(*p != 0){
            *p = 0;
            p++;
          }
        }
        nargv[narg] = 0;

        if(fork() == 0){
          exec(nargv[0], nargv);
          fprintf(2, "xargs: exec %s failed\n", nargv[0]);
          exit(1);
        }
        wait(0);

        // Reset the argument list to the fixed arguments only.
        narg = argc - 1;
      }
      n = 0;
    } else {
      if(n < sizeof(line) - 1)
        line[n++] = c;
    }
  }
  exit(0);
}
