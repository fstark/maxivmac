#pragma once

#include "devices/device.h"
#include "core/machine_config.h"
#include <cstdint>

// VIA2 Device class wrapping the original VIA2 emulation
class VIA2Device : public Device {
public:
	// Device interface
	uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
	void zap() override;
	void reset() override;
	const char* name() const override { return "VIA2"; }

	// Access the VIA configuration (port masks, wire mapping)
	const VIAConfig& viaConfig() const;

	// Timer ICT callbacks
	void doTimer1Check();
	void doTimer2Check();

	// Extra time (pause/resume timers during extra emulation cycles)
	void extraTimeBegin();
	void extraTimeEnd();

	// Pulse notifications (interrupt sources)
	void iCA1_PulseNtfy();
	void iCA2_PulseNtfy();
	void iCB1_PulseNtfy();
	void iCB2_PulseNtfy();

	// Shift register
	void shiftInData(uint8_t v);
	uint8_t shiftOutData();

	// Timer invert time
	uint16_t getT1InvertTime();

	// Internal state - public for backward compatibility during migration
	struct VIA_Ty {
		uint32_t T1C_F;  /* Timer 1 Counter Fixed Point */
		uint32_t T2C_F;  /* Timer 2 Counter Fixed Point */
		uint8_t ORB;     /* Buffer B */
		uint8_t DDR_B;   /* Data Direction Register B */
		uint8_t DDR_A;   /* Data Direction Register A */
		uint8_t T1L_L;   /* Timer 1 Latch Low */
		uint8_t T1L_H;   /* Timer 1 Latch High */
		uint8_t T2L_L;   /* Timer 2 Latch Low */
		uint8_t SR;      /* Shift Register */
		uint8_t ACR;     /* Auxiliary Control Register */
		uint8_t PCR;     /* Peripheral Control Register */
		uint8_t IFR;     /* Interrupt Flag Register */
		uint8_t IER;     /* Interrupt Enable Register */
		uint8_t ORA;     /* Buffer A */
	};

	VIA_Ty d_{};

	uint8_t T1_Active = 0;
	uint8_t T2_Active = 0;
	bool T1IntReady = false;
	bool T1Running = true;
	uint32_t T1LastTime = 0;
	bool T2Running = true;
	bool T2C_ShortTime = false;
	uint32_t T2LastTime = 0;

private:
	uint8_t getORA(uint8_t selection);
	uint8_t getORB(uint8_t selection);
	void putORA(uint8_t selection, uint8_t data);
	void putORB(uint8_t selection, uint8_t data);
	void setDDR_A(uint8_t data);
	void setDDR_B(uint8_t data);
	void checkInterruptFlag();
	void setInterruptFlag(uint8_t viaInt);
	void clrInterruptFlag(uint8_t viaInt);
	void clear();
	void checkT1IntReady();
};

