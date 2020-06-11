/* standard library headers */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

/* BG stack headers */
#include "bg_types.h"
#include "gecko_bglib.h"

/* Own header */
#include "app.h"
#include "dump.h"
#include "support.h"
#include "common.h"

// App booted flag
static bool appBooted = false;
static struct {
  char *name, *filename,*address_filename;
  uint32 advertising_interval;
  uint16 connection_interval, mtu; 
  bd_addr remote;
  uint8 advertise, connection, io_cap, sm_conf, privacy, debug;
} config = { .remote = { .addr = {0,0,0,0,0,0}},
	     .connection = 0xff,
	     .advertise = 1,
	     .name = NULL,
	     .filename = NULL,
	     .address_filename = NULL,
	     .advertising_interval = 160, // 100 ms
	     .connection_interval = 80, // 100 ms
	     .mtu = 23,
	     .io_cap = sm_io_capability_noinputnooutput,
	     .sm_conf = 2,
	     .debug = 0,
	     .privacy = 0,
};
  
const char *getAppOptions(void) {
  return "a<remote-address>b<address-filename>i<interval-ms>mskdyf<passkey-filename>pg";
}

void save_address(void) {
  struct gecko_msg_system_get_bt_address_rsp_t *resp;
  resp = gecko_cmd_system_get_bt_address();
  printf("save_address()\n");
  char buf[strlen(config.address_filename)+5];
  sprintf(buf,"%s.tmp",config.address_filename);
  printf("buf: '%s'\n",buf);
  FILE *fh;
  assert(fh = fopen(buf,"w"));
  assert(1 == fwrite(&resp->address,6,1,fh));
  fclose(fh);
  assert(0 == rename(buf,config.address_filename));
}

void save_passkey(uint32 passkey) {
  char buf[strlen(config.filename)+5];
  sprintf(buf,"%s.tmp",config.filename);
  FILE *fh;
  assert(fh = fopen(buf,"w"));
  assert(1 == fwrite(&passkey,sizeof(passkey),1,fh));
  fclose(fh);
  assert(0 == rename(buf,config.filename));
}

uint32 load_passkey() {
  uint32 passkey;
  FILE *fh;
  do {
    millisleep(1);
    fh = fopen(config.filename,"r");
  } while(!fh);
  assert(1 == fread(&passkey,sizeof(passkey),1,fh));
  fclose(fh);
  unlink(config.filename);
  return passkey;
}

void appOption(int option, const char *arg) {
  double dv;
  switch(option) {
  case 'f':
    config.filename = strdup(arg);
    break;
  case 'a':
    parse_address(arg,&config.remote);
    config.advertise = 0;
    break;
  case 'b':
    config.address_filename = strdup(arg);
    break;
  case 'i':
    sscanf(arg,"%lf",&dv);
    config.advertising_interval = round(dv/0.625);
    config.connection_interval = round(dv/1.25);
    break;
  case 'm':
    config.sm_conf |= 1;
    break;
  case 's':
    config.sm_conf |= 4;
    break;
  case 'y':
    config.io_cap = sm_io_capability_displayyesno;
    break;
  case 'd':
    if(sm_io_capability_noinputnooutput == config.io_cap) {
      config.io_cap = sm_io_capability_displayonly;
      break;
    }
    if(sm_io_capability_keyboardonly == config.io_cap) {
      config.io_cap = sm_io_capability_keyboarddisplay;
      break;
    }
    if(sm_io_capability_displayyesno == config.io_cap) {
      break;
    }
    fprintf(stderr,"Error: -d with io_cap = %d\n",config.io_cap);
    exit(1);
    break;
  case 'k':
    if(sm_io_capability_noinputnooutput == config.io_cap) {
      config.io_cap = sm_io_capability_keyboardonly;
      break;
    }
    if(sm_io_capability_displayonly == config.io_cap) {
      config.io_cap = sm_io_capability_keyboarddisplay;
      break;
    }
    fprintf(stderr,"Error: -k with io_cap = %d\n",config.io_cap);
    exit(1);
    break;
  case 'g':
    config.debug = 1;
    break;
  case 'p':
    config.privacy = 1;
    break;
  default:
    fprintf(stderr,"Unhandled option '-%c'\n",option);
    exit(1);
  }
}

