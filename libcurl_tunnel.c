#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <curl/curl.h>

#define _FILE_OFFSET_BITS 64 /* for curl_off_t magic */
#define SIZE 655536

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
    /*FD_SET(fileno(stdout), &fdwrite); Don't think this is required*/
    FD_SET(sockfd, &fdread);
    FD_SET(sockfd, &fdwrite);

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
      fprintf(stderr,"Timeout\n");
      break;
    }
    /* use FD_ISSET() to check what happened, then read/write accordingly */
    if (FD_ISSET(fileno(stdin), &fdread))
    {
      if(fdcopy(fileno(stdin),sockfd))
        break;
    }
    else if (FD_ISSET(sockfd,&fdread))
    {
      if(fdcopy(sockfd,fileno(stdout)))
        break;
    }
    else
    {
      fprintf(stderr,"Something weird happened\n");
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
  /* curl_easy_setopt(hnd, CURLOPT_HTTPPROXYTUNNEL, 1); */
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
