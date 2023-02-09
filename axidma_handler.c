#include "axidma_handler.h"
#include "xparameters.h"
#include "platform_config.h"
#include "xaxidma.h"
#include "xdebug.h"




extern XAxiDma AxiDma; /* Instance of the XAxiDma */
extern INTC Intc; /* Instance of the Interrupt Controller */

/*
 * Flags interrupt handlers use to notify the application context the events.
 */
extern volatile int TxDone;
extern volatile int RxDone;
extern volatile int Error;


#if defined(XPAR_UARTNS550_0_BASEADDR)
#include "xuartns550_l.h"       /* to use uartns550 */
/*****************************************************************************/
/*
 *
 * Uart16550 setup routine, need to set baudrate to 9600, and data bits to 8
 *
 * @param	None.
 *
 * @return	None
 *
 * @note		None.
 *
 ******************************************************************************/
static void Uart550_Setup(void)
{

	/* Set the baudrate to be predictable
	 */
	XUartNs550_SetBaud(XPAR_UARTNS550_0_BASEADDR,
			XPAR_XUARTNS550_CLOCK_HZ, 9600);

	XUartNs550_SetLineControlReg(XPAR_UARTNS550_0_BASEADDR,
			XUN_LCR_8_DATA_BITS);

}
#endif


/*****************************************************************************/
/*
 *
 * This function checks data buffer after the DMA transfer is finished.
 *
 * We use the static tx/rx buffers.
 *
 *
 * @return
 *		- XST_SUCCESS if validation is successful
 *		- XST_FAILURE if validation is failure.
 *
 * @note		None.
 *
 ******************************************************************************/
int CheckData(void) {
	u8 *RxPacket;
	int Index = 0;
	u8 Value;

	RxPacket = (u8 *) RX_BUFFER_BASE;
	Value = TEST_START_VALUE;

	/* Invalidate the DestBuffer before receiving the data, in case the
	 * Data Cache is enabled
	 */
	Xil_DCacheInvalidateRange((UINTPTR) RxPacket, MAX_PKT_LEN);

	for (Index = 0; Index < MAX_PKT_LEN; Index++) {
		if (RxPacket[Index] != Value) {
//			xil_printf("Data error %d: %x/%x\r\n", Index,
//					(unsigned int) RxPacket[Index], (unsigned int) Value);

			return XST_FAILURE;
		}
		Value = (Value + 1) & 0xFF;
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
/*
 *
 * This is the DMA TX Interrupt handler function.
 *
 * It gets the interrupt status from the hardware, acknowledges it, and if any
 * error happens, it resets the hardware. Otherwise, if a completion interrupt
 * is present, then sets the TxDone.flag
 *
 * @param	Callback is a pointer to TX channel of the DMA engine.
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
#ifdef TxDone
void TxIntrHandler(void *Callback) {

	u32 IrqStatus;
	int TimeOut;
	XAxiDma *AxiDmaInst = (XAxiDma *) Callback;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DMA_TO_DEVICE);

	/* Acknowledge pending interrupts */

	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DMA_TO_DEVICE);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {

		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		Error = 1;

		/*
		 * Reset should never fail for transmit channel
		 */
		XAxiDma_Reset(AxiDmaInst);

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if (XAxiDma_ResetIsDone(AxiDmaInst)) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If Completion interrupt is asserted, then set the TxDone flag
	 */
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

		TxDone = 1;
	}
}
#endif

/*****************************************************************************/
/*
 *
 * This is the DMA RX interrupt handler function
 *
 * It gets the interrupt status from the hardware, acknowledges it, and if any
 * error happens, it resets the hardware. Otherwise, if a completion interrupt
 * is present, then it sets the RxDone flag.
 *
 * @param	Callback is a pointer to RX channel of the DMA engine.
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
#if defined(RX_INTR_ID)
void RxIntrHandler(void *Callback) {

	u32 IrqStatus;
	int TimeOut;
	XAxiDma *AxiDmaInst = (XAxiDma *) Callback;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA);

	/* Acknowledge pending interrupts */
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		Error = 1;

		/* Reset could fail and hang
		 * NEED a way to handle this or do not call it??
		 */
		XAxiDma_Reset(AxiDmaInst);

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if (XAxiDma_ResetIsDone(AxiDmaInst)) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If completion interrupt is asserted, then set RxDone flag
	 */
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

		RxDone = 1;
	}
}
#endif

