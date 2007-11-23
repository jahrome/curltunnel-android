#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <curl/curl.h>
#include <getopt.h>

#define _FILE_OFFSET_BITS 64 /* for curl_off_t magic */
#define SIZE 655536

struct gengetopt_args_info {
  char *proxy_arg;
  char *proxyhost_arg;
  int  proxyport_arg;
  char *user_arg;
  char *pass_arg;
  char *dest_arg;

  int help_given;
  int verbose_given;
  int user_given;
  int pass_given;
  int proxy_given;
  int proxyhost_given;
  int proxyport_given;
  int dest_given;
};

void print_options(void)
{
  printf("curltunnel\nCopyright 2007 curltunnel Project\nV0.1\n"
      "\n"
      "-h       --help            Print help and exit\n"
      "-p       --proxy=STRING    Proxy host:port combination to connect to\n"
      "-u       --user=STRING     Username to send to proxy\n"
      "-s       --pass=STRING     Password to send to proxy\n"
      "-d       --dest=STRING     Host:port combination to tunnel to\n"
      "-v       --verbose         Print messages about what's going on\n");
}

static char * gengetopt_strdup(char *s)
{
  char *n, *pn, *ps=s;
  while(*ps) ps++;
  n=(char *)malloc(1+ps-s);
  if(n!=NULL)
  {
    for(ps=s,pn=n;*ps;ps++,pn++)
      *pn = *ps;
    *pn=0;
  }
  return n;
}

