#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h>
#include <assert.h>

struct config {
  uint8_t mitm, debug, privacy;
} config = { .mitm = 0, .debug = 0, .privacy = 0, };

int main(int argc, char *const*argv) {
  uint8_t done = 0;
  while(!done) {
    int rc = getopt(argc,argv,"mdp");
    switch(rc) {
    case 'm':
      config.mitm = 1;
      break;
    case 'd':
      config.debug = 1;
      break;
    case 'p':
      config.privacy = 1;
      break;
    case -1:
      done = 1;
      break;
    default:
      fprintf(stderr,"Unknown option '%c'\n",rc);
      exit(1);
    }
  }
  if(optind != (argc - 2)) {
    fprintf(stderr,"Usage: bg-ncp-bond [ -m ] [ -d ][ -p ] <comport-A> <comport-B>\n");
    exit(1);
  }
  char buf1[256], buf2[256];
  sprintf(buf1,"bg-ncp-bond-helper -F 1 -P %s -m -s -d -f /tmp/pk -b /tmp/mac-address%s",
	  argv[optind], (config.debug)?" -g":"");
  unlink("/tmp/mac-address");
  pid_t pid1 = fork();
  if(pid1) system(buf1);
  else {
    FILE *fh;
    do {
      fh = fopen("/tmp/mac-address","r");
    } while(!fh);
    uint8_t addr[6];
    assert(1 == fread(addr,6,1,fh));
    fclose(fh);
    unlink("/tmp/mac-address");
    char addr_buf[19];
    for(int i = 0; i < 6; i++) {
      sprintf(&addr_buf[3*i],"%02x:",addr[5-i]);
    }
    addr_buf[17] = 0;
    sprintf(buf2,"bg-ncp-bond-helper -F 1 -P %s -m -s -k -f /tmp/pk -a %s%s%s",argv[optind+1],addr_buf,
	    (config.debug)?" -g":"",
	    (config.privacy)?" -p":"");
    pid_t pid2 = fork();
    if(pid2) system(buf2);
    else {
      printf("addr_buf: '%s'\n",addr_buf);
      printf("buf1: '%s'\n",buf1);
      printf("buf2: '%s'\n",buf2);
    }
  }
}