void appInit(void) {
  if(config.filename) {
    if(config.advertise) return;
    for(int i = 0; i < 6; i++) {
      if(config.remote.addr[i]) return;
    }
  }
  printf("Usage: bg-ncp-bonding -f filename [ -a <address> ]\n");
  exit(1);
}

/***********************************************************************************************//**
 *  \brief  Event handler function.
 *  \param[in] evt Event pointer.
 **************************************************************************************************/
void appHandleEvents(struct gecko_cmd_packet *evt)
{
  if (NULL == evt) {
    return;
  }

  // Do not handle any events until system is booted up properly.
  if ((BGLIB_MSG_ID(evt->header) != gecko_evt_system_boot_id)
      && !appBooted) {
#if defined(DEBUG)
    printf("Event: 0x%04x\n", BGLIB_MSG_ID(evt->header));
#endif
    millisleep(50);
    return;
  }

  /* Handle events */
#ifdef DUMP
  switch (BGLIB_MSG_ID(evt->header)) {
  default:
    dump_event(evt);
  }
#endif
  switch (BGLIB_MSG_ID(evt->header)) {
  case gecko_evt_system_boot_id: /*********************************************************************************** system_boot **/
#define ED evt->data.evt_system_boot
    appBooted = true;
    if(config.address_filename) save_address();
    gecko_cmd_sm_configure(config.sm_conf,config.io_cap);
    if(config.debug) gecko_cmd_sm_set_debug_mode(1);
    if(config.advertise) {
      uint8 discoverable_mode = le_gap_general_discoverable;
      gecko_cmd_le_gap_set_advertise_timing(0,config.advertising_interval,config.advertising_interval,0,0);
      gecko_cmd_le_gap_start_advertising(0,discoverable_mode,le_gap_connectable_scannable);
    } else {
      if(config.privacy) gecko_cmd_le_gap_set_privacy_mode(1,1);
      gecko_cmd_le_gap_connect(config.remote,le_gap_address_type_public,le_gap_phy_1m);
    }
    break;
#undef ED

  case gecko_evt_le_connection_opened_id: /***************************************************************** le_connection_opened **/
#define ED evt->data.evt_le_connection_opened
    config.connection = ED.connection;
    gecko_cmd_sm_increase_security(ED.connection);
    break;
#undef ED

  case gecko_evt_gatt_mtu_exchanged_id: /********************************************************************* gatt_mtu_exchanged **/
#define ED evt->data.evt_gatt_mtu_exchanged
    config.mtu = ED.mtu;
    break;
#undef ED

  case gecko_evt_le_connection_closed_id: /***************************************************************** le_connection_closed **/
#define ED evt->data.evt_le_connection_closed
    exit(0);
    break;
#undef ED

  case gecko_evt_sm_bonded_id: /*************************************************************************************** sm_bonded **/
#define ED evt->data.evt_sm_bonded
    gecko_cmd_le_connection_close(ED.connection);
    break;
#undef ED

  case gecko_evt_sm_bonding_failed_id: /*********************************************************************** sm_bonding_failed **/
#define ED evt->data.evt_sm_bonding_failed
    gecko_cmd_le_connection_close(ED.connection);    
    break;
#undef ED

  case gecko_evt_sm_passkey_display_id: /********************************************************************* sm_passkey_display **/
#define ED evt->data.evt_sm_passkey_display
    save_passkey(ED.passkey);
    break;
#undef ED

  case gecko_evt_sm_passkey_request_id: /********************************************************************* sm_passkey_request **/
#define ED evt->data.evt_sm_passkey_request
    gecko_cmd_sm_enter_passkey(ED.connection,load_passkey());
    break;
#undef ED

  default:
    break;
  }
}
