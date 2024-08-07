/*
 * myhal_spi_driver.c
 *
 *  Created on: May 20, 2024
 *      Author: ched
 */

#include "spi-driver.h"
/**
 * Initialize clocks for using SPI1 on PB3,4,5 abstract it later
 */
void init_clocks(void)
{
	RCC->AHB1ENR |= (1 << 0); // enable GPIOA

	MYHAL_RCC_SPI1_CLK_ENABLE(); // enable SPI1

}

void init_gpio(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/**
	 * PA5 - SPI2 Clock
	 * PA6 - SPI2 MISO
	 * PA7 - SPI2 MOSI
	 */
	GPIO_InitStruct.Pin = 5 | 6 | 7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// PA8 really just used as a GPIO output pin, but we can control it with the NSS bit if we
	// enable SSOE (SS output enable). NSS bit = 1, it's just writing 1 to output of PB12
	GPIO_InitStruct.Pin = 8;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
 * Configure master vs slave mode
 *
 * @param master_mode - 1 for master, 0 for slave
 */
static void myhal_spi_conf_master_mode(SPI_TypeDef *SPIx, uint32_t master_mode)
{
	if (master_mode)
	{
		SPIx->CR1 |= SPI_REG_CR1_MSTR;
		SPIx->CR1 |= SPI_REG_CR1_SSI;
	}
	else
	{
		SPIx->CR1 &= ~SPI_REG_CR1_MSTR;
	}
}

/**
 * Configure direction of SPI
 * Either Full (0) or Half Duplex (1)
 */
static void myhal_spi_conf_dir(SPI_TypeDef *SPIx, uint32_t dir)
{
	if (dir)
	{
		SPIx->CR1 |= SPI_REG_CR1_BIDIMODE;
	}
	else
	{
		SPIx->CR1 &= ~SPI_REG_CR1_BIDIMODE;
	}
}

/**
 * MSB first (0) or LSB first (1) (eeprom does MSB)
 */
static void myhal_spi_endianness(SPI_TypeDef *SPIx, uint8_t lsbfirst)
{
	if (lsbfirst)
	{
		SPIx->CR1 |= (1 << 7); // LSB first
	}
	else
	{
		SPIx->CR1 &= ~(1 << 7); // MSB First
	}
}

/**
 * Configure SPI datasize - 16 bit (1) or 8 bit (0)
 */
static void myhal_spi_conf_size(SPI_TypeDef *SPIx, uint32_t datasize_16)
{
	if (datasize_16)
	{
	SPIx->CR1 |= SPI_REG_CR1_DFF;
	}
	else
	{
		SPIx->CR1 &= ~SPI_REG_CR1_DFF;
	}
}

/**
 * Configures Baud Rate
 * @param baud - 3 bit baud rate
 *
 * Note it should probably be very low like 1MHz max since we're using wires just use 111 to be safe ig ?
 */
static void myhal_spi_baud(SPI_TypeDef *SPIx, uint8_t baud)
{
	baud = baud & (0b00000111); // ensure 3 bits
	SPIx->CR1 |= (baud << 3);
}

/**
 * Configure Polarity and Phase (think 0 0 for eeprom)
 */
static void myhal_spi_conf_phase_polarity(SPI_TypeDef *SPIx, uint32_t phase, uint32_t polarity)
{
	if (phase)
	{
		SPIx->CR1 |= SPI_REG_CR1_CPHA;
	}
	else
	{
		SPIx->CR1 &= ~SPI_REG_CR1_CPHA;
	}
	if (polarity)
	{
		SPIx->CR1 |= SPI_REG_CR1_CPOL;
	}
	else
	{
		SPIx->CR1 &= ~SPI_REG_CR1_CPOL;
	}
}

/**
 * Configure MCU for SSM or HSM. Use HW for now
 * If you want NSS you gotta do the SSOE stuff yourself
 */
static void myhal_spi_conf_ssm(SPI_TypeDef *SPIx, uint8_t ssm_enable)
{
	if (ssm_enable)
	{
		SPIx->CR1 |= SPI_REG_CR1_SSM;
	}
	else
	{
		SPIx->CR1 &= ~(SPI_REG_CR1_SSM);
//		SPIx->CR2 |= SPI_REG_CR2_SSOE; // enable NSS pin
	}
}

/**
 * Enable SPI device
 */
static void myhal_spi_enable(SPI_TypeDef *SPIx)
{
	if (!(SPIx->CR1 & SPI_REG_CR1_SPE))
	{
		SPIx->CR1 |= SPI_REG_CR1_SPE;
	}
}

/**
 * Disable SPI device
 */
static void myhal_spi_disable(SPI_TypeDef *SPIx)
{
	SPIx->CR1 &= ~SPI_REG_CR1_SPE;
}

/**
 * Initialize SPI device
 * @param	spi_handler - base address of SPI peripheral
 */
//void myhal_spi_init (spi_handler_t *spi_handler)
//{
//	myhal_spi_conf_master_mode(spi_handler->Instance, spi_handler->Init.Mode);
//	myhal_spi_conf_dir(spi_handler->Instance, spi_handler->Init.Direction);
//	myhal_spi_endianness(spi_handler->Instance, spi_handler->Init.FirstBit);
//	myhal_spi_conf_size(spi_handler->Instance, spi_handler->Init.DataSize);
//	myhal_spi_baud(spi_handler->Instance, spi_handler->Init.Prescaler);
//	myhal_spi_conf_phase_polarity(spi_handler->Instance, spi_handler->Init.CLKPhase, spi_handler->Init.CLKPolarity);
//	myhal_spi_conf_ssm(spi_handler->Instance, spi_handler->Init.NSS);
//	// also need to set SSOE not sure where should be done tho
//}

void myhal_spi_init (spi_handler_t *spi_handler)
{
	myhal_spi_conf_master_mode(spi_handler->Instance, spi_handler->Init.Mode);
	myhal_spi_conf_dir(spi_handler->Instance, spi_handler->Init.Direction);
	myhal_spi_endianness(spi_handler->Instance, spi_handler->Init.FirstBit);
	myhal_spi_conf_size(spi_handler->Instance, spi_handler->Init.DataSize);
	myhal_spi_baud(spi_handler->Instance, spi_handler->Init.Prescaler);
	myhal_spi_conf_phase_polarity(spi_handler->Instance, spi_handler->Init.CLKPhase, spi_handler->Init.CLKPolarity);
	myhal_spi_conf_ssm(spi_handler->Instance, spi_handler->Init.NSS);
	// also need to set SSOE not sure where should be done tho
}


uint8_t spi_tx(spi_handler_t *spi_handler, uint8_t tx_data)
{
	myhal_spi_enable(spi_handler->Instance);

	spi_handler->Instance->DR = tx_data;
	int c = 0;

	// wait until SPI not busy and TX empty
	while( ((spi_handler->Instance->SR)&(1u << 7)) && (!((spi_handler->Instance->SR)&(1u << 0))) ){
		c++;
	}

	uint8_t rx_data = (uint8_t)spi_handler->Instance->DR;

	return rx_data;
}

uint8_t spi_txrx(spi_handler_t* hspi, uint8_t* pTxData, uint8_t* pRxData)
{
	  /* Set the transaction information */
	  hspi->pRxBuffPtr  = (uint8_t *)pRxData;
//	  hspi->RxXferCount = Size;
//	  hspi->RxXferSize  = Size;
	  hspi->pTxBuffPtr  = (uint8_t *)pTxData;
//	  hspi->TxXferCount = Size;
//	  hspi->TxXferSize  = Size;

	  /* Transmit and Receive data in 8 Bit mode */
	  *((__IO uint8_t *)&hspi->Instance->DR) = (*hspi->pTxBuffPtr);
//	  hspi->pTxBuffPtr += sizeof(uint8_t);
//	  hspi->TxXferCount--;

	  /* Wait until RXNE flag is reset */
	  while (!(__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE)));
	  HAL_GPIO_WritePin(GPIOA, 5, 1);
	  HAL_Delay(5000);
		(*(uint8_t *)hspi->pRxBuffPtr) = hspi->Instance->DR;
//		hspi->pRxBuffPtr++;
//		hspi->RxXferCount--;

//	}
}