/*****************************************************************************/
/*
 *
 * This is the DMA Setup Function
 *
 * It gets the DMA device ID from the hardware, looking through the configuration table from XSA.
 *
 * @param	DMA Device ID (Register Address of the DMA)
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
int DMADeviceSetup(u16 DeviceId) {
	XAxiDma_Config *CfgPtr;
	int Status;

	CfgPtr = XAxiDma_LookupConfig(DeviceId);
	if (!CfgPtr) {
//		xil_printf("No config found for %d\r\n", DeviceId);
		return XST_FAILURE;
	}

	Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
	if (Status != XST_SUCCESS) {
//		xil_printf("Initialization failed %d\r\n", Status);
		return Status;
	}

	if (XAxiDma_HasSg(&AxiDma)) {
//		xil_printf("Device configured as SG mode \r\n");
		return XST_FAILURE;
	}
	/* Disable interrupts, we use polling mode
	 */

	return XST_SUCCESS;
}


/*****************************************************************************/
/*
 *
 * This function disable all Interrupt System.
 * Disabling data transfer from both side, including device to DMA, and DMA to device
 *
 *
 * @param	None
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void DisableIntrSystem(void) {
	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
}
/*****************************************************************************/
/*
 *
 * This function Enable all Interrupt System.
 * Enabling data transfer from both side, including device to DMA, and DMA to device
 *
 *
 * @param	None
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void EnableIntrSystem(void) {
	XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
}

/*****************************************************************************/
/*
 *
 * This function setups the interrupt system so interrupts can occur for the
 * DMA, it assumes INTC component exists in the hardware system.
 *
 * @param	IntcInstancePtr is a pointer to the instance of the INTC.
 * @param	AxiDmaPtr is a pointer to the instance of the DMA engine
 * @param	IntrId is the channel Interrupt ID.
 *
 * @return
 *		- XST_SUCCESS if successful,
 *		- XST_FAILURE.if not successful
 *
 * @note		None.
 *
 ******************************************************************************/
int SetupIntrSystem(INTC * IntcInstancePtr, XAxiDma * AxiDmaPtr,
		void *IntrHandler,
		u16 IntrId) {
	int Status;

#ifdef XPAR_INTC_0_DEVICE_ID

	/* Initialize the interrupt controller and connect the ISRs */
	Status = XIntc_Initialize(IntcInstancePtr, INTC_DEVICE_ID);
	if (Status != XST_SUCCESS) {

//		xil_printf("Failed init intc\r\n");
		return XST_FAILURE;
	}

	Status = XIntc_Connect(IntcInstancePtr, IntrId,
			(XInterruptHandler) IntrHandler, AxiDmaPtr);
	if (Status != XST_SUCCESS) {

//		xil_printf("Failed to connect intc\r\n");
		return XST_FAILURE;
	}

	/* Start the interrupt controller */
	Status = XIntc_Start(IntcInstancePtr, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {

//		xil_printf("Failed to start intc\r\n");
		return XST_FAILURE;
	}

	XIntc_Enable(IntcInstancePtr, IntrId);


#else

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

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, IntrId, 0xA0, 0x3);

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, IntrId,
			(Xil_InterruptHandler) IntrHandler, AxiDmaPtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	XScuGic_Enable(IntcInstancePtr, IntrId);


#endif

	/* Enable interrupts from the hardware */

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) INTC_HANDLER, (void *) IntcInstancePtr);

	Xil_ExceptionEnable();

	return XST_SUCCESS;
}



/*****************************************************************************/
/**
 *
 * This function disables the interrupts for DMA engine.
 *
 * @param	IntcInstancePtr is the pointer to the INTC component instance
 * @param	IntrId is interrupt ID associated w/ DMA respect channel
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void DisconnectIntrSystem(INTC * IntcInstancePtr, u16 IntrId) {
#ifdef XPAR_INTC_0_DEVICE_ID
	/* Disconnect the interrupts for the DMA TX and RX channels */
	XIntc_Disconnect(IntcInstancePtr, IntrId);
#else
	XScuGic_Disconnect(IntcInstancePtr, IntrId);
#endif
}



