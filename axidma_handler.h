/*
 * axidma_handler.h
 *
 *  Created on: Nov 30, 2022
 *      Author: skltmw05
 */
#ifndef __AXIDMA_HANDLER_H_
#define __AXIDMA_HANDLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "xaxidma.h"
#include "platform_config.h"

#if defined(XPAR_INTC_0_DEVICE_ID)
	#if defined(XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID) || defined(XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID)
		#define RX_INTR_ID		XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define TX_INTR_ID		XPAR_INTC_0_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define DMA_INTR	1
	#elif defined(XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID) || defined(XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID)
		#define RX_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define TX_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define DMA_INTR	1
	#endif
#else
	#if defined(XPAR_FABRIC_AXI_DMA_S2MM_INTROUT_INTR) || defined(XPAR_FABRIC_AXI_DMA_MM2S_INTROUT_INTR)
		#define RX_INTR_ID 		XPAR_FABRIC_AXI_DMA_S2MM_INTROUT_INTR
		#define TX_INTR_ID 		XPAR_FABRIC_AXI_DMA_MM2S_INTROUT_INTR
		#define DMA_INTR 	1
	#endif
#endif



int DMADeviceSetup(u16 DeviceId);

int RxSetup(XAxiDma * AxiDmaInstPtr);
int TxSetup(XAxiDma * AxiDmaInstPtr);

void TxIntrHandler(void *Callback);
void RxIntrHandler(void *Callback);

int SendPackets(XAxiDma * AxiDmaInstPtr);

int CheckData(void);
int CheckDmaResult(XAxiDma * AxiDmaInstPtr);


int SetupIntrSystem(INTC * IntcInstancePtr, XAxiDma * AxiDmaPtr,
		void *IntrHandler, u16 IntrId);
int SetupIntrSystemTXRX(INTC * IntcInstancePtr, XAxiDma * AxiDmaPtr,
		u16 TxIntrId, u16 RxIntrId);


void DisconnectIntrSystem(INTC * IntcInstancePtr, u16 IntrId);

void EnableIntrSystem(void);
void DisableIntrSystem(void);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AXIDMA_HANDLER_H_*/