uint8_t spi_rx(spi_handler_t *spi_handler)
{
	myhal_spi_enable(spi_handler->Instance);

//	uint8_t rx_data = (uint8_t)spi_handler->Instance->DR;

	// wait until SPI not busy and RX buffer not empty
//	while( ((spi_handler->Instance->SR)&(1u << 7)) || (!((spi_handler->Instance->SR)&(1u << 0))) );
	while (!(__HAL_SPI_GET_FLAG(spi_handler, SPI_FLAG_RXNE)));
	/* read the received data */
	uint8_t rx_data = *(__IO uint8_t *)&spi_handler->Instance->DR;
	return rx_data;
}




/**
 * Enable TXE Interrupt
 */
static void myhal_spi_enable_txe_int(SPI_TypeDef *SPIx)
{
	SPIx->CR2 |= SPI_REG_CR2_TXEIE_ENABLE;
}

/**
 * Master TX
 *
 * @param	buf : poitner to TX buf
 * @param	len : length of TX data
 */
void myhal_spi_master_tx (spi_handler_t *spi_handler, uint8_t *tx_buf, uint32_t len)
{
	spi_handler->pTxBuffPtr = tx_buf;
	spi_handler->TxXferSz = len;
	spi_handler->TxXferCount = len; // decrements as data transmitted



	spi_handler->State = MYHAL_SPI_STATE_BUSY_TX;

	myhal_spi_enable(spi_handler->Instance);

	/* TXE interrupt triggered immediately after enabled b/c we've already TX'd all the data
	 * out of the DR */
	myhal_spi_enable_txe_int(spi_handler->Instance); // enables TX empty interrupt when TX done
}


