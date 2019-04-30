#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "sys/ctimer.h"
#include <stdio.h>
#include <string.h>
#include "net/uip-debug.h"

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
#define UDP_EXAMPLE_ID  190
#define DEBUG DEBUG_PRINT
#define PERIOD 60
#define START_INTERVAL		(15 * CLOCK_SECOND)
#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))
#define MAX_PAYLOAD_LEN		30

static struct uip_udp_conn *reply_conn;
static uip_ipaddr_t server_ipaddr;
static char *data;

/*---------------------------------------------------------------------------*/

// Inicia o Processo
PROCESS(udp_reply_process, "Replica a mensagem para o nó de destino");
AUTOSTART_PROCESSES(&udp_reply_process);

/*---------------------------------------------------------------------------*/

// Funções
static void send_packet(void *ptr);
static void receive_packet(void);
static void print_local_addresses(void);

PROCESS_THREAD(udp_reply_process, ev, data) { 
  
  // Declaração de variáveis
  uip_ipaddr_t ipaddr; //, ipdest;
  static struct etimer periodic;
  static struct ctimer backoff_timer;
  
  PROCESS_BEGIN();
  PROCESS_PAUSE();
  
  // Configura o IP do Nó
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
  uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
  PRINTF("UDP reply process started\n");
  print_local_addresses();

  // Configura o IP do Nó Destino
  // uip_ip6addr(&ipdest, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);

  // Configura uma nova conexão com o host de destino
  reply_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL); 
  if(reply_conn == NULL) {
    PRINTF("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(reply_conn, UIP_HTONS(UDP_CLIENT_PORT));
  PRINTF("Created a connection with the server ");
  PRINT6ADDR(&reply_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n", UIP_HTONS(reply_conn->lport), UIP_HTONS(reply_conn->rport));

  // Envia a mensagem a cada intervalo
  etimer_set(&periodic, SEND_INTERVAL);
  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) receive_packet();
    if(etimer_expired(&periodic)) {
      etimer_reset(&periodic);
      ctimer_set(&backoff_timer, SEND_TIME, send_packet, NULL);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

// Envia os dados para o nó destino
static void send_packet(void *ptr) {
  char buf[MAX_PAYLOAD_LEN];
  PRINTF("DATA send to %d: '%s'\n", server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1], data);
  uip_udp_packet_sendto(reply_conn, buf, strlen(buf), &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
}

/*---------------------------------------------------------------------------*/

// Recebe os dados e printa na tela
static void receive_packet(void) {
  char *str;
  if(uip_newdata()) {
    str = uip_appdata;
    str[uip_datalen()] = '\0';
    printf("DATA recv: '%s'\n", str);
    data = str;
  }
}

/*---------------------------------------------------------------------------*/

static void print_local_addresses(void) {
  int i;
  uint8_t state;
  PRINTF("Reply IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
	      uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}

/*---------------------------------------------------------------------------*/