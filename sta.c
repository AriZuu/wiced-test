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

static bool alreadyJoined = false;

#define MAGIC 0x1942
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base @ of Sector 1, 16 Kbytes */

typedef struct {

  union {
    int aliger;
    char key[UOS_CONFIG_KEYSIZE];
  };

  char value[UOS_CONFIG_VALUESIZE];
} KV;

typedef struct {

  uint32_t magic;
  uint32_t len;
} ConfigHeader;

typedef struct {

  ConfigHeader hdr;
  KV kv[UOSCFG_CONFIG_PREALLOC];
} Config;

void initConfig()
{
  const Config* cf = (const Config*)0x08004000; // flash sector 1

  uosConfigInit();
  if (cf->hdr.magic != MAGIC || cf->hdr.len != sizeof(Config)) {

    printf("Configuration data invalid, using defaults.\n");
  }
  else {

    int i;
    const KV* kv;

    kv = cf->kv;
    for (i = 0; i < UOSCFG_CONFIG_PREALLOC; i++, kv++) {

      if (kv->key[0] == '\0')
        break;

      uosConfigSet(kv->key, kv->value);
    }
  }
}

static uint32_t address;

static int wrSaver(void*context, const char* key, const char* value)
{
  KV kv;
  uint32_t* data;
  uint32_t byte;

  memset(&kv, '\0', sizeof(kv));
  strcpy(kv.key, key);
  strcpy(kv.value, value);

  data = (uint32_t*)&kv;

  for (byte = 0; byte < sizeof(KV); byte += 4, address += 4, data++) {

    if (FLASH_ProgramWord(address, *data) != FLASH_COMPLETE) {

      return -1;
    }
  }

  return 0;
}

static int wr(EshContext* ctx)
{
  uint32_t byte;
  uint32_t* data;
  ConfigHeader hdr;

  eshCheckNamedArgsUsed(ctx);

  eshCheckArgsUsed(ctx);
  if (eshArgError(ctx) != EshOK)
    return -1;

  memset(&hdr, '\0', sizeof(ConfigHeader));
  hdr.magic = MAGIC;
  hdr.len = sizeof(Config);
 
  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR); 

  if (FLASH_EraseSector(FLASH_Sector_1, VoltageRange_3) != FLASH_COMPLETE) {

    FLASH_Lock(); 
    eshPrintf(ctx, "Config flash erase failed.\n");
    return -1;
  }

  address = ADDR_FLASH_SECTOR_1;

  data = (uint32_t*)&hdr;
  for (byte = 0; byte < sizeof(ConfigHeader); byte += 4, address += 4, data++) {

    if (FLASH_ProgramWord(address, *data) != FLASH_COMPLETE) {

      eshPrintf(ctx, "Config flash write failed.\n");
      break;
    }
  }

  if (uosConfigSaveEntries(NULL, wrSaver) == -1) {

    eshPrintf(ctx, "Config flash write failed.\n");
  }

  wrSaver(NULL, "", "");
  FLASH_Lock(); 
  return 0;
}

static int clear(EshContext* ctx)
{
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

  FLASH_Lock(); 
  return 0;
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

static int wifiUp()
{
  wiced_ssid_t ssid;
 
  /*
   * As AP network name and password and attempt to join.
   */

  const char* ap = uosConfigGet("ap");
  const char* pass = uosConfigGet("pass");

  strcpy((char*)ssid.value, ap);
  ssid.length = strlen(ap);

  if (wwd_wifi_join(&ssid, WICED_SECURITY_WPA2_MIXED_PSK, (uint8_t*)pass, strlen(pass), NULL) != WWD_SUCCESS)
    return -1;

  printf("Join OK.\n");
  addEthernetIf();

  alreadyJoined = true;
  wifiLed(true);

  return 0;
}

static void wifiDown()
{
  wifiLed(false);

#if LWIP_DHCP != 0

  dhcp_stop(&defaultIf);

#endif

  netif_set_down(&defaultIf);
  wwd_wifi_leave(WWD_STA_INTERFACE);
}

static int sta(EshContext* ctx)
{
  char* reset = eshNamedArg(ctx, "reset", false);
  char* ap;
  char* pass;

  eshCheckNamedArgsUsed(ctx);

  if (reset == NULL) {

    ap = eshNextArg(ctx, true);
    pass = eshNextArg(ctx, true);
  }

  eshCheckArgsUsed(ctx);
  if (eshArgError(ctx) != EshOK)
    return -1;

  if (reset) {

    if (alreadyJoined) {

      wifiDown();
      eshPrintf(ctx, "Left ap.\n");
      alreadyJoined = false;
    }

    uosConfigSet("ap", "");
    uosConfigSet("pass", "");
    return 0;
  }

  if (ap == NULL || pass == NULL) {

    eshPrintf(ctx, "Usage: sta --reset | ap pass\n");
    return -1;
  }

  if (alreadyJoined) {

    eshPrintf(ctx, "Already joined Wifi network.\n");
    return -1;
  }

  uosConfigSet("ap", ap);
  uosConfigSet("pass", pass);

  eshPrintf(ctx, "Joining %s with password %s\n", ap, pass);
  if (wifiUp() == -1)
    eshPrintf(ctx, "Join failed.\n");

  return 0;
}

static void joinThread(void* arg)
{
  const char* ap;
  const char* pass;

  do {
    
    ap = uosConfigGet("ap");
    pass = uosConfigGet("pass");

    posTaskSleep(MS(5000));

  } while (ap &&
           pass &&
           !alreadyJoined &&
           wifiUp() == -1);

  printf("Background join exiting.\n");
}

void checkAP()
{
  const char* ap = uosConfigGet("ap");
  const char* pass = uosConfigGet("pass");

  if (ap[0] != '\0' && pass[0] != '\0') {

    printf("Joining %s.\n", ap);
    if (wifiUp() == -1) {

       printf("Join failed, retrying join in background.\n");
       posTaskCreate(joinThread, NULL, 5, 1024);
    }
  }
}

const EshCommand staCommand = {
  .name = "sta",
  .help = "--reset | ap pass\ndisassociate/associate with given wifi access point.",
  .handler = sta
}; 

const EshCommand wrCommand = {
  .name = "wr",
  .help = "save settings to flash",
  .handler = wr
}; 

const EshCommand clearCommand = {
  .name = "clear",
  .help = "clear saved settings from flash",
  .handler = clear
}; 

const EshCommand *eshCommandList[] = {

  &staCommand,
  &wrCommand,
  &clearCommand,
  &eshTsCommand,
  &eshPingCommand,
  &eshHelpCommand,
  &eshExitCommand,
  NULL
};

