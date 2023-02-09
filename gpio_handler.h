/*
 * gpio_handler.h
 *
 *  Created on: Nov 30, 2022
 *      Author: skltmw05
 */

#ifndef __GPIO_HANDLER_H_
#define __GPIO_HANDLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_config.h"
#include "xgpio.h"

#ifdef XPAR_INTC_0_DEVICE_ID
#include "xintc.h"
#include <stdio.h>
#else
#include "xscugic.h"
#include "xil_printf.h"
#endif


#define GPIO_DEVICE_ID		XPAR_GPIO_0_DEVICE_ID
#define GPIO_CHANNEL1		1
#define BUTTON_CHANNEL	 	1
#define BUTTON_INTERRUPT 	XGPIO_IR_CH1_MASK
/*GPIO Interupt Device*/
#ifdef XPAR_INTC_0_DEVICE_ID

#define INTC_GPIO_INTERRUPT_ID	XPAR_GPIO_0_DEVICE_ID
#define INTC_DEVICE_ID			XPAR_INTC_0_DEVICE_ID // defined in platform_zynq.c
#else
#define INTC_DEVICE_ID			XPAR_SCUGIC_SINGLE_DEVICE_ID // defined in platform_zynq.c
#define INTC_GPIO_INTERRUPT_ID	XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#endif /* XPAR_INTC_0_DEVICE_ID */


void GpioHandler(void *CallBackRef);

int GpioSetupIntrSystem(INTC *IntcInstancePtr, XGpio *InstancePtr, u16 DeviceId,
		u16 IntrId, u16 IntrMask);

void GpioDisableIntr(INTC *IntcInstancePtr, XGpio *InstancePtr, u16 IntrId,
		u16 IntrMask);

int GpioIntrInit(INTC *IntcInstancePtr, XGpio* InstancePtr, u16 DeviceId,
		u16 IntrId, u16 IntrMask);

#ifdef __cplusplus
}
#endif
#endif /* SRC_GPIO_HANDLER_H_*/
