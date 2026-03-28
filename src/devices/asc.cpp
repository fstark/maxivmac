/*
	Apple Sound Chip EMulated DEVice
*/

#include "core/common.h"



#include "devices/via.h"
#include "devices/via2.h"
#include "core/machine_obj.h"

#include "devices/asc.h"
#include "core/abnormal_ids.h"

/* Global singleton */

/*
	ReportAbnormalID unused 0x0F0E, 0x0F1E - 0x0FFF
*/

static uint8_t SoundReg801 = 0;
static uint8_t SoundReg802 = 0;
static uint8_t SoundReg803 = 0;
static uint8_t SoundReg804 = 0;
static uint8_t SoundReg805 = 0;
static uint8_t SoundReg_Volume = 0; /* 0x806 */
/* static uint8_t SoundReg807 = 0; */

static uint8_t ASC_SampBuff[0x800];

struct ASC_ChanR {
	uint8_t freq[4];
	uint8_t phase[4];
};

static ASC_ChanR ASC_ChanA[4];

static uint16_t ASC_FIFO_Out = 0;
static uint16_t ASC_FIFO_InA = 0;
static uint16_t ASC_FIFO_InB = 0;
static bool ASC_Playing = false;

#define ASC_dolog (dbglog_HAVE && 0)

static void ASC_RecalcStatus()
{
	if ((1 == SoundReg801) && ASC_Playing) {
		if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x200) {
			SoundReg804 &= ~ 0x01;
		} else {
			SoundReg804 |= 0x01;
		}
		if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x400) {
			SoundReg804 |= 0x02;
		} else {
			SoundReg804 &= ~ 0x02;
		}
		if (0 != (SoundReg802 & 2)) {
			if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x200) {
				SoundReg804 &= ~ 0x04;
			} else {
				SoundReg804 |= 0x04;
			}
			if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x400) {
				SoundReg804 |= 0x08;
			} else {
				SoundReg804 &= ~ 0x08;
			}
		}
	}
}

static void ASC_ClearFIFO()
{
	ASC_FIFO_Out = 0;
	ASC_FIFO_InA = 0;
	ASC_FIFO_InB = 0;
	ASC_Playing = false;
	ASC_RecalcStatus();
}

/*
	ASC register read/write.  Handles the 2K FIFO sample
	buffers, control/mode/volume registers, and wavetable
	frequency/phase parameters.
*/
 uint32_t ASCDevice::access(uint32_t Data, bool WriteMem, uint32_t addr)
{
	if (addr < 0x800) {
		if (WriteMem) {
			if (1 == SoundReg801) {
				if (0 == (addr & 0x400)) {
					if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x400)
					{
						SoundReg804 |= 0x02;
					} else {

					ASC_SampBuff[ASC_FIFO_InA & 0x3FF] = Data;

					++ASC_FIFO_InA;
					if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x200)
					{
						if (0 != (SoundReg804 & 0x01)) {
							/* happens normally */
							SoundReg804 &= ~ 0x01;
						}
					} else {
					}
					if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x400)
					{
						SoundReg804 |= 0x02;
#if ASC_dolog
						dbglog_WriteNote("ASC : setting full flag A");
#endif
					} else {
						if (0 != (SoundReg804 & 0x02)) {
							ReportAbnormalID(AbnormalID::kASC_full_flag_A_not_already_clear, "ASC_Access : "
								"full flag A not already clear");
							SoundReg804 &= ~ 0x02;
						}
					}

					}
				} else {
					if (0 == (SoundReg802 & 2)) {
						ReportAbnormalID(AbnormalID::kASC_Channel_B_for_Mono,
							"ASC - Channel B for Mono");
					}
					if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x400)
					{
						ReportAbnormalID(AbnormalID::kASC_Channel_B_Overflow,
							"ASC - Channel B Overflow");
						SoundReg804 |= 0x08;
					} else {

					ASC_SampBuff[0x400 + (ASC_FIFO_InB & 0x3FF)] = Data;

					++ASC_FIFO_InB;
					if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x200)
					{
						if (0 != (SoundReg804 & 0x04)) {
							/* happens normally */
							SoundReg804 &= ~ 0x04;
						}
					} else {
					}
					if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x400)
					{
						SoundReg804 |= 0x08;
#if ASC_dolog
						dbglog_WriteNote("ASC : setting full flag B");
#endif
					} else {
						if (0 != (SoundReg804 & 0x08)) {
							ReportAbnormalID(AbnormalID::kASC_full_flag_B_not_already_clear, "ASC_Access : "
								"full flag B not already clear");
							SoundReg804 &= ~ 0x08;
						}
					}

					}
				}
