/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved. 
 *
 * Contributors:
 *     <date>: <author>, <author email> - changes
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

#include <opener_api.h>
#include "networkhandler.h"
#include <encap.h>
#include <cipconnectionmanager.h>
#include <endianconv.h> 
#include <trace.h>

EIP_UINT8 g_acCommBuf[OPENER_ETHERNET_BUFFER_SIZE];

#define MAX_NO_OF_TCP_SOCKETS 10

typedef long MILLISECONDS;
fd_set master;
fd_set read_fds;
/* temporary file discriptor for select() */

int fdmax;
int listener;
int newfd;

static struct timeval tv;
static MILLISECONDS actualtime, lasttime;

EIP_STATUS
handleDataOnTCPSocket(int pa_nSocket);

static MILLISECONDS
getmilliseconds(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);
  return (MILLISECONDS) tv.tv_sec * 1000 + (MILLISECONDS) tv.tv_usec / 1000;
}

/*! Check if the socket indentifier is associate with an active connection
 * 
 *  @param pa_nFD  socket handle
 *  @return 1 if the given socket handler is of an active conneciton otherwise 0
 */
int
isConnectedFd(int pa_nFD)
{
  int i;
  extern S_CIP_ConnectionObject stConnectionObject[];

  S_CIP_ConnectionObject *con = &stConnectionObject[0]; /* this is done this way because the debugger would not display the connection table */

  for (i = 0; i < OPENER_NUMBER_OF_SUPPORTED_CONNECTIONS; i++)
    {
      if ((stConnectionObject[i].sockfd[0] == pa_nFD)
          || (stConnectionObject[i].sockfd[1] == pa_nFD))
        {
          return 1;
        }
    }
  return 0;
}

/* INT8 Start_NetworkHandler()
 * 	start a TCP listening socket, accept connections, receive data in select loop, call manageConnections periodicaly.
 * 	return status
 * 			-1 .. error
 */

