/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>

#include "netif/xadapter.h"

#include "platform.h"
#include "platform_config.h"
#include "xparameters.h"

#if defined (__arm__) || defined(__aarch64__)
#include "xil_printf.h"
#endif

#include "axidma_handler.h"
#include "gpio_handler.h"
#include "lwip/tcp.h"
#include "xil_cache.h"

#if LWIP_IPV6==1
#include "lwip/ip.h"
#else
#if LWIP_DHCP==1
#include "lwip/dhcp.h"
#endif
#endif

/* defined by each RAW mode application */
void print_app_header();
int start_application();
int transfer_data();
void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* missing declaration in lwIP */
void lwip_init();

#if LWIP_IPV6==0
#if LWIP_DHCP==1
extern volatile int dhcp_timoutcntr;
err_t dhcp_start(struct netif *netif);
#endif
#endif

XAxiDma AxiDma; /* Instance of the XAxiDma */
INTC Intc; /* Instance of the Interrupt Controller */
XGpio Gpio;

volatile int RxDone;
int Error;

extern int ETH_REQUESTED;
extern int nPulse_Flag, regVal, ADC_ACQUIRED;
extern int loop_lock;

int ADC_SIZE = DEFAULT_ADC_LENGTH;
int ADC_OFFSET = 500;

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
static struct netif server_netif;
struct netif *echo_netif;
extern struct tcp_pcb* pass;

void set_cmd(u32 Address, uint8_t* cmd, uint8_t size);
void ADC_Init();

