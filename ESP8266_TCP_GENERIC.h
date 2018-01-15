/****************************************************************
* ESP8266 TCP GENERIC LIBRARY
*
* FOR NOW ONLY SUPPORTS SINGLE CONNECTION
*
* DECEMBER 26 2017
*
* ANKIT BHATNAGAR
* ANKIT.BHATNAGARINDIA@GMAIL.COM
*
* REFERENCES
* ------------
* 	(1) https://espressif.com/en/support/explore/sample-codes
****************************************************************/

#ifndef _ESP8266_TCP_GENERIC_H_
#define _ESP8266_TCP_GENERIC_H_

#include "osapi.h"
#include "mem.h"
#include "ets_sys.h"
#include "ip_addr.h"
#include "espconn.h"
#include "os_type.h"

#define ESP8266_TCP_GENERIC_DNS_MAX_TRIES		5
#define ESP8266_TCP_GENERIC_REPLY_TIMEOUT_MS	5000

//CUSTOM VARIABLE STRUCTURES/////////////////////////////
//END CUSTOM VARIABLE STRUCTURES/////////////////////////

//FUNCTION PROTOTYPES/////////////////////////////////////
//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SetDebug(uint8_t debug_on);
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_Initialize(const char* hostname,
													    const char* host_ip,
													    uint16_t host_port,
													    const char* host_path,
                                                        uint16_t buffer_size);
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SetDnsServer(char num_dns, ip_addr_t* dns);
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SetCallbackFunctions(void (*tcp_con_cb)(void*),
															    void (*tcp_discon_cb)(void*),
															    void (tcp_send_cb)(void*),
															    void (tcp_recv_cb)(char*, unsigned short),
                                                                void (*user_dns_cb_fn)(ip_addr_t*));

//GET PARAMETERS FUNCTIONS


//CONTROL FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_ResolveHostName(void);
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_Connect(void);
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SendAndGetReply(uint8_t* data, uint16_t len);
void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_Disonnect(void);
//END FUNCTION PROTOTYPES/////////////////////////////////

#endif