EIP_STATUS
Start_NetworkHandler()
{
  struct sockaddr_in remote_addr, from;
  struct sockaddr_in my_addr;
  socklen_t addrlen;
  socklen_t fromlen = sizeof(from);
  int nUDPListener;

  int nReceived_size;
  int res;
  MILLISECONDS elapsedtime = 0;
  int fd;
  EIP_UINT8 *rxp;
  int nRemainingBytes;
  int replylen;

  /* clear the master an temp sets */
  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  /* create a new TCP socket */
  if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
      OPENER_TRACE_ERR("error allocating socket stream listener, %d\n", errno);
      return EIP_ERROR;
    }

  /* create a new UDP socket */
  if ((nUDPListener = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
    {
      OPENER_TRACE_ERR("error allocating udp listener socket, %d\n", errno);
      return EIP_ERROR;
    }

  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(OPENER_ETHERNET_PORT);
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(&my_addr.sin_zero, 0, sizeof(my_addr.sin_zero));

  /* bind the new socket to port 0xAF12 (CIP) */
  if ((bind(listener, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)))
      == -1)
    {
      OPENER_TRACE_ERR("error with bind: %s", strerror(errno));
      return EIP_ERROR;
    }

  /* enable the udp socket to receive broadcast messages*/
  int y = 1;
  if (0 > setsockopt(nUDPListener, SOL_SOCKET, SO_BROADCAST, &y, sizeof(int)))
    {
      OPENER_TRACE_ERR("error with setting broadcast receive for udp socket: %s", strerror(errno));
      return EIP_ERROR;
    }

  if ((bind(nUDPListener, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)))
      == -1)
    {
      OPENER_TRACE_ERR("error with udp bind: %s", strerror(errno));
      return EIP_ERROR;
    }

  /* switch socket in listen mode */
  if ((listen(listener, MAX_NO_OF_TCP_SOCKETS)) == -1)
    {
      OPENER_TRACE_ERR("networkhandler: error with listen: %s", strerror(errno));
      return EIP_ERROR;
    }

  /* add the listener socket to the master set */
  FD_SET(listener, &master);
  FD_SET(nUDPListener, &master);

  /* keep track of the biggest file discriptor */
  fdmax = (listener > nUDPListener) ? listener : nUDPListener;

  lasttime = getmilliseconds(); /* init timekeeping */
  elapsedtime = 0;

  while (1)
    {
      read_fds = master;

      tv.tv_sec = 0;
      tv.tv_usec = (elapsedtime < OPENER_TIMER_TICK ? OPENER_TIMER_TICK
          - elapsedtime : 0) * 1000; /* 10 ms */

      res = select(fdmax + 1, &read_fds, 0, 0, &tv);
      if (res == -1)
        {
          if (EINTR == errno)
            {
              /* we have somehow been interrupted. The default behavior is to
               * go back into the select loop.
               */
              continue;
            }
          else
            {
              OPENER_TRACE_ERR("networkhandler: error with select: %s", strerror(errno));
              return EIP_ERROR;
            }
        }

      if (res > 0)
        for (fd = 0; fd <= fdmax; fd++)
          {
            if (FD_ISSET(fd, &read_fds))
              {
                if (!FD_ISSET(fd, &master))
                  {
                    OPENER_TRACE_INFO("socket fd %d closed with pending message\n", fd);
                    continue;
                  }

                /* see if this is a connection request to the TCP listener*/
                if (fd == listener) /* handle new TCP connection */
                  {
                    OPENER_TRACE_INFO("networkhandler: new TCP connection\n");
                    addrlen = sizeof(remote_addr);
                    newfd = accept(listener, (struct sockaddr *) &remote_addr,
                        &addrlen); /* remote_addr does not seem to be used*/
                    if (newfd == -1)
                      {
                        OPENER_TRACE_ERR("networkhandler: error on accept: %s", strerror(errno));
                        continue;
                      }

                    FD_SET(newfd, &master);
                    /* add newfd to master set */
                    if (newfd > fdmax)
                      fdmax = newfd;
                    OPENER_TRACE_STATE(
                        "networkhandler: opened new TCP connection on fd %d\n",
                        newfd);
                    continue;
                  }

                /* see if this is an unsolicited inbound UDP message */
                if (fd == nUDPListener)
                  {
                    OPENER_TRACE_STATE(
                        "networkhandler: unsolicited UDP message on fd %d\n",
                        fd);

                    /*Handle udp broadcast messages */
                    nReceived_size = recvfrom(fd, g_acCommBuf,
                        OPENER_ETHERNET_BUFFER_SIZE, 0,
                        (struct sockaddr *) &from, &fromlen);

                    if (nReceived_size <= 0)
                      { /* got error */
                        OPENER_TRACE_ERR(
                            "networkhandler: error on recvfrom udp broadcast port: %s",
                            strerror(errno));
                        continue;
                      }

                    OPENER_TRACE_INFO("Data received on udp:\n");

                    rxp = &g_acCommBuf[0];
                    do
                      {
                        replylen = handleReceivedExplictData(fd, rxp,
                            nReceived_size, &nRemainingBytes);

                        rxp += nReceived_size - nRemainingBytes;
                        nReceived_size = nRemainingBytes;

                        if (replylen > 0)
                          {
                            OPENER_TRACE_INFO("reply sent:\n");

                            /* if the active fd matches a registered UDP callback, handle a UDP packet */
                            res = sendto(fd, (char *) g_acCommBuf, replylen, 0,
                                (struct sockaddr *) &from, sizeof(from));
                            if (res != replylen)
                              {
                                OPENER_TRACE_INFO(
                                    "networkhandler: UDP response was not fully sent\n");
                              }
                          }
                      }
                    while (nRemainingBytes > 0);
                    continue;
                  }

                /* see if the message is from a registered UDP socket */
                if (isConnectedFd(fd))
                  {
                    fromlen = sizeof(from);
                    nReceived_size = recvfrom(fd, g_acCommBuf,
                        OPENER_ETHERNET_BUFFER_SIZE, 0,
                        (struct sockaddr *) &from, &fromlen);
                    if (nReceived_size == 0)
                      {
                        OPENER_TRACE_STATE("connection closed by client\n");
                        FD_CLR(fd, &master);
                        /* remove from master set */
                        close(fd); /* close socket */
                        continue;
                      }
                    if (nReceived_size <= 0)
                      {
                        OPENER_TRACE_ERR("networkhandler: error on recv: %s", strerror(errno));
                        FD_CLR(fd, &master);
                        /* remove from master set */
                        close(fd); /* close socket */
                        continue;
                      }

                    handleReceivedConnectedData(g_acCommBuf, nReceived_size);
                    continue;
                  }

                /* if not registered UDP, handle as a TCP receive */
                if (EIP_ERROR == handleDataOnTCPSocket(fd)) /* if error */
                  {
                    FD_CLR(fd, &master);
                    /* remove connection from master set */
                    closeSession(fd); /* clean up session and close the socket */
                  }
              }
          }

      actualtime = getmilliseconds();
      elapsedtime += actualtime - lasttime;
      lasttime = actualtime;

      /* check if we had been not able to update the connection manager for several OPENER_TIMER_TICK.
       * This should compensate the jitter of the windows timer
       */
      while (elapsedtime >= OPENER_TIMER_TICK)
        {
          /* call manage_connections() in connection manager every OPENER_TIMER_TICK ms */
          manageConnections();
          elapsedtime -= OPENER_TIMER_TICK;
        }
    }
}

