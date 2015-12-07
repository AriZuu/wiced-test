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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/sys.h"
#include "lwip/dns.h"

#include "lwip/stats.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include <lwip/dhcp.h>
#include "lwip/tcpip.h"
#include "lwip/ip_addr.h"

#include "wwd_management.h"
#include "wwd_wifi.h"
#include "wwd_network.h"

#include "eshell.h"
#include "eshell-commands.h"
#include "wiced_test.h"

#define MAGIC 0x1942
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base @ of Sector 1, 16 Kbytes */

typedef struct {

  uint32_t magic;
  uint32_t len;
  char ap[40];
  char pass[40];
} Config;

static Config config;
static bool alreadyJoined = false;


void checkConfig()
{
  const Config* cf = (const Config*)0x08004000; // flash sector 1

  if (cf->magic != MAGIC || cf->len != sizeof(Config)) {

    printf("Configuration data invalid, using defaults.\n");
    memset(&config, '\0', sizeof(Config));
    config.magic = MAGIC;
    config.len = sizeof(Config);
  }
  else
    memcpy(&config, cf, sizeof(Config));
}

void ifStatusCallback(struct netif *netif);

/*
 * This is called by lwip when interface status changes.
 */
void ifStatusCallback(struct netif *netif)
{
  if (netif_is_up(netif)) {

    printf("netif up, ip %s\n", inet_ntoa(netif->ip_addr));
  }
  else {

    printf("netif down\n");
  }
}

struct netif defaultIf;

static void addEthernetIf()
{
  ip_addr_t ipaddr, netmask, gw;

#if LWIP_DHCP != 0

  ip_addr_set_zero( &gw );
  ip_addr_set_zero( &ipaddr );
  ip_addr_set_zero( &netmask );

#else

  IP4_ADDR(&gw, 192,168,61,1);
  IP4_ADDR(&ipaddr, 192,168,61,55);
  IP4_ADDR(&netmask, 255,255,255,0);

#endif

  netifapi_netif_add(&defaultIf,
                     &ipaddr,
                     &netmask,
                     &gw,
                     WWD_STA_INTERFACE,
                     ethernetif_init,
                     tcpip_input);

  netifapi_netif_set_default(&defaultIf);

#if LWIP_DHCP != 0

  netifapi_netif_set_up(&defaultIf);
  netif_set_status_callback(&defaultIf, ifStatusCallback);
  netifapi_dhcp_start(&defaultIf);

#else

  netif_set_status_callback(&defaultIf, ifStatusCallback);
  netifapi_netif_set_up(&defaultIf);

  ip_addr_t dns;

  IP4_ADDR(&dns, 192,168,61,10);
  dns_setserver(0, &dns);

#endif

#if LWIP_IPV6
  netifapi_netif_create_ip6_linklocal_address(&defaultIf, 1);
#endif

  printf("Ethernet IF added.\n");
}

static int join()
{
  wiced_ssid_t ssid;
 
  /*
   * As AP network name and password and attempt to join.
   */
  strcpy((char*)ssid.value, config.ap);
  ssid.length = strlen(config.ap);

  if (wwd_wifi_join(&ssid, WICED_SECURITY_WPA2_MIXED_PSK, (uint8_t*)config.pass, strlen(config.pass), NULL) != WWD_SUCCESS) {

    config.ap[0] = '\0';
    config.pass[0] = '\0';
    return -1;
  }

  printf("Join OK.\n");
  addEthernetIf();
  alreadyJoined = true;
  return 0;
}

static int ap(EshContext* ctx)
{

  eshCheckNamedArgsUsed(ctx);
  char* ap = eshNextArg(ctx, true);
  char* pass = eshNextArg(ctx, true);

  eshCheckArgsUsed(ctx);
  if (eshArgError(ctx) != EshOK)
    return -1;

  if (ap == NULL || pass == NULL) {

    eshPrintf(ctx, "Usage: ap [--save] ap pass\n");
    return -1;
  }

  if (alreadyJoined) {

    eshPrintf(ctx, "Already joined Wifi network.\n");
    return -1;
  }

  strcpy(config.ap, ap);
  strcpy(config.pass, pass);
  eshPrintf(ctx, "Joining %s with password %s\n", config.ap, config.pass);
  if (join() == -1)
    eshPrintf(ctx, "Join failed.\n");

  return 0;
}

void checkAP()
{
  if (strlen(config.ap) > 0 && strlen(config.pass) > 0) {

    printf("Joining %s...\n", config.ap);
    join();
  }
}

static int wr(EshContext* ctx)
{
  uint32_t address;
  uint32_t* data;

  eshCheckNamedArgsUsed(ctx);

  eshCheckArgsUsed(ctx);
  if (eshArgError(ctx) != EshOK)
    return -1;

  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR); 

  if (FLASH_EraseSector(FLASH_Sector_1, VoltageRange_3) != FLASH_COMPLETE) {

    FLASH_Lock(); 
    eshPrintf(ctx, "Config flash erase failed.\n");
    return -1;
  }

  data = (uint32_t*)&config;

  for (address = ADDR_FLASH_SECTOR_1; address < ADDR_FLASH_SECTOR_1 + sizeof(Config); address += 4) {

    if (FLASH_ProgramWord(address, *data) != FLASH_COMPLETE) {

      eshPrintf(ctx, "Config flash write failed.\n");
      break;
    }

    ++data;
  }

  FLASH_Lock(); 
  return 0;
}

const EshCommand apCommand = {
  .name = "ap",
  .help = "ap pass\nassociate with given wifi access point.",
  .handler = ap
}; 

const EshCommand wrCommand = {
  .name = "wr",
  .help = "save settings to flash",
  .handler = wr
}; 

const EshCommand *eshCommandList[] = {

  &apCommand,
  &wrCommand,
  &eshPingCommand,
  &eshHelpCommand,
  &eshExitCommand,
  NULL
};

