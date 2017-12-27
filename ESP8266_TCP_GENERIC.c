#include "ESP8266_TCP_GENERIC.h"

//INTERNAL LIBRARY VARIABLES/////////////////////////////////////
//DEBUG RELATED
static uint8_t s_esp8266_tcp_generic_debug;

//TCP RELATED
static struct espconn s_esp8266_tcp_generic_espconn;
static struct _esp_tcp s_esp8266_tcp_generic_user_tcp;

//IP / HOSTNAME RELATED
static const char* s_esp8266_tcp_generic_host_name;
static const char* s_esp8266_tcp_generic_host_ip;
static ip_addr_t s_esp8266_tcp_generic_resolved_host_ip;
static const char* s_esp8266_tcp_generic_host_path;
static uint16_t s_esp8266_tcp_generic_host_port;

//TIMER RELATED
static volatile os_timer_t s_esp8266_tcp_generic_dns_timer;
//static volatile os_timer_t s_esp8266_tcp_generic_data_timer;
//static uint32_t _esp8266_tcp_get_timer_interval;
static volatile os_timer_t s_esp8266_tcp_generic_tcp_timeout_timer;

//COUNTERS
static uint16_t s_esp8266_tcp_generic_dns_retry_count;

//BUFFER
static char* s_esp8266_tcp_generic_buffer;

//CALLBACK FUNCTION VARIABLES
static void (*s_esp8266_tcp_generic_dns_cb_function)(ip_addr_t*);
static void (*s_esp8266_tcp_generic_tcp_conn_cb)(void*);
static void (*s_esp8266_tcp_generic_tcp_discon_cb)(void*);
static void (*s_esp8266_tcp_generic_tcp_send_cb)(void*);
static void (*s_esp8266_tcp_generic_tcp_recv_cb)(void*, char*, unsigned short);
//END INTERNAL LIBRARY VARIABLES/////////////////////////////////

//INTERNAL FUNCTIONS /////////////////////////////////////
void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_dns_timer_cb(void* arg);
void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_dns_found_cb(const char* name, ip_addr_t* ipAddr, void* arg);
void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_connect_cb(void* arg);
void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_disconnect_cb(void* arg);
void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_send_cb(void* arg);
void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_receive_cb(void* arg, char* pusrdata, unsigned short length);
void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_receive_timeout_cb(void);
//END INTERNAL FUNCTIONS /////////////////////////////////////

void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SetDebug(uint8_t debug_on)
{
    //SET DEBUG PRINTF ON(1) OR OFF(0)
    
    s_esp8266_tcp_generic_debug = debug_on;
}

void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_Initialize(const char* hostname,
													    const char* host_ip,
													    uint16_t host_port,
													    const char* host_path,
                                                        uint16_t buffer_size)
{
    //INITIALIZE TCP CONNECTION PARAMETERS
	//HOSTNAME (RESOLVED THROUGH DNS IF HOST IP = NULL)
	//HOST IP
	//HOST PORT
	//HOST PATH

    s_esp8266_tcp_generic_host_name = hostname;
	s_esp8266_tcp_generic_host_ip = host_ip;
	s_esp8266_tcp_generic_host_port = host_port;
	s_esp8266_tcp_generic_host_path = host_path;

	s_esp8266_tcp_generic_dns_retry_count = 0;

    //INITIALZE REQUEST/REPLY BUFFER
    s_esp8266_tcp_generic_buffer = (char*)os_zalloc(buffer_size);

    //SET DEBUG ON
    s_esp8266_tcp_generic_debug = 1;
    
    os_printf("ESP8266 TCP_GENERIC : Initialized\n");
}

void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SetDnsServer(char num_dns, ip_addr_t* dns)
{
	//SET DNS SERVER RESOLVE HOSTNAME TO IP ADDRESS
	//MAX OF 2 DNS SERVER SUPPORTED (num_dns)

	if(num_dns == 1 || num_dns == 2)
	{
		espconn_dns_setserver(num_dns, dns);
	}
	return;
}

void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SetCallbackFunctions(void (*tcp_con_cb)(void*),
															    void (*tcp_discon_cb)(void*),
															    void (tcp_send_cb)(void*),
															    void (tcp_recv_cb)(void*, char*, unsigned short),
                                                                void (*user_dns_cb_fn)(ip_addr_t*))
{
	//HOOK FOR THE USER TO PROVIDE CALLBACK FUNCTIONS FOR
	//VARIOUS INTERNAL TCP OPERATION
	//SET THE CALLBACK FUNCTIONS FOR THE EVENTS:
	//	(1) TCP CONNECT
	//	(2) TCP DISCONNECT
	//	(3) TCP RECONNECT
	//	(4) TCP SEND
	//	(5) TCP RECEIVE
	//IF A NULL FUNCTION POINTER IS PASSED FOR THE CB OF A PARTICULAR
	//EVENT, THE DEFAULT CALLBACK FUNCTION IS CALLED FOR THAT EVENT

	//TCP CONNECT CB
	s_esp8266_tcp_generic_tcp_conn_cb = tcp_con_cb;

	//TCP DISCONNECT CB
	s_esp8266_tcp_generic_tcp_discon_cb = tcp_discon_cb;

	//TCP SEND CB
	s_esp8266_tcp_generic_tcp_send_cb = tcp_send_cb;

	//TCP RECEIVE CB
	s_esp8266_tcp_generic_tcp_recv_cb = tcp_recv_cb;

    //DNS RESOLVED CB
    s_esp8266_tcp_generic_dns_cb_function = user_dns_cb_fn;
}

