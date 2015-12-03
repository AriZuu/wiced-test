/*
 * Copyright (c) 2006-2015, Ari Suutari <ari@stonepile.fi>.
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
#include <picoos-u.h>
#include <picoos-lwip.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <eshell.h>

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/sys.h"

#include "lwip/stats.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include <lwip/dhcp.h>
#include "lwip/tcpip.h"
#include "lwip/ip_addr.h"

#include "wwd_management.h"
#include "wwd_wifi.h"
#include "wwd_network.h"
#include "wwd_crypto.h"

extern const UosRomFile romFiles[];

void tcpServerThread(void*);

static wiced_mac_t   myMac             = { {  0, 0, 0, 0, 0, 0 } };

/*
 * This is called by lwip when basic initialization has been completed.
 */
static void tcpipInitDone(void *arg)
{
  wwd_result_t result;
  sys_sem_t *sem;
  sem = (sys_sem_t *)arg;

  printf("Initializing Wifi.\n");

/*
 * Bring WIFI up.
 */
  while ((result = wwd_management_init(WICED_COUNTRY_FINLAND, NULL)) != WWD_SUCCESS) {

    printf("WWD init error %d\n", result);
    posTaskSleep(MS(30000));
  }


  wwd_wifi_get_mac_address(&myMac, WWD_STA_INTERFACE);

  printf("Mac addr is %02x:%02x:%02x:%02x:%02x:%02x\n", myMac.octet[0],
                myMac.octet[1], myMac.octet[2], myMac.octet[3],
                myMac.octet[4], myMac.octet[5]);

  uint32_t r;

  wwd_wifi_get_random(&r, sizeof(r));
  sys_random_init(r);

  /*
 * Signal main thread that we are done.
 */
  sys_sem_signal(sem);
}

static void mainTask(void* arg)
{
  sys_sem_t sem;

  uosInit();
  uosBootDiag();
  lwipMount();
/* 
 * Provide a filesystem which contains Wifi firmware to Wiced driver.
 */
  uosMountRom("/firmware", romFiles);

  if(sys_sem_new(&sem, 0) != ERR_OK) {
    LWIP_ASSERT("Failed to create semaphore", 0);
  }

/*
 * Bring LwIP & Wifi up.
 */
  tcpip_init(tcpipInitDone, &sem);
  sys_sem_wait(&sem);
  nosPrintf("TCP/IP initialized.\n");

/*
 * Start simple hello word server.
 */
  nosTaskCreate(tcpServerThread, NULL, 6, 1400, "accept");

  eshConsole();

#if 0
/*
 * Display task resource usage each minute.
 */
  while (1) {

   posTaskSleep(MS(60000));
   uosResourceDiag();
#if LWIP_STATS_DISPLAY
   stats_display();
#endif
  }
#endif 
}


int main(int argc, char **argv)
{
  GPIO_InitTypeDef GPIO_InitStructure;

#if PORTCFG_CON_USART == 1

  // Enable USART clock.

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

  // Configure usart2 pins.

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

#endif

  nosInit(mainTask, NULL, 1, 20000, 512);
  return 0;
}
