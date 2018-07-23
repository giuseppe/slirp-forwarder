/* usermode-netns: create a working network namespace without root privileges

   Copyright (C) 2018 Giuseppe Scrivano <giuseppe@scrivano.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <net/route.h>

#include <libslirp.h>
#include <error.h>

#include "cmsg.h"

volatile static int do_exit = 0;

static int
tun_alloc (char *dev)
{
  struct ifreq ifr;
  int fd, err;

  if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
    return fd;

  memset (&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  if (*dev)
    strncpy (ifr.ifr_name, dev, IFNAMSIZ);

  if ((err = ioctl (fd, TUNSETIFF, (void *) &ifr)) < 0)
    {
      close (fd);
      return err;
    }
  strcpy (dev, ifr.ifr_name);
  return fd;
}

static void
sigint (int signo)
{
  fprintf (stderr, "exiting...\n");
  do_exit = 1;
}

int
main (int argc, char **argv)
{
  SLIRP *slirp_session = slirp_open (SLIRP_IPV4 | SLIRP_IPV6);
  int fd, s_fd;
  int sv[2];
  pid_t pid;
  int configure_network = getenv ("CONFIGURE_NETWORK") ? 1 : 0;
#define BUF_SIZE (1 << 12)
  char *buffer;

  if (argc < 2)
    error (EXIT_FAILURE, 0, "please specify the network namespace destination file");

  if (socketpair (AF_UNIX, SOCK_DGRAM, 0, sv) < 0)
    exit (EXIT_FAILURE);

  pid = fork ();
  if (pid < 0)
    exit (EXIT_FAILURE);
  if (pid == 0)
    {
      int sockfd;
      struct ifreq ifr;
      char name[IFNAMSIZ + 1];
      struct file_t file;

      file.name = "tap";

      close (sv[1]);

      if (unshare (CLONE_NEWNET) < 0)
        exit (EXIT_FAILURE);

      name[0] = '\0';
      file.fd = tun_alloc (name);
      if (file.fd < 0)
        exit (EXIT_FAILURE);

      sockfd = socket (AF_INET, SOCK_DGRAM, 0);
      if (sockfd < 0)
        error (EXIT_FAILURE, errno, "cannot create socket");

      memset (&ifr, 0, sizeof (ifr));
      ifr.ifr_flags = IFF_UP | IFF_RUNNING;
      strcpy (ifr.ifr_name, name);

      if (ioctl (sockfd, SIOCSIFFLAGS, &ifr) < 0)
        error (EXIT_FAILURE, errno, "cannot set device up");

      if (configure_network)
        {
          struct rtentry route;
          struct sockaddr_in *sai = (struct sockaddr_in*) &ifr.ifr_addr;

          sai->sin_family = AF_INET;
          sai->sin_port = 0;
          inet_pton (AF_INET, "10.0.2.10", &sai->sin_addr);

          if (ioctl (sockfd, SIOCSIFADDR, &ifr) < 0)
            error (EXIT_FAILURE, errno, "cannot set device address");

          inet_pton (AF_INET, "255.255.255.0", &sai->sin_addr);
          if (ioctl (sockfd, SIOCSIFNETMASK, &ifr) < 0)
            error (EXIT_FAILURE, errno, "cannot set device netmask");

          memset (&route, 0, sizeof (route));
          sai = (struct sockaddr_in*) &route.rt_gateway;
          sai->sin_family = AF_INET;
          inet_pton (AF_INET, "10.0.2.2", &sai->sin_addr);
          sai = (struct sockaddr_in*) &route.rt_dst;
          sai->sin_family = AF_INET;
          sai->sin_addr.s_addr = INADDR_ANY;
          sai = (struct sockaddr_in*) &route.rt_genmask;
          sai->sin_family = AF_INET;
          sai->sin_addr.s_addr = INADDR_ANY;

          route.rt_flags = RTF_UP | RTF_GATEWAY;
          route.rt_metric = 0;
          route.rt_dev = name;

          if (ioctl (sockfd, SIOCADDRT, &route) < 0)
            error (EXIT_FAILURE, errno, "set route");
        }

      close (sockfd);

      if (sendfd (sv[0], file) < 0)
        error (EXIT_FAILURE, errno, "error sending fd");

      while (read (sv[0], &file.name, 1) < 0 && errno == EINTR);

      if (mount ("/proc/self/ns/net", argv[1], "bind", MS_BIND | MS_SLAVE, NULL) < 0)
        error (EXIT_FAILURE, errno, "cannot mount to %s", argv[1]);
      fprintf (stderr, "mounted to '%s'\n", argv[1]);
      exit (EXIT_SUCCESS);
    }
  else
    {
      int status, ret;
      struct file_t file;

      close (sv[0]);
      file = recvfd(sv[1]);
      if (file.fd < 0)
        exit (EXIT_FAILURE);
      fd = file.fd;

      write (sv[1], "0", 1);
      close (sv[1]);
      do
        ret = waitpid (pid, &status, 0);
      while (ret < 0 && errno == EINTR);
      if (!WIFEXITED (status) || WEXITSTATUS (status))
        exit (EXIT_FAILURE);
    }

  signal (SIGINT, sigint);

  if (slirp_start (slirp_session) < 0)
    exit (EXIT_FAILURE);

  s_fd = slirp_fd (slirp_session);

  buffer = malloc (BUF_SIZE);
  if (buffer == NULL)
    error (EXIT_FAILURE, errno, "cannot allocate buffer");

  while (! do_exit)
    {
      fd_set fds;
      FD_ZERO (&fds);
      FD_SET (fd, &fds);
      FD_SET (s_fd, &fds);

      if (select ((s_fd > fd ? s_fd : fd) + 1, &fds, NULL, NULL, NULL) <= 0)
        continue;

      if (FD_ISSET (s_fd, &fds))
        {
          int ret;
          ssize_t s = slirp_recv (slirp_session, buffer, BUF_SIZE);
          if (s < 0)
            error (EXIT_FAILURE, errno, "read from vnet");

          fprintf (stderr, "got %d bytes from vnet\n", s);
          do
            ret = write (fd, buffer, s);
          while (ret < 0 && errno == EINTR);
          if (ret < 0)
            error (EXIT_FAILURE, errno, "write to tap device");
        }
      if (FD_ISSET (fd, &fds))
        {
          ssize_t s;

          do
            s = read (fd, buffer, BUF_SIZE);
          while (s < 0 && errno == EINTR);
          if (s < 0)
            error (EXIT_FAILURE, errno, "read from tap device");

          fprintf (stderr, "got %d bytes from the tap device\n", s);

          s = slirp_send (slirp_session, buffer, s);
          if (s < 0)
            error (EXIT_FAILURE, errno, "write to vnet");
        }
    }
  free (buffer);

  umount (argv[1]);

  slirp_close (slirp_session);
  return 0;
}