/*
 * Enable RXNE Interrupt
 *
 */
static void myhal_spi_enable_rxne_int(SPI_TypeDef *SPIx)
{
	SPIx->CR2 |= SPI_REG_CR2_RXNEIE_ENABLE;
}

/**
 * DisableS TXE Interrupt: clears TXEIE bit
 */
static void myhal_spi_disable_txe_int(SPI_TypeDef *SPIx)
{
	SPIx->CR2 &= ~SPI_REG_CR2_TXEIE_ENABLE;
}

/**
 * Disables RXNE Interrupt: clears RXNEIE bit
 */
static void myhal_spi_disable_rxne_int(SPI_TypeDef *SPIx)
{
	SPIx->CR2 &= ~SPI_REG_CR2_RXNEIE_ENABLE;
}




/* ******************** HELPER FUNCTIONS ******************* */

/**
 * Enable SPI device AFTER configuration settings of control registers
 */





/**
 * Configure NSS pin through SW Slave Management when device in master mode
 * NSS not used when device in master mode, so this is obsolete..?
 * @param ssm - either 1 for SSM enabled, 0 for disabled
 */
static void myhal_spi_conf_nss_master(SPI_TypeDef *SPIx, uint32_t ssm_enable)
{
	if (ssm_enable)
	{
		SPIx->CR1 |= SPI_REG_CR1_SSM;
		/* Make SSI bit to 1 to hold NSS high. Since we are not planning on using this
		device in slave mode, this should stay high, so makes sense to pull high. */
	}
	else
	{
		SPIx->CR1 &= ~(SPI_REG_CR1_SSM);
	}
}

/**
 * Configure NSS pin through SW Slave Management when device in slave mode
 *
 * @param ssm - either 1 for SSM enabled, 0 for disabled
 */
static void myhal_spi_conf_nss_slave(SPI_TypeDef *SPIx, uint32_t ssm_enable)
{
	if (ssm_enable)
	{
		SPIx->CR1 |= SPI_REG_CR1_SSM;
	}
	else
	{
		SPIx->CR1 &= ~(SPI_REG_CR1_SSM);
	}
}




/**
 * Disables TXE interrupt, sets SPI ready
 */
static void myhal_spi_tx_close_int(spi_handler_t *hspi)
{
	myhal_spi_disable_txe_int(hspi->Instance);
	// If master mode and not busy, make ready
	if ((hspi->Init.Mode) && (hspi->State != MYHAL_SPI_STATE_BUSY))
	{
		hspi->State = MYHAL_SPI_STATE_READY;
	}
	/**
	 * Note that you don't want to manually set slave ready like this, only master.
	 * This is b/c slave needs clock from master, so it needs to be ready'd in RXNE closer..? 33-4
	 */
}


/**
 * Close RXNE interrupt, sets SPI to ready.
 * Called when RX Buffer is empty, no more data to be read
 */
static void myhal_spi_rx_close_int(spi_handler_t *hspi)
{
	// idk what this line does, maybe waits until no longer busy
	while (hspi->State == MYHAL_SPI_STATE_BUSY);
	myhal_spi_disable_rxne_int(hspi->Instance);

	hspi->State = MYHAL_SPI_STATE_READY;
}






/**
 * Master RX
 *
 */
void myhal_spi_master_rx (spi_handler_t *spi_handler, uint8_t *rx_buf, uint32_t len)
{
	uint16_t val;
	/* master first has to TX dummy values to slave to produce clock before actual RX */
	spi_handler->pTxBuffPtr = rx_buf;
	spi_handler->TxXferCount = len;
	spi_handler->TxXferSz = len;

	/* actually begin the RXing */
	spi_handler->pRxBuffPtr = rx_buf;
	spi_handler->RxXferCount = len;
	spi_handler->RxXferSz = len;

	spi_handler->State = MYHAL_SPI_STATE_BUSY_RX;

	myhal_spi_enable(spi_handler->Instance);

	/* Read out/Empty the DR reg BEFORE enabling RXNE interrupt
	 * Gotta do this b/c reading out DR clears RXNE, effectively "emptying"
	 * the RX buf */
	val = spi_handler->Instance->DR;

	myhal_spi_enable_txe_int(spi_handler->Instance);
	myhal_spi_enable_rxne_int(spi_handler->Instance);

}

/**
 * Slave TX
 */
