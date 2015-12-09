/*
 * Copyright (c) 2015, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <picoos.h>
#include <picoos-lwip.h>


#include <stdlib.h>
#include <unistd.h>

void tcpClientThread(void*);
void tcpServerThread(void*);

void tcpClientThread(void* arg)
{
  int sock = (long)arg;
  int i;

  nosPrintf("in tcp client\n");
  for (i = 0; i < 5; i++) {

    write(sock, "Hello!\n", 7);
    posTaskSleep(MS(1000));
  }

  close(sock);
}

void tcpServerThread(void* arg)
{
  int sockd, sockd2;
  socklen_t addrlen;
  struct sockaddr_in myAddr, peerAddr;
  int status;

  sockd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockd == -1) {

    nosPrintf("Socket creation error\n");
    return;
  }

/* 
 * Bind socket to telnet port so we can test this easily.
 */
  myAddr.sin_family = AF_INET;
  myAddr.sin_addr.s_addr = INADDR_ANY;
  myAddr.sin_port = htons(24);

  status = bind(sockd, (struct sockaddr*)&myAddr, sizeof(myAddr));
  if (status == -1) {

    nosPrintf("Binding error\n");
    return;
  }

  status = listen(sockd, 5);
  if (status == -1) {

    nosPrintf("Listening error\n");
    return;
  }

  nosPrintf("socket server listening\n");
  for(;;) {

/*
 * Wait for new connection. 
 */
    addrlen = sizeof(peerAddr);
    sockd2 = accept(sockd, (struct sockaddr*)&peerAddr, &addrlen);
    if (sockd2 == -1) {

      nosPrintf("Wrong connection\n");
      continue;
    }
/*
 * Create thread to serve connection.
 */
    if (nosTaskCreate(tcpClientThread, (void*)(long)sockd2, 5, 512, "socktest") == NULL)
       close(sockd2);

  }
}