#if ASC_dolog && 0
				dbglog_writeCStr("ASC_InputIndex =");
				dbglog_writeNum(ASC_InputIndex);
				dbglog_writeReturn();
#endif
			} else {
				ASC_SampBuff[addr] = Data;
			}
		} else {
			Data = ASC_SampBuff[addr];
		}

#if ASC_dolog && 1
		{
			dbglog_AddrAccess("ASC_Access SampBuff",
				Data, WriteMem, addr);
		}
#endif
	} else if (addr < 0x810) {
		switch (addr) {
			case 0x800: /* VERSION */
				if (WriteMem) {
					ReportAbnormalID(AbnormalID::kASC_writing_VERSION, "ASC - writing VERSION");
				} else {
					Data = 0;
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (VERSION)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x801: /* ENABLE */
				if (WriteMem) {
					if (1 == Data) {
						if (1 != SoundReg801) {
							ASC_ClearFIFO();
						}
					} else {
						if (Data > 2) {
							ReportAbnormalID(AbnormalID::kASC_unexpected_ENABLE,
								"ASC - unexpected ENABLE");
						}
					}
					SoundReg801 = Data;
				} else {
					Data = SoundReg801;
					/* happens in LodeRunner */
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (ENABLE)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x802: /* CONTROL */
				if (WriteMem) {
#if 1
					if (0 != SoundReg801) {
						if (SoundReg802 == Data) {
							/*
								this happens normally,
								such as in Lunar Phantom
							*/
						} else {
							if (1 == SoundReg801) {
/*
	happens in dark castle, if play other sound first,
	such as by changing beep sound in sound control panel.
*/
								ASC_ClearFIFO();
							}

						}
					}
#endif
					if (0 != (Data & ~ 2)) {
						ReportAbnormalID(AbnormalID::kASC_unexpected_CONTROL_value,
							"ASC - unexpected CONTROL value");
					}
					SoundReg802 = Data;
				} else {
					Data = SoundReg802;
					ReportAbnormalID(AbnormalID::kASC_reading_CONTROL_value,
						"ASC - reading CONTROL value");
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (CONTROL)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x803:
				if (WriteMem) {
					if (0 != (Data & ~ 0x80)) {
						ReportAbnormalID(AbnormalID::kASC_unexpected_FIFO_MODE,
							"ASC - unexpected FIFO MODE");
					}
					if (0 != (Data & 0x80)) {
						if (0 != (SoundReg803 & 0x80)) {
							ReportAbnormalID(AbnormalID::kASC_set_clear_FIFO_again,
								"ASC - set clear FIFO again");
						} else
						if (1 != SoundReg801) {
						} else
						{
							ASC_ClearFIFO();
							/*
								ASC_interrupt_PulseNtfy();
									Doesn't seem to be needed,
									but doesn't hurt either.
							*/
						}
					}
					SoundReg803 = Data;
				} else {
					Data = SoundReg803;
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (FIFO MODE)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x804:
				if (WriteMem) {
					SoundReg804 = Data;
					if (0 != SoundReg804) {
					if (auto* via2 = machine_->findDevice<VIA2Device>())
						via2->iCB1_PulseNtfy();
						/*
							Generating this interrupt seems
							to be the point of writing to
							this register.
						*/
					}
#if ASC_dolog && 1
					dbglog_AddrAccess(
						"ASC_Access Control (FIFO IRQ STATUS)",
						Data, WriteMem, addr);
#endif
				} else {
					Data = SoundReg804;
					/* SoundReg804 = 0; */
					SoundReg804 &= ~ 0x01;
					SoundReg804 &= ~ 0x04;
						/*
							In lunar phantom, observe checking
							full flag before first write, but
							status was read previous.
						*/
#if ASC_dolog && 1
					{
						dbglog_AddrAccess(
							"ASC_Access Control (FIFO IRQ STATUS)",
							Data, WriteMem, addr);
					}
#endif
				}
				break;
			case 0x805:
				if (WriteMem) {
					SoundReg805 = Data;
					/* cleared in LodeRunner */
				} else {
					Data = SoundReg805;
					ReportAbnormalID(AbnormalID::kASC_reading_WAVE_CONTROL_register,
						"ASC - reading WAVE CONTROL register");
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (WAVE CONTROL)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x806: /* VOLUME */
				if (WriteMem) {
					SoundReg_Volume = Data >> 5;
					if (0 != (Data & 0x1F)) {
						ReportAbnormalID(AbnormalID::kASC_unexpected_volume_value,
							"ASC - unexpected volume value");
					}
				} else {
					Data = SoundReg_Volume << 5;
					ReportAbnormalID(AbnormalID::kASC_reading_volume_register,
						"ASC - reading volume register");
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (VOLUME)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x807: /* CLOCK RATE */
				if (WriteMem) {
					/* SoundReg807 = Data; */
					if (0 != Data) {
						ReportAbnormalID(AbnormalID::kASC_nonstandard_CLOCK_RATE,
							"ASC - nonstandard CLOCK RATE");
					}
				} else {
					/* Data = SoundReg807; */
					ReportAbnormalID(AbnormalID::kASC_reading_CLOCK_RATE,
						"ASC - reading CLOCK RATE");
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (CLOCK RATE)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x808: /* CONTROL */
				if (WriteMem) {
					ReportAbnormalID(AbnormalID::kASC_write_to_808, "ASC - write to 808");
				} else {
					/* happens on boot System 7.5.5 */
					Data = 0;
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (CONTROL)",
					Data, WriteMem, addr);
#endif
				break;
			case 0x80A: /* ? */
				if (WriteMem) {
					ReportAbnormalID(AbnormalID::kASC_write_to_80A, "ASC - write to 80A");
				} else {
					/*
						happens in system 6, Lunar Phantom,
							soon after new game.
					*/
					Data = 0;
				}
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (80A)",
					Data, WriteMem, addr);
#endif
				break;
			default:
				if (WriteMem) {
				} else {
					Data = 0;
				}
				ReportAbnormalID(AbnormalID::kASC_unknown_ASC_reg, "ASC - unknown ASC reg");
#if ASC_dolog && 1
				dbglog_AddrAccess("ASC_Access Control (?)",
					Data, WriteMem, addr);
#endif
				break;
		}
	} else if (addr < 0x830) {
		uint8_t b = addr & 3;
		uint8_t chan = ((addr - 0x810) >> 3) & 3;

		if (0 != (addr & 4)) {

			if (WriteMem) {
				ASC_ChanA[chan].freq[b] = Data;
			} else {
				Data = ASC_ChanA[chan].freq[b];
			}
#if ASC_dolog && 1
			dbglog_AddrAccess("ASC_Access Control (frequency)",
				Data, WriteMem, addr);
#endif
#if ASC_dolog && 0
			dbglog_writeCStr("freq b=");
			dbglog_writeNum(WriteMem);
			dbglog_writeCStr(", chan=");
			dbglog_writeNum(chan);
			dbglog_writeReturn();
#endif
		} else {

			if (WriteMem) {
				ASC_ChanA[chan].phase[b] = Data;
			} else {
				Data = ASC_ChanA[chan].phase[b];
			}
#if ASC_dolog && 1
			dbglog_AddrAccess("ASC_Access Control (phase)",
				Data, WriteMem, addr);
#endif
		}
	} else if (addr < 0x838) {
#if ASC_dolog && 1
		dbglog_AddrAccess("ASC_Access Control *** unknown reg",
			Data, WriteMem, addr);
#endif
	} else {
#if ASC_dolog && 1
		dbglog_AddrAccess("ASC_Access Control ? *** unknown reg",
			Data, WriteMem, addr);
#endif

		ReportAbnormalID(AbnormalID::kASC_unknown_ASC_reg_2, "unknown ASC reg");
	}

	return Data;
}

/*
	approximate volume levels of vMac, so:

	x * vol_mult[SoundVolume] >> 16
		+ vol_offset[SoundVolume]
	= {approx} (x - kCenterSound) / (8 - SoundVolume) + kCenterSound;
*/

static const uint16_t vol_mult[] = {
	8192, 9362, 10922, 13107, 16384, 21845, 32768
};

static const trSoundSamp vol_offset[] = {
	28672, 28087, 27307, 26215, 24576, 21846, 16384, 0
};

static const uint8_t SubTick_n[kNumSubTicks] = {
	23,  23,  23,  23,  23,  23,  23,  24,
	23,  23,  23,  23,  23,  23,  23,  24
};
/*
	Generate audio samples for one sub-tick.  In FIFO mode,
	drains the sample buffer; in wavetable mode, steps
	through the 512-byte wave at the programmed frequency.
*/void ASCDevice::subTick(int SubTick)
{
	uint16_t actL;
	tpSoundSamp p;
	uint16_t i;
	uint16_t n = SubTick_n[SubTick];
	uint8_t SoundVolume = SoundReg_Volume;

	while (n > 0) {
		p = MySound_BeginWrite(n, &actL);
		if (actL <= 0) {
			break;
		}

		if (1 == SoundReg801) {
			uint8_t * addr;

			if (0 != (SoundReg802 & 2)) {

			if (! ASC_Playing) {
				if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x200) {
					if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x200)
					{
						SoundReg804 &= ~ 0x01;
						SoundReg804 &= ~ 0x04;
						ASC_Playing = true;
#if ASC_dolog
						dbglog_WriteNote("ASC : start stereo playing");
#endif
					} else {
						if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) == 0)
						if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out))
							>= 370)
						{
#if ASC_dolog
							dbglog_WriteNote("ASC : switch to mono");
#endif
							SoundReg802 &= ~ 2;
							/*
								cludge to get Tetris to work,
								may not actually work on real machine.
							*/
						}
					}
				}
			}

			for (i = 0; i < actL; i++) {
				if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) == 0) {
					ASC_Playing = false;
				}
				if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) == 0) {
					ASC_Playing = false;
				}
				if (! ASC_Playing) {
					*p++ = 0x80;
				} else
				{

				addr = ASC_SampBuff + (ASC_FIFO_Out & 0x3FF);

#if ASC_dolog && 1
				dbglog_StartLine();
				dbglog_writeCStr("out sound ");
				dbglog_writeCStr("[");
				dbglog_writeHex(ASC_FIFO_Out);
				dbglog_writeCStr("]");
				dbglog_writeCStr(" = ");
				dbglog_writeHex(*addr);
				dbglog_writeCStr(" , ");
				dbglog_writeHex(addr[0x400]);
				dbglog_writeReturn();
#endif

				*p++ = ((addr[0] + addr[0x400])
					<< 8
					) >> 1;

				ASC_FIFO_Out += 1;

				}
			}

			} else {

			/* mono */

			if (! ASC_Playing) {
				if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x200)
				{
					SoundReg804 &= ~ 0x01;
					ASC_Playing = true;
#if ASC_dolog
					dbglog_WriteNote("ASC : start mono playing");
#endif
				}
			}

			for (i = 0; i < actL; i++) {
				if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) == 0) {
					ASC_Playing = false;
				}
				if (! ASC_Playing) {
					*p++ = 0x80;
				} else
				{

				addr = ASC_SampBuff + (ASC_FIFO_Out & 0x3FF);

#if ASC_dolog && 1
				dbglog_StartLine();
				dbglog_writeCStr("out sound ");
				dbglog_writeCStr("[");
				dbglog_writeHex(ASC_FIFO_Out);
				dbglog_writeCStr("]");
				dbglog_writeCStr(" = ");
				dbglog_writeHex(*addr);
				dbglog_writeCStr(", in buff: ");
				dbglog_writeHex((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out));
				dbglog_writeReturn();
#endif

				*p++ = (addr[0])
					<< 8
					;

				/* Move the address on */
				/* *addr = 0x80; */
				/* addr += 2; */
				ASC_FIFO_Out += 1;

				}
			}

			}
		} else if (2 == SoundReg801) {
			uint16_t v;
			uint16_t i0;
			uint16_t i1;
			uint16_t i2;
			uint16_t i3;
			uint32_t freq0 = do_get_mem_long(ASC_ChanA[0].freq);
			uint32_t freq1 = do_get_mem_long(ASC_ChanA[1].freq);
			uint32_t freq2 = do_get_mem_long(ASC_ChanA[2].freq);
			uint32_t freq3 = do_get_mem_long(ASC_ChanA[3].freq);
			uint32_t phase0 = do_get_mem_long(ASC_ChanA[0].phase);
			uint32_t phase1 = do_get_mem_long(ASC_ChanA[1].phase);
			uint32_t phase2 = do_get_mem_long(ASC_ChanA[2].phase);
			uint32_t phase3 = do_get_mem_long(ASC_ChanA[3].phase);
#if ASC_dolog && 1
			dbglog_writeCStr("freq0=");
			dbglog_writeNum(freq0);
			dbglog_writeCStr(", freq1=");
			dbglog_writeNum(freq1);
			dbglog_writeCStr(", freq2=");
			dbglog_writeNum(freq2);
			dbglog_writeCStr(", freq3=");
			dbglog_writeNum(freq3);
			dbglog_writeReturn();
#endif
			for (i = 0; i < actL; i++) {

				phase0 += freq0;
				phase1 += freq1;
				phase2 += freq2;
				phase3 += freq3;


#if 1
				i0 = ((phase0 + 0x4000) >> 15) & 0x1FF;
				i1 = ((phase1 + 0x4000) >> 15) & 0x1FF;
				i2 = ((phase2 + 0x4000) >> 15) & 0x1FF;
				i3 = ((phase3 + 0x4000) >> 15) & 0x1FF;
#else
				i0 = ((phase0 + 0x8000) >> 16) & 0x1FF;
				i1 = ((phase1 + 0x8000) >> 16) & 0x1FF;
				i2 = ((phase2 + 0x8000) >> 16) & 0x1FF;
				i3 = ((phase3 + 0x8000) >> 16) & 0x1FF;
#endif

				v = ASC_SampBuff[i0]
					+ ASC_SampBuff[0x0200 + i1]
					+ ASC_SampBuff[0x0400 + i2]
					+ ASC_SampBuff[0x0600 + i3];

#if ASC_dolog && 1
				dbglog_StartLine();
				dbglog_writeCStr("i0=");
				dbglog_writeNum(i0);
				dbglog_writeCStr(", i1=");
				dbglog_writeNum(i1);
				dbglog_writeCStr(", i2=");
				dbglog_writeNum(i2);
				dbglog_writeCStr(", i3=");
				dbglog_writeNum(i3);
				dbglog_writeCStr(", output sound v=");
				dbglog_writeNum(v);
				dbglog_writeReturn();
#endif

				*p++ = (v >> 2);
			}

			do_put_mem_long(ASC_ChanA[0].phase, phase0);
			do_put_mem_long(ASC_ChanA[1].phase, phase1);
			do_put_mem_long(ASC_ChanA[2].phase, phase2);
			do_put_mem_long(ASC_ChanA[3].phase, phase3);
		} else {
			for (i = 0; i < actL; i++) {
				*p++ = kCenterSound;
			}
		}


		if (SoundVolume < 7) {
			/*
				Usually have volume at 7, so this
				is just for completeness.
			*/
			uint32_t mult = (uint32_t)vol_mult[SoundVolume];
			trSoundSamp offset = vol_offset[SoundVolume];

			p -= actL;
			for (i = 0; i < actL; i++) {
				*p = (trSoundSamp)((uint32_t)(*p) * mult >> 16) + offset;
				++p;
			}
		}

		MySound_EndWrite(actL);
		n -= actL;
	}