void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_ResolveHostName(void)
{
    //RESOLVE PROVIDED HOSTNAME USING THE SUPPLIED DNS SERVER
	//AND CALL THE USER PROVIDED DNS DONE CB FUNCTION WHEN DONE

	//DONE ONLY IF THE HOSTNAME SUPPLIED IN INITIALIZATION FUNCTION
	//IS NOT NULL. IF NULL, USER SUPPLIED IP ADDRESS IS USED INSTEAD
	//AND NO DNS REOSLUTION IS DONE

	//SET DNS RETRY COUNTER TO ZERO
	s_esp8266_tcp_generic_dns_retry_count = 0;

	if(s_esp8266_tcp_generic_host_name != NULL)
	{
		//NEED TO DO DNS RESOLUTION
		//START THE DNS RESOLVING PROCESS AND TIMER
		struct espconn temp;
		s_esp8266_tcp_generic_resolved_host_ip.addr = 0;
		espconn_gethostbyname(&temp, s_esp8266_tcp_generic_host_name, &s_esp8266_tcp_generic_resolved_host_ip, s_esp8266_tcp_generic_dns_found_cb);
		os_timer_setfn(&s_esp8266_tcp_generic_dns_timer, (os_timer_func_t*)s_esp8266_tcp_generic_dns_timer_cb, &temp);
		os_timer_arm(&s_esp8266_tcp_generic_dns_timer, 1000, 0);
		return;
	}

	//NO NEED TO DO DNS RESOLUTION. USE USER SUPPLIED IP ADDRESS STRING
	s_esp8266_tcp_generic_resolved_host_ip.addr = ipaddr_addr(s_esp8266_tcp_generic_host_ip);

	//CALL USER SUPPLIED DNS RESOLVE CB FUNCTION
	(*s_esp8266_tcp_generic_dns_cb_function)(&s_esp8266_tcp_generic_resolved_host_ip);
}

void ICACHE_FLASH_ATTR ESP8266_TCP_GENERIC_SendAndGetReply(char* data, uint16_t len)
{
    //SEND TCP DATA + GET REPLY
}


void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_dns_timer_cb(void* arg)
{
    //DNS TIMEOUT TIMER CB FUNCTION
    //ESP8266 DNS CHECK TIMER CALLBACK FUNCTIONS
	//TIME PERIOD = 1 SEC

	//DNS TIMER CB CALLED IE. DNS RESOLUTION DID NOT WORK
	//DO ANOTHER DNS CALL AND RE-ARM THE TIMER

	s_esp8266_tcp_generic_dns_retry_count++;

	if(s_esp8266_tcp_generic_dns_retry_count == ESP8266_TCP_GENERIC_DNS_MAX_TRIES)
	{
		//NO MORE DNS TRIES TO BE DONE
		//STOP THE DNS TIMER
		os_timer_disarm(&s_esp8266_tcp_generic_dns_timer);

		if(s_esp8266_tcp_generic_debug)
		{
		    os_printf("ESP8266 TCP_GENERIC : DNS Max retry exceeded. DNS unsuccessfull\n");
		}

		//CALL USER DNS CB FUNCTION WILL NULL ARGUMENT)
		if(*s_esp8266_tcp_generic_dns_cb_function != NULL)
		{
			(*s_esp8266_tcp_generic_dns_cb_function)(NULL);
		}
		return;
	}

	if(s_esp8266_tcp_generic_debug)
	{
	    os_printf("ESP8266 TCP_GENERIC : DNS resolve timer expired. Starting another timer of 1 second...\n");
	}

	struct espconn *pespconn = arg;
	espconn_gethostbyname(pespconn, s_esp8266_tcp_generic_host_name, &s_esp8266_tcp_generic_resolved_host_ip, s_esp8266_tcp_generic_dns_found_cb);
	os_timer_arm(&s_esp8266_tcp_generic_dns_timer, 1000, 0);
}