EIP_STATUS
IApp_SendUDPData(struct sockaddr_in *pa_pstAddr, int pa_nSockFd,
    EIP_UINT8 *pa_acData, EIP_UINT16 pa_nDataLength)
{
  int sentlength;

  sentlength = sendto(pa_nSockFd, (char *) pa_acData, pa_nDataLength, 0,
      (struct sockaddr *) pa_pstAddr, sizeof(*pa_pstAddr));

  if (sentlength < 0)
    {
      OPENER_TRACE_ERR("networkhandler: error with sendto in sendUDPData: %s", strerror(errno));
      return EIP_ERROR;
    }
  else if (sentlength != pa_nDataLength)
    {
      OPENER_TRACE_WARN("not all data was sent in sendUDPData, sent %d of %d\n",
          sentlength, pa_nDataLength);
      return EIP_ERROR;
    }
  else
    return EIP_OK;
}

EIP_STATUS
handleDataOnTCPSocket(int pa_nSocket)
{
  EIP_UINT8 *rxp;
  int nDataSize;
  int nDataSent;
  int nRemainingBytes = 0;

  /* We will handle just one EIP packet here the rest is done by the select
   * method which will inform us if more data is available in the socket
   because of the current implementation of the main loop this may not be
   the fastest way and a loop here with a non blocking socket would better
   fit*/

  /*Check how many data is here -- read the first four bytes from the connection */
  nDataSize = recv(pa_nSocket, g_acCommBuf, 4, 0); /*TODO we may have to set the socket to a non blocking socket */

  if (nDataSize == 0)
    {
      OPENER_TRACE_ERR("networkhandler: connection closed by client: %s", strerror(errno));
      return EIP_ERROR;
    }
  if (nDataSize < 0)
    {
      OPENER_TRACE_ERR("networkhandler: error on recv: %s", strerror(errno));
      return EIP_ERROR;
    }

  rxp = &g_acCommBuf[2]; /* at this place EIP stores the data length */
  nDataSize = ltohs(&rxp) + ENCAPSULATION_HEADER_LENGTH - 4; /* -4 is for the 4 bytes we have already read*/
  /* (NOTE this advances the buffer pointer) */
  if (OPENER_ETHERNET_BUFFER_SIZE < nDataSize)
    { /*TODO can this be handled in a better way?*/
      OPENER_TRACE_ERR("too large packet recieved will be ignored\n"); /* this may corrupt the connection ???*/
      return EIP_OK;
    }

  nDataSize = recv(pa_nSocket, &g_acCommBuf[4], nDataSize, 0);

  if (nDataSize == 0) /* got error or connection closed by client */
    {
      OPENER_TRACE_ERR("networkhandler: connection closed by client: %s", strerror(errno));
      return EIP_ERROR;
    }
  if (nDataSize < 0)
    {
      OPENER_TRACE_ERR("networkhandler: error on recv: %s", strerror(errno));
      return EIP_ERROR;
    }

  nDataSize += 4;
  /*TODO handle partial packets*/
  OPENER_TRACE_INFO("Data received on tcp:\n");

  nDataSize = handleReceivedExplictData(pa_nSocket, g_acCommBuf, nDataSize,
      &nRemainingBytes);
  if (nRemainingBytes != 0)
    {
      OPENER_TRACE_WARN("Warning: received packet was to long: %d Bytes left!\n",
          nRemainingBytes);
    }

  if (nDataSize > 0)
    {
      OPENER_TRACE_INFO("reply sent:\n");

      nDataSent = send(pa_nSocket, (char *) g_acCommBuf, nDataSize, 0);
      if (nDataSent != nDataSize)
        {
          OPENER_TRACE_WARN("TCP response was not fully sent\n");
        }
    }

  return EIP_OK;
}

/* create a new UDP socket for the connection manager
 returns the fd if successful, else -1 */
int
IApp_CreateUDPSocket(int pa_nDirection, struct sockaddr_in *pa_pstAddr)
{
  /* create a new UDP socket */
  if ((newfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
    {
      OPENER_TRACE_ERR("networkhandler: cannot create UDP socket: %s\n", strerror(errno));
      return EIP_ERROR;
    }

  OPENER_TRACE_INFO("networkhandler: UDP socket %d\n", newfd);

  /* check if it is sending or receiving */
  if (pa_nDirection == CONSUMING)
    { /* bind is only for consuming necessary */
      if ((bind(newfd, (struct sockaddr *) pa_pstAddr, sizeof(struct sockaddr)))
          == -1)
        {
          OPENER_TRACE_ERR("error on bind udp: %s\n", strerror(errno));
          return EIP_ERROR;
        }

      OPENER_TRACE_INFO("networkhandler: bind UDP socket %d\n", newfd);
    }

  /* add new fd to the master list */
  FD_SET(newfd, &master);
  if (newfd > fdmax)
    {
      fdmax = newfd;
    }
  return newfd;
}

void
IApp_CloseSocket(int pa_nSockFd)
{
  if (EIP_INVALID_SOCKET != pa_nSockFd)
    {
      FD_CLR(pa_nSockFd, &master);
      close(pa_nSockFd);
    }
}