void myhal_spi_slave_tx (spi_handler_t *spi_handler, uint8_t *tx_buf, uint32_t len)
{

	// Slave needs Master to TX, so enable slave RX
	// initialize TX
	spi_handler->pTxBuffPtr = tx_buf;
	spi_handler->TxXferCount = len;
	spi_handler->TxXferSz = len;

	/* Dummy RX */
	spi_handler->pRxBuffPtr = tx_buf;
	spi_handler->RxXferCount = len;
	spi_handler->RxXferSz = len;

	spi_handler->State = MYHAL_SPI_STATE_BUSY_TX;

	myhal_spi_enable(spi_handler->Instance);

	/* both need to be enabled b/c whenever we do slave rx or tx, there's always
	   some dummy transmission going on */
	myhal_spi_enable_rxne_int(spi_handler->Instance);
	myhal_spi_enable_txe_int(spi_handler->Instance);
}

/**
 * Slave RX
 *
 * @param rcv_buf : pointer to RX buf
 *
 */
void myhal_spi_slave_rx (spi_handler_t *spi_handler, uint8_t *rx_buf, uint32_t len)
{
	/* no dummy TX b/c slave receives first, slave TX is unnecessary */

	spi_handler->pRxBuffPtr = rx_buf;
	spi_handler->RxXferCount = len;
	spi_handler->RxXferSz = len;

	spi_handler->State = MYHAL_SPI_STATE_BUSY_RX;

	myhal_spi_enable(spi_handler->Instance);

	/* Slave RXNE interrupt goes when slave is full, don't need TXE interrupt b/c
	 * slave isn't TXing here */
	myhal_spi_enable_rxne_int(spi_handler->Instance);
	//myhal_spi_enable_txe_int(spi_handler->Instance);
}

/**
 * SPI IRQ Handler
 * @param	hspi : pointer to spi handler_t struc which contains config info for SPI
 */
void myhal_spi_irq_handler(spi_handler_t *hspi)
{
	uint32_t tmp1 = 0, tmp2 = 0;

	/* Check RX interrupt event */
	// check RXNE flag set in SR
	tmp1 = (hspi->Instance->SR & SPI_REG_SR_RXNE_FLAG);
	// check RXNEIE enabled in CR2
	tmp2 = (hspi->Instance->CR2 & SPI_REG_CR2_RXNEIE_ENABLE);
	if ( (tmp1 != RESET) && (tmp2 != RESET) )
	{
		// RXNE flag is set, handle RX data
		myhal_spi_handle_rx_int(hspi);
		return;
	}

	/* Check TX interrupt event */
	// check TXE flag set in SR
	tmp1 = (hspi->Instance->SR & SPI_REG_SR_TXE_FLAG);
	// check TXEIE enabled in CR2
	tmp2 = (hspi->Instance->CR2 & SPI_REG_CR2_TXEIE_ENABLE);
	if ( (tmp1 != RESET) && (tmp2 != RESET) )
	{
		// TXE flag set, handle TX data
		myhal_spi_handle_tx_int(hspi);
		return;
	}

}

/**
 * Handles TXE interrupt, only gets called when TXEIE = 1, TXE = 1
 * Calls from within master or slave TX, so TX buffer address already in pTXBuffPtr
 */
void myhal_spi_handle_tx_int(spi_handler_t *hspi)
{
	// TX in 8-bit mode
	if (hspi->Init.DataSize == SPI_8BIT_DF_ENABLE)
	{
		// read TX Buf and increment forward in it to next byte
		hspi->Instance->DR = (*hspi->pTxBuffPtr++);
		// decrement count by 1 byte
		hspi->TxXferCount--;
	// TX in 16-bit mode
	}
	else
	{
		hspi->Instance->DR = *( (uint16_t*)hspi->pTxBuffPtr );
		hspi->pTxBuffPtr+=2;
		hspi->TxXferCount-=2;
	}
	// No more data to TX
	if (hspi->TxXferCount == 0)
	{
		myhal_spi_tx_close_int(hspi);
	}
}

/**
 * Handles RXNE interrupt, only gets called when RXNEIE=1 AND RXNE=1
 * RXNE means there's stuff to be read in RX_buf, we have RXBuf in pRXBuffPtr, so just access
 * that way.
 */
void myhal_spi_handle_rx_int(spi_handler_t *hspi)
{
	uint8_t val;
	if (hspi->Init.DataSize == SPI_8BIT_DF_ENABLE)
	{
		// read DR value, put into
		(*hspi->pRxBuffPtr++) = hspi->Instance->DR;

		hspi->RxXferCount--;
	}
	else
	{
		*(hspi->pRxBuffPtr) = hspi->Instance->DR;
		hspi->pRxBuffPtr+=2;
		hspi->RxXferCount-=2;
	}
	// no more data to read
	if (hspi->RxXferCount == 0)
	{
		myhal_spi_rx_close_int(hspi);
	}
}