#if LWIP_IPV6==1
void print_ip6(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf(" %x:%x:%x:%x:%x:%x:%x:%x\n\r",
			IP6_ADDR_BLOCK1(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK2(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK3(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK4(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK5(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK6(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK7(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK8(&ip->u_addr.ip6));

}
#else
void print_ip(char *msg, ip_addr_t *ip) {
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip),
			ip4_addr4(ip));
}

void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw) {

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}
#endif

#if defined (__arm__) && !defined (ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
int ProgramSi5324(void);
int ProgramSfpPhy(void);
#endif
#endif

#ifdef XPS_BOARD_ZCU102
#ifdef XPAR_XIICPS_0_DEVICE_ID
int IicPhyReset(void);
#endif
#endif

void set_cmd(u32 Address, uint8_t* cmd, uint8_t size) {
	uint8_t r_StatusReg = 0;
	uint32_t regVerify = 0;

	u32 memAddr = (UINTPTR) Address;
	Xil_Out32(memAddr + regOffset3, size);

	for (int i = 0; i < size; i++) {
		Xil_Out32(memAddr + regOffset1, cmd[i]);
		r_StatusReg = Xil_In32(memAddr + regOffset0);
		r_StatusReg |= DataIn_bit;
		Xil_Out32(memAddr + regOffset0, r_StatusReg);
		regVerify = Xil_In32(memAddr + regOffset0);
		while (regVerify & DataIn_bit) {
			regVerify = Xil_In32(memAddr + regOffset0);
		}
		r_StatusReg = regVerify;
	}

	r_StatusReg |= RdySend_bit;
	Xil_Out32(memAddr + regOffset0, r_StatusReg);
	regVerify = Xil_In32(memAddr + regOffset0);
	while (!(regVerify & Done_bit)) {
		regVerify = Xil_In32(memAddr + regOffset0);
	}
	regVerify &= 0x7B;
	r_StatusReg = regVerify;
	Xil_Out32(memAddr + regOffset0, r_StatusReg);
}

void ADC_Init() {
	u32 address = XPAR_PL_SPI_ADC_MASTERSTR_0_BASEADDR;
	uint8_t cmd[] = { 0x00, 0x02 };
	set_cmd(address, cmd, 2);
	usleep(50000);

	cmd[0] = 0x03;
	cmd[1] = 0x03;
	set_cmd(address, cmd, 2);
	usleep(50000);

	cmd[0] = 0x25;
	cmd[1] = 0xC0;
	set_cmd(address, cmd, 2);
	usleep(50000);

	cmd[0] = 0x43;
	cmd[1] = 0x10;
	set_cmd(address, cmd, 2);
	usleep(50000);
}


int main() {
#if LWIP_IPV6==0
	ip_addr_t ipaddr, netmask, gw;

#endif
	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] =
			{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	echo_netif = &server_netif;

	int Status;
	u16 *RxBufferPtr;

	RxBufferPtr = (u16 *) RX_BUFFER_BASE;
#if defined (__arm__) && !defined (ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
	ProgramSi5324();
	ProgramSfpPhy();
#endif
#endif

	xil_printf("\r\n--- Configuring DMA --- \r\n");
	DMADeviceSetup(DMA_DEV_ID);

	/* Disable all interrupts before setup */
	DisableIntrSystem();

	/* Initialize flags before start transfer test  */
	RxDone = 0;
	Error = 0;

	//	Xil_DCacheDisable();
	//	Initialize Buffer
	memset((UINTPTR) RxBufferPtr, NULL, MAX_PKT_LEN);

	/* Flush the buffers before the DMA transfer, in case the Data Cache
	 * is enabled
	 */
	Xil_DCacheFlushRange((UINTPTR) RxBufferPtr, MAX_PKT_LEN);

	Xil_DCacheInvalidateRange((UINTPTR) RxBufferPtr, MAX_PKT_LEN);
	//	SetupIntrSystem(&Intc, &AxiDma, &RxIntrHandler, RX_INTR_ID);
	//

	//
	//
	//	/* Enable all interrupts before setup */
	//	EnableIntrSystem();

	/* Define this board specific macro in order perform PHY reset on ZCU102 */
#ifdef XPS_BOARD_ZCU102
	if (IicPhyReset()) {
		xil_printf("Error performing PHY reset \n\r");
		return -1;
	}
#endif
	ADC_Init();
	init_platform();

#if LWIP_IPV6==0
#if LWIP_DHCP==1
	ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;
#else
	/* initialize IP addresses to be used */
	IP4_ADDR(&ipaddr, 192, 168, 1, 10);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
	IP4_ADDR(&gw, 192, 168, 1, 1);
#endif
#endif
	print_app_header();

	lwip_init();

#if (LWIP_IPV6 == 0)
	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(echo_netif, &ipaddr, &netmask, &gw, mac_ethernet_address,
	PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}
#else
	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(echo_netif, NULL, NULL, NULL, mac_ethernet_address,
					PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}
	echo_netif->ip6_autoconfig_enabled = 1;

	netif_create_ip6_linklocal_address(echo_netif, 1);
	netif_ip6_addr_set_state(echo_netif, 0, IP6_ADDR_VALID);

	print_ip6("\n\rBoard IPv6 address ", &echo_netif->ip6_addr[0].u_addr.ip6);

#endif
	netif_set_default(echo_netif);

	/* now enable interrupts */
	platform_enable_interrupts();

	/* specify that the network if is up */
	netif_set_up(echo_netif);

#if (LWIP_IPV6 == 0)
#if (LWIP_DHCP==1)
	/* Create a new DHCP client for this interface.
	 * Note: you must call dhcp_fine_tmr() and dhcp_coarse_tmr() at
	 * the predefined regular intervals after starting the client.
	 */
	dhcp_start(echo_netif);
	dhcp_timoutcntr = 24;

	while (((echo_netif->ip_addr.addr) == 0) && (dhcp_timoutcntr > 0))
		xemacif_input(echo_netif);

	if (dhcp_timoutcntr <= 0) {
		if ((echo_netif->ip_addr.addr) == 0) {
			xil_printf("DHCP Timeout\r\n");
			xil_printf("Configuring default IP of 192.168.1.10\r\n");
			IP4_ADDR(&(echo_netif->ip_addr), 192, 168, 1, 10);
			IP4_ADDR(&(echo_netif->netmask), 255, 255, 255, 0);
			IP4_ADDR(&(echo_netif->gw), 192, 168, 1, 1);
		}
	}

	ipaddr.addr = echo_netif->ip_addr.addr;
	gw.addr = echo_netif->gw.addr;
	netmask.addr = echo_netif->netmask.addr;
#endif

	print_ip_settings(&ipaddr, &netmask, &gw);

#endif

	/* Initialize the GPIO driver. If an error occurs then exit */
	Status = GpioIntrInit(&Intc, &Gpio, GPIO_DEVICE_ID, INTC_GPIO_INTERRUPT_ID,
	GPIO_CHANNEL1);

//	Xil_DCacheFlushRange((UINTPTR) RxBufferPtr, MAX_PKT_LEN);
//	memset(RxBufferPtr, NULL, ADC_SIZE);

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* start the application (web server, rxtest, txtest, etc..) */
	start_application();
	int i = 0;
	/* receive and process packets */
	while (1) {
		if (TcpFastTmrFlag) {
			tcp_fasttmr();
			TcpFastTmrFlag = 0;
		}
		if (TcpSlowTmrFlag) {
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;
		}
		xemacif_input(echo_netif);



		if (ETH_REQUESTED && ADC_ACQUIRED) {

			err_t err;

			//			int data = Xil_In16(RX_BUFFER_BASE + (i * 2)) & 0xFFF;
			int data = RxBufferPtr[i] & 0xFFF;

			// TODO : interupt handler for higher throughput
			/*
			 * DATA Format : "x100:x101:..."
			 * */

			/*convert integer to sring*/
			int size = snprintf(NULL, 0, "%d:", data);
			char* temp = (char*) malloc(size);
			sprintf(temp, "%d:", data);

			err = tcp_write(pass, temp, size, 0x01);

			while (err == ERR_MEM)
				err = tcp_write(pass, temp, size, 0x01);

			//			 check data bytesize < TCP_SND_BUF

			if (pass->snd_buf <= 0x3B00 || i >= ADC_SIZE - 1) {
				err = tcp_output(pass);
			}

			if (err != ERR_OK)
				err = tcp_output(pass);

			i++;

			free(temp);

			if (i > ADC_SIZE) {
				// Sent data termination
				int size = snprintf(NULL, 0, "\n", NULL);
				char* temp = (char*) malloc(size);
				sprintf(temp, "\n", NULL);
				err = tcp_write(pass, temp, size, 0x01);
				err = tcp_output(pass);

				i = 0;
				ADC_ACQUIRED = 0;
				ETH_REQUESTED = 0;
				Xil_DCacheInvalidateRange((UINTPTR) RxBufferPtr,
				MAX_PKT_LEN);

			}
		}
	}

	/* never reached */
	cleanup_platform();

	return 0;
}