void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_dns_found_cb(const char* name, ip_addr_t* ipAddr, void* arg)
{
    //DNS FOUND CB FUNCTION
    
    //DISABLE THE DNS TIMER
	os_timer_disarm(&s_esp8266_tcp_generic_dns_timer);

	if(ipAddr == NULL)
	{
		//HOST NAME COULD NOT BE RESOLVED
		if(s_esp8266_tcp_generic_debug)
		{
		    os_printf("ESP8266 TCP_GENERIC : hostname : %s, could not be resolved\n", s_esp8266_tcp_generic_host_name);
		}

		//CALL USER PROVIDED DNS CB FUNCTION WITH NULL PARAMETER
		if(*s_esp8266_tcp_generic_dns_cb_function != NULL)
		{
			(*s_esp8266_tcp_generic_dns_cb_function)(NULL);
		}
		return;
	}

	//DNS GOT IP
	s_esp8266_tcp_generic_resolved_host_ip.addr = ipAddr->addr;
	if(s_esp8266_tcp_generic_debug)
	{
	    os_printf("ESP8266 TCP_GENERIC : hostname : %s, resolved. IP = %d.%d.%d.%d\n", s_esp8266_tcp_generic_host_name,
                                                                                        *((uint8_t*)&s_esp8266_tcp_generic_resolved_host_ip.addr),
                                                                                        *((uint8_t*)&s_esp8266_tcp_generic_resolved_host_ip.addr + 1),
                                                                                        *((uint8_t*)&s_esp8266_tcp_generic_resolved_host_ip.addr + 2),
                                                                                        *((uint8_t*)&s_esp8266_tcp_generic_resolved_host_ip.addr + 3));
	}
	//CALL USER PROVIDED DNS CB FUNCTION
	if(*s_esp8266_tcp_generic_dns_cb_function != NULL)
	{
		(*s_esp8266_tcp_generic_dns_cb_function)(&s_esp8266_tcp_generic_resolved_host_ip);
	}
}

void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_connect_cb(void* arg)
{
    //TCP CONNECT CB

	if(s_esp8266_tcp_generic_debug)
	{
	    os_printf("ESP8266 TCP_GENERIC : TCP CONNECTED\n");
	}

	//GET THE NEW USER TCP CONNECTION
	struct espconn *pespconn = arg;

	//REGISTER SEND AND RECEIVE CALLBACKS
	espconn_regist_sentcb(pespconn, s_esp8266_tcp_generic_send_cb);
	espconn_regist_recvcb(pespconn, s_esp8266_tcp_generic_receive_cb);

	//CALL USER CALLBACK IF NOT NULL
	if(s_esp8266_tcp_generic_tcp_conn_cb != NULL)
	{
		(*s_esp8266_tcp_generic_tcp_conn_cb)(arg);
	}
}

void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_disconnect_cb(void* arg)
{
    //TCP DISCONNECT CB

    if(s_esp8266_tcp_generic_debug)
	{
	    os_printf("ESP8266 TCP_GENERIC : TCP DISCONNECTED\n");
	}

	//CALL USER CALLBACK IF NOT NULL
	if(s_esp8266_tcp_generic_tcp_discon_cb != NULL)
	{
		(*s_esp8266_tcp_generic_tcp_discon_cb)(arg);
	}
}

void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_send_cb(void* arg)
{
    //TCP SEND CB

    if(s_esp8266_tcp_generic_debug)
	{
	    os_printf("ESP8266 TCP_GENERIC : TCP DATA SENT\n");
	}

	//SET AND THE TCP GET REPLY TIMEOUT TIMER
	os_timer_setfn(&s_esp8266_tcp_generic_tcp_timeout_timer, (os_timer_func_t*)s_esp8266_tcp_generic_receive_timeout_cb, NULL);
	os_timer_arm(&s_esp8266_tcp_generic_tcp_timeout_timer, ESP8266_TCP_GENERIC_REPLY_TIMEOUT_MS, 0);
	if(s_esp8266_tcp_generic_debug)
	{
		os_printf("ESP8266 TCP_GENERIC : Started 5 second reply timeout timer\n");
	}

	//CALL USER CALLBACK IF NOT NULL
	if(s_esp8266_tcp_generic_tcp_send_cb != NULL)
	{
		(*s_esp8266_tcp_generic_tcp_send_cb)(arg);
	}
}

void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_receive_cb(void* arg, char* pusrdata, unsigned short length)
{
    //TCP RECEIVE CB

    if(s_esp8266_tcp_generic_debug)
	{
	    os_printf("ESP8266 TCP_GENERIC : TCP DATA RECEIVED\n");
	}

	//PROCESS INCOMING TCP DATA

	//CALL USER CALLBACK IF NOT NULL
	if(s_esp8266_tcp_generic_tcp_recv_cb != NULL)
	{
		(*s_esp8266_tcp_generic_tcp_recv_cb)(arg, pusrdata, length);
	}
}

void ICACHE_FLASH_ATTR s_esp8266_tcp_generic_receive_timeout_cb(void)
{
    //TCP REPLY RECEIVE TIMEOUT CB
    //IF CALLED => TCP GET REPLY NOT RECEIVED IN SET TIME

	if(s_esp8266_tcp_generic_debug)
	{
		os_printf("ESP8266 TCP_GENERIC : TCP get reply timeout !\n");
	}

	//DISCONNECT THE TCP CONNECTION TO END THE CURRENT TRANSACTION
	espconn_disconnect(&s_esp8266_tcp_generic_espconn);

	////CALL USER SPECIFIED DATA RECEIVE CALLBACK WITH NULL ARGUMENT
	//CALL USER CALLBACK IF NOT NULL
	if(s_esp8266_tcp_generic_tcp_recv_cb != NULL)
	{
		(*s_esp8266_tcp_generic_tcp_recv_cb)(NULL, NULL, 0);
	}
}