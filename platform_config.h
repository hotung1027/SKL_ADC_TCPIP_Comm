#ifndef __PLATFORM_CONFIG_H_
#define __PLATFORM_CONFIG_H_
#include "xscugic.h"

#ifdef XPAR_XEMACPS_3_BASEADDR
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_3_BASEADDR
#endif
#ifdef XPAR_XEMACPS_2_BASEADDR
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_2_BASEADDR
#endif
#ifdef XPAR_XEMACPS_1_BASEADDR
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_1_BASEADDR
#endif
#ifdef XPAR_XEMACPS_0_BASEADDR
#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_0_BASEADDR
#endif

#ifdef XPAR_AXI_TIMER_0_BASEADDR
#define PLATFORM_TIMER_BASEADDR XPAR_AXI_TIMER_0_BASEADDR
#define PLATFORM_TIMER_INTERRUPT_INTR XPAR_AXI_TIMER_0_INTR
#define PLATFORM_TIMER_INTERRUPT_MASK (1 << XPAR_AXI_TIMER_0_INTR)
#endif

/*
 * Device hardware build related constraints
 * */

#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID

#ifdef XPAR_AXI_7SDDR_0_S_AXI_BASEADDR
#define DDR_BASE_ADDR	XPAR_AXI_7SDDR_0_S_AXI_BASEADDR
#elif defined (XPAR_MIG7SERIES_0_BASEADDR)
#define DDR_BASE_ADDR	XPAR_MIG7SERIES_0_BASEADDR
#elif defined (XPAR_MIG_0_C0_DDR4_MEMORY_MAP_BASEADDR)
#define DDR_BASE_ADDR	XPAR_MIG_0_C0_DDR4_MEMORY_MAP_BASEADDR
#elif defined (XPAR_PSU_DDR_0_S_AXI_BASEADDR)
#define DDR_BASE_ADDR	XPAR_PSU_DDR_0_S_AXI_BASEADDR
#endif

/*
 warning CHECK FOR THE VALID DDR ADDRESS IN XPARAMETERS.H, \
		 DEFAULT SET TO 0x01000000
 */
/*
 * Buffer and Buffer Descriptor related constant definition
 */
#define PKT_SIZE 0x100 // Burst Size
#define DEFAULT_ADC_LENGTH 10000
// MaxTransferLen define by Width Buffer Length

#define BUFFER_SIZE 	 1 << XPAR_AXIDMA_0_c_sg_length_width
#define MAX_PKT_LEN		BUFFER_SIZE
#define NUMBER_OF_TRANSFERS	10

#ifndef DDR_BASE_ADDR



#define MEM_BASE_ADDR		XPAR_PS7_DDR_0_S_AXI_BASEADDR
#else
#define MEM_BASE_ADDR		(DDR_BASE_ADDR + 0x00100000)
#endif

#define TX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00100000)
#define RX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00300000)
#define RX_BUFFER_HIGH		(MEM_BASE_ADDR + 0x004FFFFF)

/* Timeout loop counter for reset
 */
#define RESET_TIMEOUT_COUNTER	10000
#define TEST_START_VALUE	0xC



#define TEST_START_VALUE	0xC



#define regOffset0  0
#define regOffset1  4
#define regOffset2  8
#define regOffset3	12
#define regOffset4	16
#define regOffset5	20
#define regOffset6	24

#define Reset_bit   0x01
#define DataIn_bit  0x02
#define RdyRece_bit 0x04
#define RdySend_bit 0x08
#define Done_bit    0x80





#ifdef XPAR_INTC_0_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_INTC_0_DEVICE_ID
#define INTC				XIntc
#define INTC_HANDLER		XIntc_InterruptHandler
#else
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define INTC				XScuGic
#define INTC_HANDLER		XScuGic_InterruptHandler
#endif /* XPAR_INTC_0_DEVICE_ID */



#define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_0_BASEADDR

#define PLATFORM_ZYNQMP 


#endif
