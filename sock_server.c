#include <picoos.h>
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"


#include <stdlib.h>

void tcpClientThread(void*);
void tcpServerThread(void*);

void tcpClientThread(void* arg)
{
  int sock = (long)arg;
  int i;
  nosPrintf("in tcp client\n");
  for (i = 0; i < 5; i++) {
    lwip_write(sock, "Hello!\n", 7);
    posTaskSleep(MS(1000));
  }

  lwip_close(sock);
}

void tcpServerThread(void* arg)
{
  int sockd, sockd2;
  socklen_t addrlen;
  struct sockaddr_in my_name, peer_name;
  int status;

  /* create a socket */
  sockd = lwip_socket(AF_INET, SOCK_STREAM, 0);
  if (sockd == -1)
  {
    nosPrintf("Socket creation error\n");
    return;
  }

  /* server address  */
  my_name.sin_family = AF_INET;
  my_name.sin_addr.s_addr = INADDR_ANY;
  my_name.sin_port = htons(23);

  status = lwip_bind(sockd, (struct sockaddr*)&my_name, sizeof(my_name));
  if (status == -1)
  {
    nosPrintf("Binding error\n");
    return;
  }

  status = lwip_listen(sockd, 5);
  if (status == -1)
  {
    nosPrintf("Listening error\n");
    return;
  }

  nosPrintf("socket server listening\n");
  for(;;)
  {
    /* wait for a connection */
    addrlen = sizeof(peer_name);
    sockd2 = accept(sockd, (struct sockaddr*)&peer_name, &addrlen);
    if (sockd2 == -1)
    {
      nosPrintf("Wrong connection\n");
      continue;
    }

    if (posTaskCreate(tcpClientThread, (void*)(long)sockd2, 5, 512) == NULL)
       lwip_close(sockd2);

  }
}