#if 1
	if ((1 == SoundReg801) && ASC_Playing) {
		if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x200) {
			if (0 != (SoundReg804 & 0x01)) {
				ReportAbnormalID(AbnormalID::kASC_half_flag_A_not_already_clear,
					"half flag A not already clear");
				SoundReg804 &= ~ 0x01;
			}
		} else {
			if (0 != (SoundReg804 & 0x01)) {
				/* happens in lode runner */
			} else {
#if ASC_dolog
				dbglog_WriteNote("setting half flag A");
#endif
				if (auto* via2 = machine_->findDevice<VIA2Device>())
					via2->iCB1_PulseNtfy();
				SoundReg804 |= 0x01;
			}
		}
		if (((uint16_t)(ASC_FIFO_InA - ASC_FIFO_Out)) >= 0x400) {
			if (0 == (SoundReg804 & 0x02)) {
				ReportAbnormalID(AbnormalID::kASC_full_flag_A_not_already_set, "full flag A not already set");
				SoundReg804 |= 0x02;
			}
		} else {
			if (0 != (SoundReg804 & 0x02)) {
				/* ReportAbnormal("full flag A not already clear"); */
				SoundReg804 &= ~ 0x02;
			}
		}
		if (0 != (SoundReg802 & 2)) {
			if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x200) {
				if (0 != (SoundReg804 & 0x04)) {
					ReportAbnormalID(AbnormalID::kASC_half_flag_B_not_already_clear,
						"half flag B not already clear");
					SoundReg804 &= ~ 0x04;
				}
			} else {
				if (0 != (SoundReg804 & 0x04)) {
					/* happens in Lunar Phantom */
				} else {
#if ASC_dolog
					dbglog_WriteNote("setting half flag B");
#endif
					if (auto* via2 = machine_->findDevice<VIA2Device>())
						via2->iCB1_PulseNtfy();
					SoundReg804 |= 0x04;
				}
			}
			if (((uint16_t)(ASC_FIFO_InB - ASC_FIFO_Out)) >= 0x400) {
				if (0 == (SoundReg804 & 0x08)) {
					ReportAbnormalID(AbnormalID::kASC_full_flag_B_not_already_set,
						"full flag B not already set");
					SoundReg804 |= 0x08;
				}
			} else {
				if (0 != (SoundReg804 & 0x08)) {
					/*
						ReportAbnormal("full flag B not already clear");
					*/
					SoundReg804 &= ~ 0x08;
				}
			}
		}
	}
#endif
}
