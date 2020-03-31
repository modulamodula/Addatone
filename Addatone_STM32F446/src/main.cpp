#include "initialisation.h"
#include "bitstream.h"


volatile uint32_t SysTickVal;

extern "C" {
#include "interrupts.h"
}

void programFPGA() {
	// SPI clock should be between 1 and 25MHz with minimum clock low/high time of 20ns)

	// Pull reset low (PB8)
	GPIOB->BSRR |= GPIO_BSRR_BR_8;

	// Drive SPI_SS low (PB12)
	GPIOB->BSRR |= GPIO_BSRR_BR_12;

	// Hold the CRESET_B pin Low for at least 200 ns
	// Clock of 144MHz gives tick of 6.94ns so wait 400/6.94 = 57 ticks (measured at 7500ns)
	for (int i = 0; i < 60; ++i);
	GPIOB->BSRR |= GPIO_BSRR_BS_8;

	// After driving CRESET_B High the AP must wait a minimum of 1200 µs, to let the FPGA to clear its internal configuration memory
	// SysTick is set to around 400us so allow four for safety (measured at 1453us)
	int tempTime = SysTickVal;
	while (SysTickVal - tempTime < 4);

	// Drive SPI_SS high, wait 8 clock cycles then back to Low
	GPIOB->BSRR |= GPIO_BSRR_BS_12;
	SPI2->DR = 0;
	//SPI2->CR1 &= ~SPI_CR1_CPOL;						// Switch the clock polarity back to idle low to drive clock pin low
	while (((SPI2->SR & SPI_SR_TXE) == 0) | ((SPI2->SR & SPI_SR_BSY) == SPI_SR_BSY) );
	GPIOB->BSRR |= GPIO_BSRR_BR_12;

	// Send thge bitstream MSB first, data clocked into FPGA on rising edge
	for (int b = 0; b < bitstreamSize; ++b) {
	//for (int b = 0; b < 30; ++b) {
		while (((SPI2->SR & SPI_SR_TXE) == 0) | ((SPI2->SR & SPI_SR_BSY) == SPI_SR_BSY) );
		SPI2->DR = bitstream[b];
	}

	// Drive SPI_SS High, wait 100 SPI_SCK to allow CDONE to go high.
	while (((SPI2->SR & SPI_SR_TXE) == 0) | ((SPI2->SR & SPI_SR_BSY) == SPI_SR_BSY) );
	GPIOB->BSRR |= GPIO_BSRR_BS_12;
	for (int b = 0; b < 13; ++b) {
		while (((SPI2->SR & SPI_SR_TXE) == 0) | ((SPI2->SR & SPI_SR_BSY) == SPI_SR_BSY) );
		SPI2->DR = 0;
	}

	/*
	After sending the entire image, the iCE40 FPGA releases the CDONE output allowing it to float High via the external
	pull-up resistor to AP_VCC. If the CDONE pin remains Low, then an error occurred during configuration and the AP
	should handle the error accordingly for the application.

	After the CDONE output pin goes High, send at least 49 additional dummy bits, effectively 49 additional SPI_SCK clock
	cycles measured from rising-edge to rising-edge.
	*/
	for (int b = 0; b < 7; ++b) {
		while (((SPI2->SR & SPI_SR_TXE) == 0) | ((SPI2->SR & SPI_SR_BSY) == SPI_SR_BSY) );
		SPI2->DR = 0;
	}
	//SPI2->CR1 |= SPI_CR1_CPOL;						// Reset the clock polarity to idle high

}

extern uint32_t SystemCoreClock;
int main(void)
{
	SystemInit();							// Activates floating point coprocessor and resets clock
	SystemClock_Config();					// Configure the clock and PLL
	SystemCoreClockUpdate();				// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL

	InitSysTick();
	InitFPGAProg();
	programFPGA();

	while (1)
	{
	}
}