int command_line_parser(int argc, char * const *argv, struct gengetopt_args_info *args_info)
{
  int c;
  int r;

#define clear_args()\
{\
  args_info->help_given = 0;\
  args_info->verbose_given = 0;\
  args_info->user_given = 0;\
  args_info->pass_given = 0;\
  args_info->proxy_given = 0;\
  args_info->proxyhost_given = 0;\
  args_info->proxyport_given = 0;\
  args_info->dest_given = 0;\
  args_info->proxyport_arg = 0;\
  args_info->proxy_arg = NULL;\
  args_info->proxyhost_arg = NULL;\
  args_info->user_arg = NULL;\
  args_info->pass_arg = NULL;\
  args_info->dest_arg = NULL;\
}
  optarg = 0;
  optind = 1;
  opterr = 1;
  optopt = '?';

  while(1)
  {
    int option_index = 0;

    static struct option long_options[] = {
      { "help", 0, NULL, 'h' },
      { "verbose", 0, NULL, 'v' },
      { "proxy", 1, NULL, 'p' },
      { "user", 1, NULL, 'u' },
      { "pass", 1, NULL, 's' },
      { "dest", 1, NULL, 'd' },
      { NULL, 0, NULL, 0}
    };

    c = getopt_long(argc,argv,"hv:p:u:s:d", long_options, &option_index);

    if(c==-1) break;

    switch(c)
    {
      case 'h':
        clear_args();
        print_options();
        exit(0);
      case 'v':
        args_info->verbose_given = !(args_info->verbose_given);
        if(args_info->verbose_given)
          printf("Verbose mode on\n");
        break;
      case 'p':
        if(args_info->proxy_given)
        {
          fprintf(stderr,"curltunnel: `--proxy' (`-p') option given more than once\n");
          clear_args();
          exit(1);
        }
        args_info->proxy_given = 1;
        args_info->proxy_arg = gengetopt_strdup(optarg);
        break;
      case 'u':
        if(args_info->user_given)
        {
          fprintf(stderr,"curltunnel: `--user' (`-u') option given more than once\n");
          clear_args();
          exit(1);
        }
        args_info->user_given = 1;
        args_info->user_arg = gengetopt_strdup(optarg);
        break;
      case 'p':
        if(args_info->pass_given)
        {
          fprintf(stderr,"curltunnel: `--pass' (`-s') option given more than once\n);
          clear_args();
          exit(1);
        }
        args_info->pass_given = 1;
        args_info->pass_arg = gengetopt_strdup(optarg);
        break;
      case 'd':
        if(args_info->dest_given)
        {
          fprintf(stderr,"curltunnel: `--dest' (`-d') option given more than once\n");
          clear_args();
          exit(1);
        }
        args_info->dest_given = 1;
        args_info->dest_arg = gengetopt_strdup(optarg);
        break;
    }
  }

  if(!args_info->proxy_given && !args_info->dest_given)
  {
    clear_args();
    print_options();
    exit(0);
  }

  if(args_info->proxy_given)
  {
    char *phost;
    int pport;

    phost = malloc(50+1);

    r = sscanf(args_info->proxy_arg, "%50[^:]:%5u", phost, &pport);
    if(r==2)
    {
      args_info->proxyhost_arg=phost;
      args_info->proxyport_arg=pport;
      args_info->proxyhost_given = 1;
      args_info->proxyport_given = 1;
    }
    else
    {
      fprintf(stderr,"curltunnel: Couldn't find your proxy hostname/ip\n");
      clear_args();
      exit(1);
    }
  }
    return 0;
}

int fd_read(int fd, void *buf, size_t len)
{
  int bytes_read;
  bytes_read = read(fd,buf,len);
  return bytes_read;
}

int fd_write(int fd, void *buf, size_t len)
{
  int bytes_written;
  bytes_written = write(fd,buf,len);
  return bytes_written;
}

int fdcopy(int fdin, int fdout)
{
  char buf[SIZE];
  int n;

  if((n=fd_read(fdin,buf,SIZE))<0)
  {
    fprintf(stderr,"Read Error\n");
    exit(1);
  }

  if(n==0)
    return 1;

  if(fd_write(fdout,buf,n)!=n)
  {
    fprintf(stderr,"Write Error\n");
    exit(1);
  }

  return 0;
}

void wait_and_act(int sockfd)
{
  struct timeval timeout;
  int rc; /* select() return code */
  long timeout_ms = 1000;

  fd_set fdread;
  fd_set fdwrite;
  fd_set fdexcep;
  int maxfd=sockfd;

  while(1==1)
  {
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    /* set timeout to wait */
    timeout.tv_sec = timeout_ms/1000;
    timeout.tv_usec = (timeout_ms%1000)*1000;

    FD_SET(fileno(stdin), &fdread);
    FD_SET(sockfd, &fdread);
    /* FD_SET(sockfd, &fdwrite); */

    rc = select(maxfd+1,
          (fd_set *)&fdread,
          (fd_set *)&fdwrite,
          (fd_set *)&fdexcep, &timeout);

    if(rc==-1) {
      /* select error */
      fprintf(stderr,"select() error\n");
      break;
    }
    if(rc==0){
      /* timeout! */
      /* fprintf(stderr,"Timeout\n"); */
    }
    /* use FD_ISSET() to check what happened, then read/write accordingly */
    if (FD_ISSET(fileno(stdin), &fdread))
    {
      if(fdcopy(fileno(stdin),sockfd))
        break;
    }
    if (FD_ISSET(sockfd,&fdread))
    {
      if(fdcopy(sockfd,fileno(stdout)))
        break;
    }
  }
}

int main(int argc, char *argv[])
{
  CURLcode ret;
  long sckt;
  CURL *hnd = curl_easy_init();

  /* Hard coded for convenience currently. Obviously we'll change this to
     read from the command line at some point */
  curl_easy_setopt(hnd, CURLOPT_URL, "http://godeater.dyndns.org:443/");
  curl_easy_setopt(hnd, CURLOPT_PROXY, "10.1.23.219:8080");
  curl_easy_setopt(hnd, CURLOPT_PROXYUSERPWD, "gbchlb:X344ekl25");
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.16.4 (i686-pc-linux-gnu) libcurl/7.16.4 GnuTLS/1.4.4 zlib/1.2.3 c-ares/1.4.0");
  curl_easy_setopt(hnd, CURLOPT_HTTPPROXYTUNNEL, 1);
  curl_easy_setopt(hnd, CURLOPT_CONNECT_ONLY, 1);
  curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1);
  ret = curl_easy_perform(hnd);

  if(ret == CURLE_OK)
  {
    /* Extract socket, and select() */
    ret = curl_easy_getinfo(hnd, CURLINFO_LASTSOCKET, &sckt);
    if((sckt==-1) || ret){
      fprintf(stderr,"[ERR] Couldn't get socket");
      return(-1);
    }
  }

  wait_and_act((int)sckt);


  curl_easy_cleanup(hnd);
}
