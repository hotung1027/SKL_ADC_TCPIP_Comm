/******************************************************************************
 * Copyright (C) 2002 - 2021 Xilinx, Inc.  All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 *
 * This file contains a design example using the GPIO driver (XGpio) in an
 * interrupt driven mode of operation. This example does assume that there is
 * an interrupt controller in the hardware system and the GPIO device is
 * connected to the interrupt controller.
 *
 * This file is used in the Peripheral Tests Application in SDK to include a
 * simplified test for gpio interrupts.
 * The buttons and LEDs are on 2 separate channels of the GPIO so that interrupts
 * are not caused when the LEDs are turned on and off.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date	 Changes
 * ----- ---- -------- -------------------------------------------------------
 * 2.01a sn   05/09/06 Modified to be used by TestAppGen to include test for
 *		      interrupts.
 * 3.00a ktn  11/21/09 Updated to use HAL Processor APIs and minor changes
 *		      as per coding guidelines.
 * 3.00a sdm  02/16/11 Updated to support ARM Generic Interrupt Controller
 * 4.1   lks  11/18/15 Updated to use canonical xparameters and
 *		      clean up of the comments and code for CR 900381
 * 4.3   ms   01/23/17 Modified xil_printf statement in main function to
 *                     ensure that "Successfully ran" and "Failed" strings
 *                     are available in all examples. This is a fix for
 *                     CR-965028.
 *
 *</pre>
 *
 */


#include "gpio_handler.h"
#include "axidma_handler.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xil_exception.h"
#include "platform_config.h"

/*AXI DMA Handler */
extern XAxiDma AxiDma;
extern INTC Intc; /* The Instance of the Interrupt Controller Driver */
extern struct tcp_pcb* pass;
extern ADC_SIZE;
extern XGpio Gpio; /* The Instance of the GPIO Driver */

int nPulse_Flag, regVal, ADC_ACQUIRED;
int loop_lock = 0;


static u16 GlobalIntrMask; /* GPIO channel mask that is needed by
 * the Interrupt Handler */
static volatile u32 IntrFlag; /* Interrupt Handler Flag */
/* ---------	------	  --------		--------------		----------------------
 * |ADC VCC| -> |GPIO| -> |ON/OFF| ->   |Failing Edge| ->	|DMA RxBuffer to DRAM|
 * ---------	------	  --------		--------------		----------------------
 */
void GpioHandler(void *CallbackRef) {

	if (!(Xil_In32(XPAR_PL_SPI_ADC_MASTERSTR_0_BASEADDR + regOffset5) == 0x00)
			&& !(XGpio_DiscreteRead(&Gpio, GPIO_CHANNEL1) & 0x01)) {

		int Status;
		u16 *RxBufferPtr;
		RxBufferPtr = (u16 *) RX_BUFFER_BASE;

		// allocate buffer
		memset((UINTPTR) RxBufferPtr, NULL, MAX_PKT_LEN
		);

		Xil_DCacheFlushRange((UINTPTR) RxBufferPtr,
		MAX_PKT_LEN);

//		Xil_DCacheFlushRange((UINTPTR) RxBufferPtr,
//				(2 * ADC_SIZE * 16U / 32U) * 32U);



		// Cache invalidate right after buffer allocation
		Xil_DCacheInvalidateRange((UINTPTR) RxBufferPtr,
		MAX_PKT_LEN
		);


//		Xil_DCacheInvalidateRange((UINTPTR) RxBufferPtr,
//				(2 * ADC_SIZE * 16U / 32U) * 32U);

		// Read Data from PL to DDR Memory
		Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR) RxBufferPtr,
				(ADC_SIZE * 16U / 32U + 1) * 32U,
				XAXIDMA_DEVICE_TO_DMA);



//		Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR) RxBufferPtr,
//		MAX_PKT_LEN,
//		XAXIDMA_DEVICE_TO_DMA);

		if (Status != XST_SUCCESS) {
			xil_printf("Failed submit DMA receiving request!");
			return;
		}

		while ((XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA))) {
			/* Wait */

			asm("NOP");
		}
		
//		Xil_DCacheFlushRange((UINTPTR) RxBufferPtr,
//				(DEFAULT_ADC_LENGTH * 16U / 32U + 1) * 32U);
		Xil_DCacheInvalidateRange((UINTPTR) RxBufferPtr,
		MAX_PKT_LEN);

		// release adc read request flag
		Xil_Out32(XPAR_PL_SPI_ADC_MASTERSTR_0_BASEADDR + regOffset5, 0x00);

		ADC_ACQUIRED = 1;
	}


	XGpio *GpioPtr = (XGpio *) CallbackRef;
	XGpio_InterruptClear(GpioPtr, GlobalIntrMask);
}

int GpioSetupIntrSystem(INTC *IntcInstancePtr, XGpio *InstancePtr, u16 DeviceId,
		u16 IntrId, u16 IntrMask) {
	int Status;

	GlobalIntrMask = IntrMask;
	XScuGic_Config *IntcConfig;

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}
	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
			IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, IntrId, 0x20, 0x02);

	Status = XScuGic_SelfTest(IntcInstancePtr);
	if (Status != XST_SUCCESS) {
#ifdef SCUGIC_DEBUG
		xil_printf("In %s: ScuGic Self test failed...\r\n", __func__);
#endif
		return XST_FAILURE;
	}

	/*
	 * Connect the interrupt handler that will be called when an
	 * interrupt occurs for the device.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, IntrId,
			(Xil_ExceptionHandler) GpioHandler, InstancePtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	/* Enable the interrupt for the GPIO device.*/
	XScuGic_Enable(IntcInstancePtr, IntrId);

	/*
	 * Enable the GPIO channel interrupts so that push button can be
	 * detected and enable interrupts for the GPIO device
	 */
	XGpio_InterruptEnable(InstancePtr, IntrMask);
	XGpio_InterruptGlobalEnable(InstancePtr);

	/*
	 * Initialize the exception table and register the interrupt
	 * controller handler with the exception table
	 */
	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) INTC_HANDLER, IntcInstancePtr);

	/* Enable non-critical exceptions */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

int GpioIntrInit(INTC *IntcInstancePtr, XGpio* InstancePtr, u16 DeviceId,
		u16 IntrId, u16 IntrMask) {
	int Status;


	/* Initialize the GPIO driver. If an error occurs then exit */
	Status = XGpio_Initialize(InstancePtr, DeviceId);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = GpioSetupIntrSystem(IntcInstancePtr, InstancePtr, DeviceId, IntrId,
			IntrMask);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
}








