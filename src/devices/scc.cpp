/*
	Serial Communications Controller EMulated DEVice

	additions for LocalTalk networking support
		Copyright 2011-2012, Michael Fort
		enabled with "EmLocalTalk"

	-- original description: --

	Emulates the Z8530 SCC found in the Mac Plus.
		But only the minimum amount needed to emulate
		normal operation in a Mac Plus with nothing
		connected to the serial ports.
		(and not even that much is complete yet)

	This code adapted from "s_scc.c" in vMac by Philip Cummins.
	With additional code by Weston Pawlowski from the Windows
	port of vMac.

	Further information was found in the
	"Zilog SCC/ESCC User's Manual".
*/

#include "core/common.h"

#include "devices/scc.h"
#include "devices/serial_backend.h"
#include "core/wire_bus.h"
#include "core/machine_obj.h"
#include "core/abnormal_ids.h"

/* Serial backends — one per channel (0 = A/modem, 1 = B/printer). */
static std::unique_ptr<SerialBackend> s_serialBackend[2];

/* Global singleton */

/*
	ReportAbnormalID unused 0x074D - 0x07FF
*/

#define SCC_dolog 0
#define SCC_TrackMore 0

#if EmLocalTalk

static bool s_ctsPacketPending = false;
static uint8_t s_ctsPacketRxDA;
static uint8_t s_ctsPacketRxSA;

static bool s_isFindingNode = false;

/*
	Function used when all the tx data is sent to the SCC as indicated
	by resetting the TX underrun/EOM latch.  If the transmit packet is
	a unicast RTS LAPD packet, we fake the corresponding CTS LAPD
	packet.  This is okay because it is only a collision avoidance
	mechanism and the Ethernet device itself and BPF automatically
	handle collision detection and retransmission.  Besides this is
	what a standard AppleTalk (LocalTalk to EtherTalk) bridge does.
*/
static void LT_TransmitPacket1()
{
	/* Check for LLAP RTS/CTS packets, which we won't send */
#if SCC_dolog
	dbglog_WriteNote("SCC sending packet to UDP");
	dbglog_StartLine();
	dbglog_writelnNum("LT_TxBuffSz", g_ltTxBuffSz);
#endif

	if (g_ltTxBuffSz < 3) {
		ReportAbnormalID(AbnormalID::kSCC_packet_too_small_in,
			"packet too small in "
				"in LT_TransmitPacket1");
	} else {
		uint8_t type = g_ltTxBuffer[2];

#if SCC_dolog
		dbglog_StartLine();
		dbglog_writelnNum("dst", g_ltTxBuffer[0]);
		dbglog_StartLine();
		dbglog_writelnNum("src", g_ltTxBuffer[1]);
		dbglog_StartLine();
		dbglog_writelnNum("type", type);
#endif

		if (type < 0x80) {
			/* data packet */
#if LT_MayHaveEcho
			s_isFindingNode = false;
#endif
			LT_TransmitPacket();
		} else {
			/* control packet */

			if (3 != g_ltTxBuffSz) {
				ReportAbnormalID(AbnormalID::kSCC_unexpected_size_of_control_packet_in,
					"unexpected size of control packet in "
						"in LT_TransmitPacket1");
			}

			if (0x81 == type) {
#if SCC_dolog
				dbglog_WriteNote(
					"SCC LLAP packet lapENQ");
#endif
#if LT_MayHaveEcho
				s_isFindingNode = true;
#endif
				LT_TransmitPacket();
			} else
			if (0x82 == type) {
#if SCC_dolog
				dbglog_WriteNote(
					"SCC LLAP packet lapACK");
#endif
				LT_TransmitPacket();
			} else
			if (0x84 == type) {
				/* lapRTS - Request to send*/
				if (0xFF == g_ltTxBuffer[0]) {
#if SCC_dolog
					dbglog_WriteNote(
						"SCC LLAP packet ignore broadcast lapRTS");
#endif
				} else
				if (s_ctsPacketPending) {
					ReportAbnormalID(AbnormalID::kSCC_Already_CTSpacketPending,
						"Already s_ctsPacketPending "
							"in LT_TransmitPacket1");
				} else
				{
#if SCC_dolog
					dbglog_WriteNote(
						"SCC LLAP packet lapRTS");
#endif
					s_ctsPacketRxDA = g_ltTxBuffer[1]; /* rx da = tx sa */
					s_ctsPacketRxSA = g_ltTxBuffer[0]; /* rx sa = tx da */
					s_ctsPacketPending = true;
				}
			} else
			if (0x85 == type) {
				/* ignore lapCTS - Clear To Send */
#if SCC_dolog
				dbglog_WriteNote(
					"SCC LLAP packet lapCTS");
#endif
			} else
			{
#if SCC_dolog
				dbglog_WriteNote(
					"SCC LLAP packet unknown");
#endif

				LT_TransmitPacket();
			}
		}
	}
}

static uint8_t MyCTSBuffer[4];

static void GetCTSpacket()
{
	/* Get a single buffer worth of packets at a time */
	uint8_t * device_buffer = MyCTSBuffer;

#if SCC_dolog
	dbglog_WriteNote("SCC receiving CTS packet");
#endif
	/* Create the fake response from the other node */
	device_buffer[0] = s_ctsPacketRxDA;
	device_buffer[1] = s_ctsPacketRxSA;
	device_buffer[2] = 0x85;          /* llap cts */

	/* Start the receiver */
	g_ltRxBuffer = device_buffer;
	g_ltRxBuffSz = 3;

	s_ctsPacketPending = false;
}

/* LLAP/SDLC address */
static uint8_t my_node_address = 0;

static bool s_ltAddrSrchMd = false;

static void GetNextPacketForMe()
{
	uint8_t dst;
	uint8_t src;
	uint8_t type;

	for (;;) {
		LT_ReceivePacket();

		if (nullptr == g_ltRxBuffer) {
			break;
		}

		/* Is this packet destined for me? */
		dst = g_ltRxBuffer[0];
		src = g_ltRxBuffer[1];
		type = g_ltRxBuffer[2];

#if SCC_dolog
		dbglog_StartLine();
		dbglog_writeln("SCC receiving packet from UDP");
		dbglog_writelnNum("LT_RxBuffSz", g_ltRxBuffSz);
		dbglog_writelnNum("dst", dst);
		dbglog_writelnNum("src", src);
		dbglog_writelnNum("type", type);
#endif

		if ((dst != my_node_address)
			&& (dst != 0xFF)
			&& s_ltAddrSrchMd)
		{
#if SCC_dolog
			dbglog_WriteNote("SCC ignore packet not for me");
#endif
			g_ltRxBuffer = nullptr;
			continue;
		}
#if LT_MayHaveEcho
		if (g_certainlyNotMyPacket) {
#if SCC_dolog
			dbglog_WriteNote("CertainlyNotMyPacket");
#endif
		} else
		if (src != my_node_address) {
			/* we definitely did not send it, so ok */
		} else
		/*
			we should ignore packets "from" myself except ACK packets,
			which tell me that we've got an address collision,
			and ENQ packets which tell me I might be about to
		*/
		if (0x81 == type) {
			if (! s_isFindingNode) {
				/* pass it on for lapACK reply */
#if SCC_dolog
				dbglog_WriteNote("received lapENQ to us");
#endif
			} else {
				/* probably this is ourself, ignore */
#if SCC_dolog
				dbglog_WriteNote("received lapENQ probably from us");
#endif
				g_ltRxBuffer = nullptr;
				continue;
			}
		} else
		if (0x82 == type) {
			if (! s_isFindingNode) {
#if SCC_dolog
				dbglog_WriteNote("received lapACK probably from us");
#endif
				g_ltRxBuffer = nullptr;
				continue;
			} else {
				/* lapACK, pass it on handle collision */
#if SCC_dolog
				dbglog_WriteNote("received lapACK to us");
#endif
			}
		} else
		{
#if SCC_dolog
			dbglog_WriteNote("SCC ignore packet from myself");
#endif
			g_ltRxBuffer = nullptr;
			continue;
		}
#else
		{
			/*
				checking for own packets isn't needed, because of
				packetIsOneISent check. if someone else is masquerading
				as our address, it probably is more accurate emulation
				to accept the packet.
			*/

			/* ok */
		}
#endif
		break;
	}
}

static void LT_ReceivePacket1()
{
	if (s_ctsPacketPending)  {
		GetCTSpacket();
	} else {
		GetNextPacketForMe();
	}
}

static void LT_AddrSrchMdSet(bool v)
{
#if SCC_dolog
	dbglog_StartLine();
	dbglog_writelnNum("LT_AddrSrchMdSet", v);
#endif
	s_ltAddrSrchMd = v;
}

static void LT_NodeAddressSet(uint8_t v)
{
#if SCC_dolog
	dbglog_StartLine();
	dbglog_writelnNum("LT_NodeAddressSet", v);
#endif
	if (0 != v) {
		my_node_address = v;
	}
}

#endif /* EmLocalTalk */

/* Just to make things a little easier */

/* SCC Interrupts */
#define SCC_A_Rx       8 /* Rx Char Available */
#define SCC_A_Rx_Spec  7 /* Rx Special Condition */
#define SCC_A_Tx_Empty 6 /* Tx Buffer Empty */
#define SCC_A_Ext      5 /* External/Status Change */
#define SCC_B_Rx       4 /* Rx Char Available */
#define SCC_B_Rx_Spec  3 /* Rx Special Condition */
#define SCC_B_Tx_Empty 2 /* Tx Buffer Empty */
#define SCC_B_Ext      1 /* External/Status Change */

struct Channel_Ty {
	bool TxEnable;
	bool RxEnable;
	bool TxIE; /* Transmit Interrupt Enable */
	bool TxUnderrun;
	bool SyncHunt;
	bool TxIP; /* Transmit Interrupt Pending */
	uint8_t RxBuff;
	/*
		TxBufferEmpty: true when the transmit buffer is empty.
		Without EmLocalTalk or a serial backend this is always
		true, but the field must exist unconditionally now.
	*/
	bool TxBufferEmpty;
#if EmLocalTalk || SCC_TrackMore
	bool ExtIE;
#endif
#if SCC_TrackMore
	bool WaitRqstEnbl;
#endif
#if SCC_TrackMore
	bool WaitRqstSlct;
#endif
#if SCC_TrackMore
	bool WaitRqstRT;
#endif
#if SCC_TrackMore
	bool PrtySpclCond;
#endif
#if SCC_TrackMore
	bool PrtyEnable;
#endif
#if SCC_TrackMore
	bool PrtyEven;
#endif
#if SCC_TrackMore
	bool RxCRCEnbl;
#endif
#if SCC_TrackMore
	bool TxCRCEnbl;
#endif
#if SCC_TrackMore
	bool RTSctrl;
#endif
#if SCC_TrackMore
	bool SndBrkCtrl;
#endif
#if SCC_TrackMore
	bool DTRctrl;
#endif
#if EmLocalTalk || SCC_TrackMore
	bool AddrSrchMd;
#endif
#if SCC_TrackMore
	bool SyncChrLdInhb;
#endif
#if SCC_TrackMore
	uint8_t ClockRate;
#endif
#if SCC_TrackMore
	uint8_t DataEncoding;
#endif
#if SCC_TrackMore
	uint8_t TRxCsrc;
#endif
#if SCC_TrackMore
	uint8_t TClkSlct;
#endif
#if SCC_TrackMore
	uint8_t RClkSlct;
#endif
#if SCC_TrackMore
	uint8_t RBitsPerChar;
#endif
#if SCC_TrackMore
	uint8_t TBitsPerChar;
#endif
	uint8_t RxIntMode;
	bool FirstChar;
#if EmLocalTalk || SCC_TrackMore
	uint8_t SyncMode;
#endif
#if SCC_TrackMore
	uint8_t StopBits;
#endif
	bool RxChrAvail;
#if EmLocalTalk
	/* EndOfFrame only used by LocalTalk framing */
	bool EndOfFrame;
#endif
#if SCC_TrackMore /* don't care about CTS_IE */
	bool CTS_IE;
#endif
#if SCC_TrackMore
	bool CRCPreset;
#endif
#if SCC_TrackMore
	bool BRGEnbl;
#endif
#if SCC_TrackMore /* don't care about BreakAbortIE */
	bool BreakAbortIE;
#endif
#if SCC_TrackMore /* don't care about Baud */
	uint8_t BaudLo;
	uint8_t BaudHi;
#endif
};

struct SCC_Ty {
	Channel_Ty a[2]; /* 0 = channel A, 1 = channel B */
	int SCC_Interrupt_Type;
	int PointerBits;
	uint8_t InterruptVector;
	bool MIE; /* master interrupt enable */
#if SCC_TrackMore
	bool NoVectorSlct;
#endif
};

static SCC_Ty s_scc;


#if EmLocalTalk
static int rx_data_offset = 0;
	/* when data pending, this is used */
#endif

bool SCCDevice::interruptsEnabled()
{
	return s_scc.MIE;
}

/* ---- */

/* Function used to update the interrupt state of the SCC */
static void CheckSCCInterruptFlag()
{
	uint8_t NewSCCInterruptRequest;

	/* --- Channel A receive interrupts (serial backend) --- */
	bool ReceiveAInterrupt = false;
	bool RxSpclAInterrupt = false;

	if (s_serialBackend[0]) {
		switch (s_scc.a[0].RxIntMode) {
			case 0: break;
			case 1:
				if (s_scc.a[0].RxChrAvail && s_scc.a[0].FirstChar)
					ReceiveAInterrupt = true;
				break;
			case 2:
				if (s_scc.a[0].RxChrAvail)
					ReceiveAInterrupt = true;
				break;
			case 3: break;
		}
	}

	/* --- Channel B receive interrupts (LocalTalk or serial backend) --- */
	bool ReceiveBInterrupt = false;
	bool RxSpclBInterrupt = false
#if EmLocalTalk
		| s_scc.a[1].EndOfFrame
#endif
		;

	if (s_serialBackend[1]
#if EmLocalTalk
		|| true /* LocalTalk also uses these fields on ch B */
#endif
	) {
		switch (s_scc.a[1].RxIntMode) {
			case 0:
				RxSpclBInterrupt = false;
				break;
			case 1:
				if (s_scc.a[1].RxChrAvail && s_scc.a[1].FirstChar)
					ReceiveBInterrupt = true;
				break;
			case 2:
				if (s_scc.a[1].RxChrAvail)
					ReceiveBInterrupt = true;
				break;
			case 3: break;
		}
	}

	/* Master Interrupt Enable */
	if (! s_scc.MIE) {
		s_scc.SCC_Interrupt_Type = 0;
	} else
	/* Z8530 priority: A Rx > A Tx > B Rx > B Tx */
	if (ReceiveAInterrupt) {
		s_scc.SCC_Interrupt_Type = SCC_A_Rx;
	} else
	if (RxSpclAInterrupt) {
		s_scc.SCC_Interrupt_Type = SCC_A_Rx_Spec;
	} else
	if (s_scc.a[0].TxIP && s_scc.a[0].TxIE) {
		s_scc.SCC_Interrupt_Type = SCC_A_Tx_Empty;
	} else
	if (ReceiveBInterrupt) {
		s_scc.SCC_Interrupt_Type = SCC_B_Rx;
	} else
	if (RxSpclBInterrupt) {
		s_scc.SCC_Interrupt_Type = SCC_B_Rx_Spec;
	} else
	if (s_scc.a[1].TxIP && s_scc.a[1].TxIE) {
		s_scc.SCC_Interrupt_Type = SCC_B_Tx_Empty;
	} else
	{
		s_scc.SCC_Interrupt_Type = 0;
	}

	NewSCCInterruptRequest = (s_scc.SCC_Interrupt_Type != 0) ? 1 : 0;
	if (NewSCCInterruptRequest != SCCInterruptRequest) {
#if SCC_dolog
		dbglog_WriteSetBool("SCCInterruptRequest change",
			NewSCCInterruptRequest);

		dbglog_StartLine();
		dbglog_writeCStr("s_scc.SCC_Interrupt_Type <- ");
		dbglog_writeHex(s_scc.SCC_Interrupt_Type);
		dbglog_writeReturn();
#endif
		g_wires.set(Wire_SCCInterruptRequest, NewSCCInterruptRequest);
	}
}

static void SCC_InitChannel(int chan)
{
	/* anything not done by ResetChannel */

	s_scc.a[chan].SyncHunt = true;
#if SCC_TrackMore /* don't care about Baud */
	s_scc.a[chan].BaudLo = 0;
	s_scc.a[chan].BaudHi = 0;
#endif
#if SCC_TrackMore
	s_scc.a[chan].BRGEnbl = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].TRxCsrc = 0;
#endif
#if SCC_TrackMore
	s_scc.a[chan].TClkSlct = 1;
#endif
#if SCC_TrackMore
	s_scc.a[chan].RClkSlct = 0;
#endif
}

/* Clear all per-channel register state to post-reset defaults. */
static void SCC_ResetChannel(int chan)
{
/* RR 0 */
	s_scc.a[chan].RxBuff = 0;
	s_scc.a[chan].RxChrAvail = false;
	s_scc.a[chan].TxBufferEmpty = true;
	s_scc.a[chan].TxUnderrun = true;
/* RR 1 */
#if EmLocalTalk
	/* otherwise EndOfFrame always false */
	s_scc.a[chan].EndOfFrame = false;
#endif
/* RR 3 */
#if EmLocalTalk || SCC_TrackMore
	s_scc.a[chan].ExtIE = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].RxCRCEnbl = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].TxCRCEnbl = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].RTSctrl = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].SndBrkCtrl = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].DTRctrl = false;
#endif
#if EmLocalTalk || SCC_TrackMore
	s_scc.a[chan].AddrSrchMd = false;
	if (0 != chan) {
		LT_AddrSrchMdSet(false);
	}
#endif
#if SCC_TrackMore
	s_scc.a[chan].SyncChrLdInhb = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].WaitRqstEnbl = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].WaitRqstSlct = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].WaitRqstRT = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].PrtySpclCond = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].PrtyEnable = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].PrtyEven = false;
#endif
#if SCC_TrackMore
	s_scc.a[chan].ClockRate = 0;
#endif
#if SCC_TrackMore
	s_scc.a[chan].DataEncoding = 0;
#endif
#if SCC_TrackMore
	s_scc.a[chan].RBitsPerChar = 0;
#endif
#if SCC_TrackMore
	s_scc.a[chan].TBitsPerChar = 0;
#endif
	s_scc.a[chan].RxIntMode = 0;
	s_scc.a[chan].FirstChar = false;
#if EmLocalTalk || SCC_TrackMore
	s_scc.a[chan].SyncMode = 0;
#endif
#if SCC_TrackMore
	s_scc.a[chan].StopBits = 0;
#endif
#if SCC_TrackMore
	s_scc.NoVectorSlct = false;
#endif
	s_scc.a[chan].TxIP = false;

	s_scc.a[chan].TxEnable = false;
	s_scc.a[chan].RxEnable = false;
	s_scc.a[chan].TxIE = false;

#if SCC_TrackMore /* don't care about CTS_IE */
	s_scc.a[chan].CTS_IE = true;
#endif
#if SCC_TrackMore
	s_scc.a[chan].CRCPreset = false;
#endif
#if SCC_TrackMore /* don't care about BreakAbortIE */
	s_scc.a[chan].BreakAbortIE = true;
#endif

	s_scc.PointerBits = 0;

}

/* Reset all SCC state: clear interrupt vector, pointer bits, and
   reinitialise both channels to power-on defaults. */
void SCCDevice::reset()
{
	g_wires.set(Wire_VIA1_iA7_SCCwaitrq, 1);

	s_scc.SCC_Interrupt_Type = 0;

	g_wires.set(Wire_SCCInterruptRequest, 0);
	s_scc.PointerBits = 0;
	s_scc.MIE = false;
	s_scc.InterruptVector = 0;

	SCC_InitChannel(1);
	SCC_InitChannel(0);

	SCC_ResetChannel(1);
	SCC_ResetChannel(0);
}


#if EmLocalTalk

static void SCC_TxBuffPut(uint8_t Data)
{
	/* Buffer the data in the transmit buffer */
	if (g_ltTxBuffSz < LT_TxBfMxSz) {
		g_ltTxBuffer[g_ltTxBuffSz] = Data;
		++g_ltTxBuffSz;
	}
}

/*
	This function is called once all the normal packet bytes have been
	received.
*/
static void rx_complete()
{
	if (s_scc.a[1].EndOfFrame) {
		ReportAbnormalID(AbnormalID::kSCC_EndOfFrame_true_in_rx_complete, "EndOfFrame true in rx_complete");
	}
	if (! s_scc.a[1].RxChrAvail) {
		ReportAbnormalID(AbnormalID::kSCC_RxChrAvail_false_in_rx_complete, "RxChrAvail false in rx_complete");
	}
	if (s_scc.a[1].SyncHunt) {
		ReportAbnormalID(AbnormalID::kSCC_SyncHunt_true_in_rx_complete, "SyncHunt true in rx_complete");
	}

	/*
		Need to wait for rx_eof_pending (end of frame) to clear before
		preparing the next packet for receive.
	*/
	g_ltRxBuffer = nullptr;

	s_scc.a[1].EndOfFrame = true;
}

static void SCC_RxBuffAdvance()
{
	uint8_t value;

	/*
		From the manual:
		"If status is checked, it must be done before the data is read,
		because the act of reading the data pops both the data and
		error FIFOs."
	*/

	if (nullptr == g_ltRxBuffer) {
		value = 0x7E;
		s_scc.a[1].RxChrAvail = false;
	} else {
		if (rx_data_offset < g_ltRxBuffSz) {
			value = g_ltRxBuffer[rx_data_offset];
		} else {
			uint32_t i = rx_data_offset - g_ltRxBuffSz;

			/* if i==0 in first byte of CRC, have not got EOF yet */
			if (i == 1) {
				rx_complete();
			}

			value = 0;
		}
		++rx_data_offset;
	}

	s_scc.a[1].RxBuff = value;
}

/*
	External function, called periodically, to poll for any new LTOE
	packets. Any new packets are queued into the packet receipt queue.
*/
void SCCDevice::localTalkTick()
{
	if (s_scc.a[1].RxEnable
		&& (! s_scc.a[1].RxChrAvail))
	{
		if (nullptr != g_ltRxBuffer) {
#if SCC_dolog
			dbglog_WriteNote("SCC recover abandoned packet");
#endif
		} else {
			LT_ReceivePacket1();
		}

		if (nullptr != g_ltRxBuffer) {
			rx_data_offset  = 0;
			s_scc.a[1].EndOfFrame = false;
			s_scc.a[1].RxChrAvail = true;
			s_scc.a[1].SyncHunt = false;

			SCC_RxBuffAdvance();
			/* We can update the rx interrupt if enabled */
			CheckSCCInterruptFlag();
		}
	}
}

#endif




#if SCC_dolog
static void SCC_DbgLogChanStartLine(int chan)
{
	dbglog_StartLine();
	dbglog_writeCStr("SCC chan(");
	if (chan) {
		dbglog_writeCStr("B");
	} else {
		dbglog_writeCStr("A");
	}
	/* dbglog_writeHex(chan); */
	dbglog_writeCStr(")");
}
#endif

static uint8_t SCC_GetRR0(int chan)
{
	/* happens on boot always */

	return 0
		| (s_scc.a[chan].TxUnderrun ? (1 << 6) : 0)
		| (s_scc.a[chan].SyncHunt ? (1 << 4) : 0)
		| (s_scc.a[chan].TxBufferEmpty ? (1 << 2) : 0)
		| (s_scc.a[chan].RxChrAvail ? (1 << 0) : 0)
		/* When a serial backend is attached, assert DCD (bit 3)
		   and CTS (bit 5) so guest software sees a connected device. */
		| (s_serialBackend[chan] ? ((1 << 5) | (1 << 3)) : 0)
		;
}

static uint8_t SCC_GetRR1(int chan)
{
	/* happens in MacCheck */

	uint8_t value;
#if ! EmLocalTalk
	UNUSED(chan);
#endif

	value = (1 << 2) | (1 << 1)
		| (1 << 0)
#if EmLocalTalk
		/* otherwise EndOfFrame always false */
		| (s_scc.a[chan].EndOfFrame ? (1 << 7) : 0)
#endif
		;

	return value;
}

static uint8_t SCC_GetRR2(int chan)
{
	/* happens in MacCheck */
	/* happens in Print to ImageWriter */

	uint8_t value = s_scc.InterruptVector;

	if (0 != chan) { /* B Channel */
		{
			/* Status Low */
			value = value
				& ((1 << 0) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));

			switch (s_scc.SCC_Interrupt_Type) {
				case SCC_A_Rx:
					value |= (1 << 3) | (1 << 2);
					break;

				case SCC_A_Rx_Spec:
					value |= (1 << 3) | (1 << 2) | (1 << 1);
					break;

				case SCC_A_Tx_Empty:
					value |= (1 << 3);
					break;

				case SCC_A_Ext:
					value |= (1 << 3) | (1 << 1);
					break;

				case SCC_B_Rx:
					value |= (1 << 2);
					break;

				case SCC_B_Rx_Spec:
					value |= (1 << 2) | (1 << 1);
					break;

				case SCC_B_Tx_Empty:
					value |= 0;
					break;

				case SCC_B_Ext:
					value |= (1 << 1);
					break;

				default:
					value |= (1 << 2) | (1 << 1);
					break;
			}
		}

		/* s_scc.SCC_Interrupt_Type = 0; */
	}

	return value;
}

static uint8_t SCC_GetRR3(int chan)
{
	uint8_t value = 0;

	UNUSED(chan);
	ReportAbnormalID(AbnormalID::kSCC_RR_3, "RR 3");


	return value;
}

static uint8_t SCC_GetRR8(int chan)
{
	uint8_t value = 0;

	/* Receive Buffer */
	/* happens on boot with appletalk on */
	if (s_scc.a[chan].RxEnable) {
		if (s_serialBackend[chan] && s_scc.a[chan].RxChrAvail) {
			/* Serial backend path: return buffered byte */
			value = s_scc.a[chan].RxBuff;
			s_scc.a[chan].RxChrAvail = false;
			s_scc.a[chan].FirstChar = false;
			CheckSCCInterruptFlag();
		} else
#if EmLocalTalk
		if (0 != chan) {
			/*
				Check the receive state, handling a complete rx
				if necessary
			*/
			value = s_scc.a[1].RxBuff;
			s_scc.a[1].FirstChar = false;
			SCC_RxBuffAdvance();
		} else {
			value = 0x7E;
		}
#else
		/* Rx Enable */
		if (!g_machine->config().isSEOrLater()) {
			ReportAbnormalID(AbnormalID::kSCC_read_rr8_when_RxEnable, "read rr8 when RxEnable");
		}

		/* Input 1 byte from Modem Port/Printer into Data */
#endif
	} else {
		/* happens on boot with appletalk on */
	}

	return value;
}

static uint8_t SCC_GetRR10(int chan)
{
	/* happens on boot with appletalk on */

	uint8_t value = 0;
	UNUSED(chan);


	return value;
}

static uint8_t SCC_GetRR12(int chan)
{
	uint8_t value = 0;

#if ! SCC_TrackMore
	UNUSED(chan);
#endif
	ReportAbnormalID(AbnormalID::kSCC_RR_12, "RR 12");

#if SCC_TrackMore /* don't care about Baud */
	value = s_scc.a[chan].BaudLo;
#endif

	return value;
}

static uint8_t SCC_GetRR13(int chan)
{
	uint8_t value = 0;

#if ! SCC_TrackMore
	UNUSED(chan);
#endif
	ReportAbnormalID(AbnormalID::kSCC_RR_13, "RR 13");

#if SCC_TrackMore /* don't care about Baud */
	value = s_scc.a[chan].BaudHi;
#endif

	return value;
}

static uint8_t SCC_GetRR15(int chan)
{
	uint8_t value = 0;

	UNUSED(chan);
	ReportAbnormalID(AbnormalID::kSCC_RR_15, "RR 15");


	return value;
}

#if SCC_dolog
static void SCC_DbgLogChanCmnd(int chan, char *s)
{
	SCC_DbgLogChanStartLine(chan);
	dbglog_writeCStr(" ");
	dbglog_writeCStr(s);
	dbglog_writeReturn();
}
#endif

#if SCC_dolog
static void SCC_DbgLogChanChngBit(int chan, char *s, bool v)
{
	SCC_DbgLogChanStartLine(chan);
	dbglog_writeCStr(" ");
	dbglog_writeCStr(s);
	dbglog_writeCStr(" <- ");
	if (v) {
		dbglog_writeCStr("1");
	} else {
		dbglog_writeCStr("0");
	}
	dbglog_writeReturn();
}
#endif

static void SCC_PutWR0(uint8_t Data, int chan)
/*
	"CRC initialize, initialization commands for the various modes,
	Register Pointers"
*/
{
	switch ((Data >> 6) & 3) {
		case 1:
			ReportAbnormalID(AbnormalID::kSCC_Reset_Rx_CRC_Checker, "Reset Rx CRC Checker");
			break;
		case 2:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "Reset Tx CRC Generator");
#endif
			/* happens on boot with appletalk on */
			break;
		case 3:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan,
				"Reset Tx Underrun/EOM Latch");
#endif
			/* happens on boot with appletalk on */
#if EmLocalTalk
			/*
				This is the indication we are done transmitting
				data for the current packet.
			*/
			if (0 != chan) {
				LT_TransmitPacket1();
			}
#endif
			break;
		case 0:
		default:
			/* Null Code */
			break;
	}
	s_scc.PointerBits = Data & 0x07;
	switch ((Data >> 3) & 7) {
		case 1: /* Point High */
			s_scc.PointerBits |= 8;
			break;
		case 2:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "Reset Ext/Status Ints");
#endif
			/* happens on boot always */
			s_scc.a[chan].SyncHunt = false;
			break;
		case 3:
			ReportAbnormalID(AbnormalID::kSCC_Send_Abort_SDLC, "Send Abort (SDLC)");
#if EmLocalTalk
			s_scc.a[chan].TxBufferEmpty = true;
#endif
			break;
		case 4:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan,
				"Enable Int on next Rx char");
#endif
#if EmLocalTalk || SCC_TrackMore
			s_scc.a[chan].FirstChar = true;
#endif
			/* happens in MacCheck */
			break;
		case 5:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "Reset Tx Int Pending");
#endif
			/* happens in MacCheck */
			/* happens in Print to ImageWriter */
			s_scc.a[chan].TxIP = false;
			CheckSCCInterruptFlag();
			break;
		case 6:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "Error Reset");
#endif
			/* happens on boot with appletalk on */
#if EmLocalTalk
			s_scc.a[chan].EndOfFrame = false;
#endif
			break;
		case 7:
			/* happens in "Network Watch" program (Cayman Systems) */
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "Reset Highest IUS");
#endif
			break;
		case 0:
		default:
			/* Null Code */
			break;
	}
}

static void SCC_PutWR1(uint8_t Data, int chan)
/*
	"Transmit/Receive interrupt and data transfer mode definition"
*/
{
#if EmLocalTalk || SCC_TrackMore
	{
		bool NewExtIE = (Data & (1 << 0)) != 0;
		if (s_scc.a[chan].ExtIE != NewExtIE) {
			s_scc.a[chan].ExtIE = NewExtIE;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan, "EXT INT Enable",
				NewExtIE);
#endif
			/*
				set to 1 on start up, set to 0 in MacCheck
				and in Print to ImageWriter
			*/
		}
	}
#endif

	{
		bool NewTxIE = (Data & (1 << 1)) != 0;
		if (s_scc.a[chan].TxIE != NewTxIE) {
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan, "Tx Int Enable",
				NewTxIE);
#endif
			/* happens in MacCheck */
			/* happens in Print to ImageWriter */
			s_scc.a[chan].TxIE = NewTxIE;
			CheckSCCInterruptFlag();
		}
	}

#if SCC_TrackMore
	{
		bool NewPrtySpclCond = (Data & (1 << 2)) != 0;
		if (s_scc.a[chan].PrtySpclCond != NewPrtySpclCond) {
			s_scc.a[chan].PrtySpclCond = NewPrtySpclCond;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Parity is special condition", NewPrtySpclCond);
#endif
			/*
				set to 1 in MacCheck
				and in Print to ImageWriter
			*/
		}
	}
#endif

	{
		uint8_t NewRxIntMode = (Data >> 3) & 3;
		if (s_scc.a[chan].RxIntMode != NewRxIntMode) {
			s_scc.a[chan].RxIntMode = NewRxIntMode;

			switch (NewRxIntMode) {
				case 0:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan, "Rx INT Disable");
#endif
					/* happens on boot always */
					break;
				case 1:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Rx INT on 1st char"
						" or special condition");
#endif
					s_scc.a[chan].FirstChar = true;
					/* happens on boot with appletalk on */
					break;
				case 2:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"INT on all Rx char"
						" or special condition");
#endif
					/* happens in MacCheck */
					/* happens in Print to ImageWriter */
					break;
				case 3:
					ReportAbnormalID(AbnormalID::kSCC_Rx_INT_on_special_condition_only,
						"Rx INT on special condition only");
					break;
			}
		}
	}

#if SCC_TrackMore
	{
		bool NewWaitRqstRT = (Data & (1 << 5)) != 0;
		if (s_scc.a[chan].WaitRqstRT != NewWaitRqstRT) {
			s_scc.a[chan].WaitRqstRT = NewWaitRqstRT;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Wait/DMA request on receive/transmit",
				NewWaitRqstRT);
#endif
			/* happens in MacCheck */
		}
	}
#endif

#if SCC_TrackMore
	{
		bool NewWaitRqstSlct = (Data & (1 << 6)) != 0;
		if (s_scc.a[chan].WaitRqstSlct != NewWaitRqstSlct) {
			s_scc.a[chan].WaitRqstSlct = NewWaitRqstSlct;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Wait/DMA request function", NewWaitRqstSlct);
#endif
			/* happens in MacCheck */
		}
	}
#endif

#if SCC_TrackMore
	{
		bool NewWaitRqstEnbl = (Data & (1 << 7)) != 0;
		if (s_scc.a[chan].WaitRqstEnbl != NewWaitRqstEnbl) {
			s_scc.a[chan].WaitRqstEnbl = NewWaitRqstEnbl;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Wait/DMA request enable", NewWaitRqstEnbl);
#endif
			/* happens in MacCheck */
		}
	}
#endif
}

static void SCC_PutWR2(uint8_t Data, int chan)
/* "Interrupt Vector (accessed through either channel)" */
{
	/*
		Only 1 interrupt vector for the SCC, which
		is stored in channels A and B. B is modified
		when read.
	*/

	/* happens on boot always */

#if ! SCC_dolog
	UNUSED(chan);
#endif

	if (s_scc.InterruptVector != Data) {
#if SCC_dolog
		SCC_DbgLogChanStartLine(chan);
		dbglog_writeCStr(" InterruptVector <- ");
		dbglog_writeHex(Data);
		dbglog_writeReturn();
#endif
		s_scc.InterruptVector = Data;
	}
	if ((Data & (1 << 0)) != 0) { /* interrupt vector 0 */
		ReportAbnormalID(AbnormalID::kSCC_interrupt_vector_0, "interrupt vector 0");
	}
	if ((Data & (1 << 1)) != 0) { /* interrupt vector 1 */
		ReportAbnormalID(AbnormalID::kSCC_interrupt_vector_1, "interrupt vector 1");
	}
	if ((Data & (1 << 2)) != 0) { /* interrupt vector 2 */
		ReportAbnormalID(AbnormalID::kSCC_interrupt_vector_2, "interrupt vector 2");
	}
	if ((Data & (1 << 3)) != 0) { /* interrupt vector 3 */
		ReportAbnormalID(AbnormalID::kSCC_interrupt_vector_3, "interrupt vector 3");
	}
	if ((Data & (1 << 4)) != 0) { /* interrupt vector 4 */
		/* happens on boot with appletalk on */
	}
	if ((Data & (1 << 5)) != 0) { /* interrupt vector 5 */
		/* happens on boot with appletalk on */
	}
	if ((Data & (1 << 6)) != 0) { /* interrupt vector 6 */
		ReportAbnormalID(AbnormalID::kSCC_interrupt_vector_6, "interrupt vector 6");
	}
	if ((Data & (1 << 7)) != 0) { /* interrupt vector 7 */
		ReportAbnormalID(AbnormalID::kSCC_interrupt_vector_7, "interrupt vector 7");
	}
}

static void SCC_PutWR3(uint8_t Data, int chan)
/* "Receive parameters and control" */
{
#if SCC_TrackMore
	{
		uint8_t NewRBitsPerChar = (Data >> 6) & 3;
		if (s_scc.a[chan].RBitsPerChar != NewRBitsPerChar) {
			s_scc.a[chan].RBitsPerChar = NewRBitsPerChar;

			switch (NewRBitsPerChar) {
				case 0:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Rx Bits/Character <- 5");
#endif
					break;
				case 1:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Rx Bits/Character <- 7");
#endif
					break;
				case 2:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Rx Bits/Character <- 6");
#endif
					break;
				case 3:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Rx Bits/Character <- 8");
#endif
					break;
			}
		}
	}
#endif

	if ((Data & (1 << 5)) != 0) { /* Auto Enables */
		/*
			use DCD input as receiver enable,
			and set RTS output when transmit buffer empty
		*/
		ReportAbnormalID(AbnormalID::kSCC_Auto_Enables, "Auto Enables");
	}

	if ((Data & (1 << 4)) != 0) { /* Enter Hunt Mode */
#if SCC_dolog
		SCC_DbgLogChanCmnd(chan, "Enter Hunt Mode");
#endif
		/* happens on boot with appletalk on */
		if (! (s_scc.a[chan].SyncHunt)) {
			s_scc.a[chan].SyncHunt = true;

		}
	}

#if SCC_TrackMore
	{
		bool NewRxCRCEnbl = (Data & (1 << 3)) != 0;
		if (s_scc.a[chan].RxCRCEnbl != NewRxCRCEnbl) {
			s_scc.a[chan].RxCRCEnbl = NewRxCRCEnbl;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Rx CRC Enable", NewRxCRCEnbl);
#endif
			/* happens on boot with appletalk on */
		}
	}
#endif

#if EmLocalTalk || SCC_TrackMore
	{
		bool NewAddrSrchMd = (Data & (1 << 2)) != 0;
		if (s_scc.a[chan].AddrSrchMd != NewAddrSrchMd) {
			s_scc.a[chan].AddrSrchMd = NewAddrSrchMd;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Addr Search Mode (SDLC)", NewAddrSrchMd);
#endif
			/* happens on boot with appletalk on */
			if (0 != chan) {
				LT_AddrSrchMdSet(NewAddrSrchMd);
			}
		}
	}
#endif

#if SCC_TrackMore
	{
		bool NewSyncChrLdInhb = (Data & (1 << 1)) != 0;
		if (s_scc.a[chan].SyncChrLdInhb != NewSyncChrLdInhb) {
			s_scc.a[chan].SyncChrLdInhb = NewSyncChrLdInhb;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Sync Char Load Inhibit", NewSyncChrLdInhb);
#endif
			/* happens on boot with appletalk on */
		}
	}
#endif

	{
		bool NewRxEnable = (Data & (1 << 0)) != 0;
		if (s_scc.a[chan].RxEnable != NewRxEnable) {
			s_scc.a[chan].RxEnable = NewRxEnable;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Rx Enable", NewRxEnable);
#endif
			/* true on boot with appletalk on */
			/* true on Print to ImageWriter */

#if EmLocalTalk
			if (! NewRxEnable) {
#if SCC_dolog
				if ((0 != chan) && (nullptr != g_ltRxBuffer)) {
					dbglog_WriteNote("SCC abandon packet");
				}
#endif

				/*
					Go back into the idle state if we were
					waiting for EOF
				*/
				s_scc.a[chan].EndOfFrame = false;
				s_scc.a[chan].RxChrAvail = false;
				s_scc.a[chan].SyncHunt = true;
			} else {
				/* look for a packet */
				if (0 != chan) {
					g_machine->findDevice<SCCDevice>()->localTalkTick();
				}
			}
#endif
		}
	}
}

static void SCC_PutWR4(uint8_t Data, int chan)
/* "Transmit/Receive miscellaneous parameters and modes" */
{
#if ! (EmLocalTalk || SCC_TrackMore)
	UNUSED(Data);
	UNUSED(chan);
#endif

#if SCC_TrackMore
	{
		bool NewPrtyEnable = (Data & (1 << 0)) != 0;
		if (s_scc.a[chan].PrtyEnable != NewPrtyEnable) {
			s_scc.a[chan].PrtyEnable = NewPrtyEnable;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Parity Enable", NewPrtyEnable);
#endif
		}
	}
#endif

#if SCC_TrackMore
	{
		bool NewPrtyEven = (Data & (1 << 1)) != 0;
		if (s_scc.a[chan].PrtyEven != NewPrtyEven) {
			s_scc.a[chan].PrtyEven = NewPrtyEven;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Parity Enable", NewPrtyEven);
#endif
		}
	}
#endif

#if SCC_TrackMore
	{
		uint8_t NewStopBits = (Data >> 2) & 3;
		if (s_scc.a[chan].StopBits != NewStopBits) {
			s_scc.a[chan].StopBits = NewStopBits;

			/* SCC_SetStopBits(chan, NewStopBits); */
			switch (NewStopBits) {
				case 0:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Sync Modes Enable");
#endif
					break;
				case 1:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan, "1 Stop Bit");
#endif
					break;
				case 2:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan, "1 1/2 Stop Bits");
#endif
					break;
				case 3:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan, "2 Stop Bits");
#endif
					break;
			}
		}
	}
#endif

#if EmLocalTalk || SCC_TrackMore
	{
		uint8_t NewSyncMode = (Data >> 4) & 3;
		if (s_scc.a[chan].SyncMode != NewSyncMode) {
			s_scc.a[chan].SyncMode = NewSyncMode;

			switch (NewSyncMode) {
				case 0:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan, "8 bit sync char");
#endif
					/* happens on boot always */
					break;
				case 1:
					ReportAbnormalID(AbnormalID::kSCC_16_bit_sync_char, "16 bit sync char");
					break;
				case 2:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan, "SDLC MODE");
#endif
					/* happens on boot with appletalk on */
#if EmLocalTalk
					s_scc.a[chan].TxBufferEmpty = true;
#endif
					break;
				case 3:
					ReportAbnormalID(AbnormalID::kSCC_External_sync_mode, "External sync mode");
					break;
			}
		}
	}
#endif

#if SCC_TrackMore
	{
		uint8_t NewClockRate = (Data >> 6) & 3;
		if (s_scc.a[chan].ClockRate != NewClockRate) {
			s_scc.a[chan].ClockRate = NewClockRate;

			switch (NewClockRate) {
				case 0:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Clock Rate <- X1");
#endif
					/* happens on boot with appletalk on */
					break;
				case 1:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Clock Rate <- X16");
#endif
					/* happens on boot always */
					break;
				case 2:
					ReportAbnormalID(AbnormalID::kSCC_Clock_Rate_X32, "Clock Rate <- X32");
					break;
				case 3:
					ReportAbnormalID(AbnormalID::kSCC_Clock_Rate_X64, "Clock Rate <- X64");
					break;
			}
		}
	}
#endif
}

static void SCC_PutWR5(uint8_t Data, int chan)
/* "Transmit parameters and controls" */
{
	/* happens on boot with appletalk on */
	/* happens in Print to ImageWriter */

#if SCC_TrackMore
	{
		bool NewTxCRCEnbl = (Data & (1 << 0)) != 0;
		if (s_scc.a[chan].TxCRCEnbl != NewTxCRCEnbl) {
			s_scc.a[chan].TxCRCEnbl = NewTxCRCEnbl;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Tx CRC Enable", NewTxCRCEnbl);
#endif
			/* both values on boot with appletalk on */
		}
	}
#endif

#if SCC_TrackMore
	{
		bool NewRTSctrl = (Data & (1 << 1)) != 0;
		if (s_scc.a[chan].RTSctrl != NewRTSctrl) {
			s_scc.a[chan].RTSctrl = NewRTSctrl;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"RTS Control", NewRTSctrl);
#endif
			/* both values on boot with appletalk on */
			/*
				value of Request To Send output pin, when
				Auto Enable is off
			*/
		}
	}
#endif

	if ((Data & (1 << 2)) != 0) { /* SDLC/CRC-16 */
		ReportAbnormalID(AbnormalID::kSCC_SDLC_CRC_16, "SDLC/CRC-16");
	}

	{
		bool NewTxEnable = (Data & (1 << 3)) != 0;
		if (s_scc.a[chan].TxEnable != NewTxEnable) {
			s_scc.a[chan].TxEnable = NewTxEnable;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Tx Enable", NewTxEnable);
#endif

			if (NewTxEnable) {
				/* happens on boot with appletalk on */
				/* happens in Print to ImageWriter */
#if EmLocalTalk
				if (0 != chan) {
					g_ltTxBuffSz = 0;
				}
#endif
			} else {
#if EmLocalTalk
				s_scc.a[chan].TxBufferEmpty = true;
#endif
			}
		}
	}

#if SCC_TrackMore
	{
		bool NewSndBrkCtrl = (Data & (1 << 4)) != 0;
		if (s_scc.a[chan].SndBrkCtrl != NewSndBrkCtrl) {
			s_scc.a[chan].SndBrkCtrl = NewSndBrkCtrl;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Send Break Control", NewSndBrkCtrl);
#endif
			/* true in Print to LaserWriter 300 */
		}
	}
#endif

#if SCC_TrackMore
	{
		uint8_t NewTBitsPerChar = (Data >> 5) & 3;
		if (s_scc.a[chan].TBitsPerChar != NewTBitsPerChar) {
			s_scc.a[chan].TBitsPerChar = NewTBitsPerChar;

			switch (NewTBitsPerChar) {
				case 0:
					ReportAbnormalID(AbnormalID::kSCC_Tx_Bits_Character_5, "Tx Bits/Character <- 5");
					break;
				case 1:
					ReportAbnormalID(AbnormalID::kSCC_Tx_Bits_Character_7, "Tx Bits/Character <- 7");
					break;
				case 2:
					ReportAbnormalID(AbnormalID::kSCC_Tx_Bits_Character_6, "Tx Bits/Character <- 6");
					break;
				case 3:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Tx Bits/Character <- 8");
#endif
					/* happens on boot with appletalk on */
					/* happens in Print to ImageWriter */
					break;
			}
		}
	}
#endif

#if SCC_TrackMore
	{
		bool NewDTRctrl = (Data & (1 << 7)) != 0;
		if (s_scc.a[chan].DTRctrl != NewDTRctrl) {
			s_scc.a[chan].DTRctrl = NewDTRctrl;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"Data Terminal Ready Control", NewDTRctrl);
#endif
			/* zero happens in MacCheck */
			/*
				value of Data Terminal Ready output pin,
				when WR14 D2 = 0 (DTR/request function)
			*/
		}
	}
#endif
}

static void SCC_PutWR6(uint8_t Data, int chan)
/* "Sync characters or SDLC address field" */
{
	/* happens on boot with appletalk on */

#if ! (EmLocalTalk || SCC_dolog)
	UNUSED(Data);
#endif
#if ! SCC_dolog
	UNUSED(chan);
#endif

#if SCC_dolog
	SCC_DbgLogChanStartLine(chan);
	dbglog_writeCStr(" Sync Char <- ");
	dbglog_writeHex(Data);
	dbglog_writeReturn();
#endif

#if EmLocalTalk
	if (0 != chan) {
		LT_NodeAddressSet(Data);
	}
#endif
}

static void SCC_PutWR7(uint8_t Data, int chan)
/* "Sync character or SDLC flag" */
{
	/* happens on boot with appletalk on */

#if ! SCC_TrackMore
	UNUSED(Data);
	UNUSED(chan);
#endif

#if SCC_TrackMore
	if (2 == s_scc.a[chan].SyncMode) {
		if (0x7E != Data) {
			ReportAbnormalID(AbnormalID::kSCC_unexpect_flag_character_for_SDLC,
				"unexpect flag character for SDLC");
		}
	} else {
		ReportAbnormalID(AbnormalID::kSCC_WR7_and_not_SDLC, "WR7 and not SDLC");
	}
#endif
}

static void SCC_PutWR8(uint8_t Data, int chan)
/* "Transmit Buffer" */
{
	/* happens on boot with appletalk on */
	/* happens in Print to ImageWriter */

#if ! (EmLocalTalk || SCC_dolog)
	/* Data always consumed when a serial backend is attached,
	   but suppress the warning for the no-backend-no-LocalTalk path. */
	(void)Data;
#endif

#if SCC_dolog
	SCC_DbgLogChanStartLine(chan);
	dbglog_writeCStr(" Transmit Buffer");
	dbglog_writeCStr(" <- ");
	dbglog_writeHex(Data);
	dbglog_writeCStr(" '");
	dbglog_writeMacChar(Data);
	dbglog_writeCStr("'");
	dbglog_writeReturn();
#endif

	if (s_scc.a[chan].TxEnable) { /* Tx Enable */
		/* Output (Data) to Modem(B) or Printer(A) Port */

		if (s_serialBackend[chan]) {
			s_serialBackend[chan]->txByte(Data);
		} else
		{
			/* happens on boot with appletalk on */
#if EmLocalTalk
			if (0 != chan) {
				SCC_TxBuffPut(Data);
			}
#else
			s_scc.a[chan].TxUnderrun = true; /* underrun ? */
#endif
		}

		s_scc.a[chan].TxIP = true;
		CheckSCCInterruptFlag();
	} else {
		ReportAbnormalID(AbnormalID::kSCC_write_when_Transmit_Buffer_not_Enabled,
			"write when Transmit Buffer not Enabled");
	}
}

static void SCC_PutWR9(uint8_t Data, int chan)
/*
	"Master interrupt control and reset
	(accessed through either channel)"
*/
{
	/* Only 1 WR9 in the SCC */

	UNUSED(chan);

	if ((Data & (1 << 0)) != 0) { /* VIS */
		ReportAbnormalID(AbnormalID::kSCC_VIS, "VIS");
	}

#if SCC_TrackMore
	{
		bool NewNoVectorSlct = (Data & (1 << 1)) != 0;
		if (s_scc.NoVectorSlct != NewNoVectorSlct) {
			s_scc.NoVectorSlct = NewNoVectorSlct;
#if SCC_dolog
			dbglog_WriteSetBool("SCC No Vector select",
				NewNoVectorSlct);
#endif
			/* has both values on boot always */
		}
	}
#endif

	if ((Data & (1 << 2)) != 0) { /* DLC */
		ReportAbnormalID(AbnormalID::kSCC_DLC, "DLC");
	}

	{
		bool NewMIE = (Data & (1 << 3)) != 0;
			/* has both values on boot always */
		if (s_scc.MIE != NewMIE) {
			s_scc.MIE = NewMIE;
#if SCC_dolog
			dbglog_WriteSetBool("SCC Master Interrupt Enable",
				NewMIE);
#endif
			CheckSCCInterruptFlag();
		}
	}

	if ((Data & (1 << 4)) != 0) { /* Status high/low */
		ReportAbnormalID(AbnormalID::kSCC_Status_high_low, "Status high/low");
	}
	if ((Data & (1 << 5)) != 0) { /* WR9 b5 should be 0 */
		ReportAbnormalID(AbnormalID::kSCC_WR9_b5_should_be_0, "WR9 b5 should be 0");
	}

	switch ((Data >> 6) & 3) {
		case 1:
#if SCC_dolog
			SCC_DbgLogChanCmnd(1, "Channel Reset");
#endif
			/* happens on boot always */
			SCC_ResetChannel(1);
			CheckSCCInterruptFlag();
			break;
		case 2:
#if SCC_dolog
			SCC_DbgLogChanCmnd(0, "Channel Reset");
#endif
			/* happens on boot always */
			SCC_ResetChannel(0);
			CheckSCCInterruptFlag();
			break;
		case 3:
#if SCC_dolog
			dbglog_WriteNote("SCC Force Hardware Reset");
#endif
			if (!g_machine->config().isSEOrLater()) {
				ReportAbnormalID(AbnormalID::kSCC_SCC_Reset, "SCC_Reset");
			}
			g_machine->findDevice<SCCDevice>()->reset();
			CheckSCCInterruptFlag();
			break;
		case 0: /* No Reset */
		default:
			break;
	}
}

static void SCC_PutWR10(uint8_t Data, int chan)
/* "Miscellaneous transmitter/receiver control bits" */
{
	/* happens on boot with appletalk on */
	/* happens in Print to ImageWriter */

#if ! SCC_TrackMore
	UNUSED(chan);
#endif

	if ((Data & (1 << 0)) != 0) { /* 6 bit/8 bit sync */
		ReportAbnormalID(AbnormalID::kSCC_6_bit_8_bit_sync, "6 bit/8 bit sync");
	}
	if ((Data & (1 << 1)) != 0) { /* loop mode */
		ReportAbnormalID(AbnormalID::kSCC_loop_mode, "loop mode");
	}
	if ((Data & (1 << 2)) != 0) { /* abort/flag on underrun */
		ReportAbnormalID(AbnormalID::kSCC_abort_flag_on_underrun, "abort/flag on underrun");
	}
	if ((Data & (1 << 3)) != 0) { /* mark/flag idle */
		ReportAbnormalID(AbnormalID::kSCC_mark_flag_idle, "mark/flag idle");
	}
	if ((Data & (1 << 4)) != 0) { /* go active on poll */
		ReportAbnormalID(AbnormalID::kSCC_go_active_on_poll, "go active on poll");
	}

#if SCC_TrackMore
	{
		uint8_t NewDataEncoding = (Data >> 5) & 3;
		if (s_scc.a[chan].DataEncoding != NewDataEncoding) {
			s_scc.a[chan].DataEncoding = NewDataEncoding;

			switch (NewDataEncoding) {
				case 0:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Data Encoding <- NRZ");
#endif
					/* happens in MacCheck */
					/* happens in Print to ImageWriter */
					break;
				case 1:
					ReportAbnormalID(AbnormalID::kSCC_Data_Encoding_NRZI, "Data Encoding <- NRZI");
					break;
				case 2:
					ReportAbnormalID(AbnormalID::kSCC_Data_Encoding_FM1, "Data Encoding <- FM1");
					break;
				case 3:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"Data Encoding <- FM0");
#endif
					/* happens on boot with appletalk on */
					break;
			}
		}
	}
#endif

#if SCC_TrackMore
	{
		bool NewCRCPreset = (Data & (1 << 7)) != 0;
		if (s_scc.a[chan].CRCPreset != NewCRCPreset) {
			s_scc.a[chan].CRCPreset = NewCRCPreset;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"CRC preset I/O", NewCRCPreset);
#endif
			/* false happens in MacCheck */
			/* true happens in Print to ImageWriter */
		}
	}
#endif
}

static void SCC_PutWR11(uint8_t Data, int chan)
/* "Clock mode control" */
{
	/* happens on boot with appletalk on */
	/* happens in Print to ImageWriter */
	/* happens in MacCheck */

#if ! SCC_TrackMore
	UNUSED(chan);
#endif

#if SCC_TrackMore
	/* Transmit External Control Selection */
	{
		uint8_t NewTRxCsrc = Data & 3;
		if (s_scc.a[chan].TRxCsrc != NewTRxCsrc) {
			s_scc.a[chan].TRxCsrc = NewTRxCsrc;

			switch (NewTRxCsrc) {
				case 0:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"TRxC OUT = XTAL output");
#endif
					/* happens on boot with appletalk on */
					/* happens in Print to ImageWriter */
					/* happens in MacCheck */
					break;
				case 1:
					ReportAbnormalID(AbnormalID::kSCC_TRxC_OUT_transmit_clock,
						"TRxC OUT = transmit clock");
					break;
				case 2:
					ReportAbnormalID(AbnormalID::kSCC_TRxC_OUT_BR_generator_output,
						"TRxC OUT = BR generator output");
					break;
				case 3:
					ReportAbnormalID(AbnormalID::kSCC_TRxC_OUT_dpll_output, "TRxC OUT = dpll output");
					break;
			}
		}
	}
#endif

	if ((Data & (1 << 2)) != 0) {
		ReportAbnormalID(AbnormalID::kSCC_TRxC_O_I, "TRxC O/I");
	}

#if SCC_TrackMore
	{
		uint8_t NewTClkSlct = (Data >> 3) & 3;
		if (s_scc.a[chan].TClkSlct != NewTClkSlct) {
			s_scc.a[chan].TClkSlct = NewTClkSlct;

			switch (NewTClkSlct) {
				case 0:
					ReportAbnormalID(AbnormalID::kSCC_transmit_clock_RTxC_pin,
						"transmit clock = RTxC pin");
					break;
				case 1:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"transmit clock = TRxC pin");
#endif
					/* happens in Print to LaserWriter 300 */
					break;
				case 2:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"transmit clock = BR generator output");
#endif
					/* happens on boot with appletalk on */
					/* happens in Print to ImageWriter */
					/* happens in MacCheck */
					break;
				case 3:
					ReportAbnormalID(AbnormalID::kSCC_transmit_clock_dpll_output,
						"transmit clock = dpll output");
					break;
			}
		}
	}
#endif

#if SCC_TrackMore
	{
		uint8_t NewRClkSlct = (Data >> 5) & 3;
		if (s_scc.a[chan].RClkSlct != NewRClkSlct) {
			s_scc.a[chan].RClkSlct = NewRClkSlct;

			switch (NewRClkSlct) {
				case 0:
					ReportAbnormalID(AbnormalID::kSCC_receive_clock_RTxC_pin,
						"receive clock = RTxC pin");
					break;
				case 1:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"receive clock = TRxC pin");
#endif
					/* happens in Print to LaserWriter 300 */
					break;
				case 2:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"receive clock = BR generator output");
#endif
					/* happens in MacCheck */
					/* happens in Print to ImageWriter */
					break;
				case 3:
#if SCC_dolog
					SCC_DbgLogChanCmnd(chan,
						"receive clock = dpll output");
#endif
					/* happens on boot with appletalk on */
					break;
			}
		}
	}
#endif

	if ((Data & (1 << 7)) != 0) {
		ReportAbnormalID(AbnormalID::kSCC_RTxC_XTAL_NO_XTAL, "RTxC XTAL/NO XTAL");
	}
}

static void SCC_PutWR12(uint8_t Data, int chan)
/* "Lower byte of baud rate generator time constant" */
{
	/* happens on boot with appletalk on */
	/* happens in Print to ImageWriter */

#if ! SCC_TrackMore
	UNUSED(Data);
	UNUSED(chan);
#endif

#if SCC_TrackMore /* don't care about Baud */
	if (s_scc.a[chan].BaudLo != Data) {
		s_scc.a[chan].BaudLo = Data;

#if SCC_dolog
		SCC_DbgLogChanStartLine(chan);
		dbglog_writeCStr(" BaudLo <- ");
		dbglog_writeHex(Data);
		dbglog_writeReturn();
#endif
	}
#endif

}

static void SCC_PutWR13(uint8_t Data, int chan)
/* "Upper byte of baud rate generator time constant" */
{
	/* happens on boot with appletalk on */
	/* happens in Print to ImageWriter */

#if ! SCC_TrackMore
	UNUSED(Data);
	UNUSED(chan);
#endif

#if SCC_TrackMore /* don't care about Baud */
	if (s_scc.a[chan].BaudHi != Data) {
		s_scc.a[chan].BaudHi = Data;

#if SCC_dolog
		SCC_DbgLogChanStartLine(chan);
		dbglog_writeCStr(" BaudHi <- ");
		dbglog_writeHex(Data);
		dbglog_writeReturn();
#endif
	}
#endif

}

static void SCC_PutWR14(uint8_t Data, int chan)
/* "Miscellaneous control bits" */
{
	/* happens on boot with appletalk on */

#if ! (SCC_TrackMore || SCC_dolog)
	UNUSED(chan);
#endif

#if SCC_TrackMore
	{
		bool NewBRGEnbl = (Data & (1 << 0)) != 0;
		if (s_scc.a[chan].BRGEnbl != NewBRGEnbl) {
			s_scc.a[chan].BRGEnbl = NewBRGEnbl;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"BR generator enable", NewBRGEnbl);
#endif
			/* both values on boot with appletalk on */
			/* true happens in Print to ImageWriter */
		}
	}
#endif

	if ((Data & (1 << 1)) != 0) { /* BR generator source */
		ReportAbnormalID(AbnormalID::kSCC_BR_generator_source, "BR generator source");
	}
	if ((Data & (1 << 2)) != 0) { /* DTR/request function */
		ReportAbnormalID(AbnormalID::kSCC_DTR_request_function, "DTR/request function");
	}
	if ((Data & (1 << 3)) != 0) { /* auto echo */
		ReportAbnormalID(AbnormalID::kSCC_auto_echo, "auto echo");
	}
	if ((Data & (1 << 4)) != 0) { /* local loopback */
		ReportAbnormalID(AbnormalID::kSCC_local_loopback, "local loopback");
	}

	switch ((Data >> 5) & 7) {
		case 1:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "enter search mode");
#endif
			/* happens on boot with appletalk on */
			break;
		case 2:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "reset missing clock");
#endif
			/* happens on boot with appletalk on */
			/*
				should clear Bit 6 and Bit 7 of RR[10], but
				since these are never set, don't need
				to do anything
			*/
			break;
		case 3:
			ReportAbnormalID(AbnormalID::kSCC_disable_dpll, "disable dpll");
			/*
				should clear Bit 6 and Bit 7 of RR[10], but
				since these are never set, don't need
				to do anything
			*/
			break;
		case 4:
			ReportAbnormalID(AbnormalID::kSCC_set_source_br_generator, "set source = br generator");
			break;
		case 5:
			ReportAbnormalID(AbnormalID::kSCC_set_source_RTxC, "set source = RTxC");
			break;
		case 6:
#if SCC_dolog
			SCC_DbgLogChanCmnd(chan, "set FM mode");
#endif
			/* happens on boot with appletalk on */
			break;
		case 7:
			ReportAbnormalID(AbnormalID::kSCC_set_NRZI_mode, "set NRZI mode");
			break;
		case 0: /* No Reset */
		default:
			break;
	}
}

static void SCC_PutWR15(uint8_t Data, int chan)
/* "External/Status interrupt control" */
{
	/* happens on boot always */

#if ! SCC_TrackMore
	UNUSED(chan);
#endif

	if ((Data & (1 << 0)) != 0) { /* WR15 b0 should be 0 */
		ReportAbnormalID(AbnormalID::kSCC_WR15_b0_should_be_0, "WR15 b0 should be 0");
	}
	if ((Data & (1 << 1)) != 0) { /* zero count IE */
		ReportAbnormalID(AbnormalID::kSCC_zero_count_IE, "zero count IE");
	}
	if ((Data & (1 << 2)) != 0) { /* WR15 b2 should be 0 */
		ReportAbnormalID(AbnormalID::kSCC_WR15_b2_should_be_0, "WR15 b2 should be 0");
	}

	if ((Data & (1 << 3)) == 0) { /* DCD_IE */
		if (!g_machine->config().isSEOrLater()) {
			ReportAbnormalID(AbnormalID::kSCC_not_DCD_IE, "not DCD IE");
		}
	}

	if ((Data & (1 << 4)) != 0) {
		/* SYNC/HUNT IE */
		ReportAbnormalID(AbnormalID::kSCC_SYNC_HUNT_IE, "SYNC/HUNT IE");
	}

#if SCC_TrackMore /* don't care about CTS_IE */
	{
		bool NewCTS_IE = (Data & (1 << 5)) != 0;
		if (s_scc.a[chan].CTS_IE != NewCTS_IE) {
			s_scc.a[chan].CTS_IE = NewCTS_IE;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"CTS IE", NewCTS_IE);
#endif
			/* happens in MacCheck */
			/* happens in Print to ImageWriter */
		}
	}
#endif

	if ((Data & (1 << 6)) != 0) { /* Tx underrun/EOM IE */
		ReportAbnormalID(AbnormalID::kSCC_Tx_underrun_EOM_IE, "Tx underrun/EOM IE");
	}

#if SCC_TrackMore
	{
		bool NewBreakAbortIE = (Data & (1 << 7)) != 0;
		if (s_scc.a[chan].BreakAbortIE != NewBreakAbortIE) {
			s_scc.a[chan].BreakAbortIE = NewBreakAbortIE;
#if SCC_dolog
			SCC_DbgLogChanChngBit(chan,
				"BreakAbort IE", NewBreakAbortIE);
#endif
			/* happens in MacCheck */
			/* happens in Print to ImageWriter */
		}
	}
#endif
}

static uint8_t SCC_GetReg(int chan, uint8_t SCC_Reg)
{
	uint8_t value;

	switch (SCC_Reg) {
		case 0:
			value = SCC_GetRR0(chan);
			break;
		case 1:
			value = SCC_GetRR1(chan);
			break;
		case 2:
			value = SCC_GetRR2(chan);
			break;
		case 3:
			value = SCC_GetRR3(chan);
			break;
		case 4:
			ReportAbnormalID(AbnormalID::kSCC_RR_4, "RR 4"); /* same as RR0 */
			value = SCC_GetRR0(chan);
			break;
		case 5:
			ReportAbnormalID(AbnormalID::kSCC_RR_5, "RR 5"); /* same as RR1 */
			value = SCC_GetRR1(chan);
			break;
		case 6:
			ReportAbnormalID(AbnormalID::kSCC_RR_6, "RR 6"); /* same as RR2 */
			value = SCC_GetRR2(chan);
			break;
		case 7:
			ReportAbnormalID(AbnormalID::kSCC_RR_7, "RR 7"); /* same as RR3 */
			value = SCC_GetRR3(chan);
			break;
		case 8:
			value = SCC_GetRR8(chan);
			break;
		case 9:
			ReportAbnormalID(AbnormalID::kSCC_RR_9, "RR 9"); /* same as RR13 */
			value = SCC_GetRR13(chan);
			break;
		case 10:
			value = SCC_GetRR10(chan);
			break;
		case 11:
			ReportAbnormalID(AbnormalID::kSCC_RR_11, "RR 11"); /* same as RR15 */
			value = SCC_GetRR15(chan);
			break;
		case 12:
			value = SCC_GetRR12(chan);
			break;
		case 13:
			value = SCC_GetRR13(chan);
			break;
		case 14:
			ReportAbnormalID(AbnormalID::kSCC_RR_14, "RR 14");
			value = 0;
			break;
		case 15:
			value = SCC_GetRR15(chan);
			break;
		default:
			ReportAbnormalID(AbnormalID::kSCC_unexpected_SCC_Reg_in_SCC_GetReg,
				"unexpected SCC_Reg in SCC_GetReg");
			value = 0;
			break;
	}

#if EmLocalTalk
	/*
		Always check to see if interrupt state changed after
		ANY register access
	*/
	CheckSCCInterruptFlag();
#endif

#if SCC_dolog
	SCC_DbgLogChanStartLine(chan);
	dbglog_writeCStr(" RR[");
	dbglog_writeHex(SCC_Reg);
	dbglog_writeCStr("] -> ");
	dbglog_writeHex(value);
	dbglog_writeReturn();
#endif

	return value;
}

static void SCC_PutReg(uint8_t Data, int chan, uint8_t SCC_Reg)
{
#if SCC_dolog && 0
	SCC_DbgLogChanStartLine(chan);
	dbglog_writeCStr(" WR[");
	dbglog_writeHex(SCC_Reg);
	dbglog_writeCStr("] <- ");
	dbglog_writeHex(Data);
	dbglog_writeReturn();
#endif

	switch (SCC_Reg) {
		case 0:
			SCC_PutWR0(Data, chan);
			break;
		case 1:
			SCC_PutWR1(Data, chan);
			break;
		case 2:
			SCC_PutWR2(Data, chan);
			break;
		case 3:
			SCC_PutWR3(Data, chan);
			break;
		case 4:
			SCC_PutWR4(Data, chan);
			break;
		case 5:
			SCC_PutWR5(Data, chan);
			break;
		case 6:
			SCC_PutWR6(Data, chan);
			break;
		case 7:
			SCC_PutWR7(Data, chan);
			break;
		case 8:
			SCC_PutWR8(Data, chan);
			break;
		case 9:
			SCC_PutWR9(Data, chan);
			break;
		case 10:
			SCC_PutWR10(Data, chan);
			break;
		case 11:
			SCC_PutWR11(Data, chan);
			break;
		case 12:
			SCC_PutWR12(Data, chan);
			break;
		case 13:
			SCC_PutWR13(Data, chan);
			break;
		case 14:
			SCC_PutWR14(Data, chan);
			break;
		case 15:
			SCC_PutWR15(Data, chan);
			break;
		default:
			ReportAbnormalID(AbnormalID::kSCC_unexpected_SCC_Reg_in_SCC_PutReg,
				"unexpected SCC_Reg in SCC_PutReg");
			break;
	}

#if EmLocalTalk
	/*
		Always check to see if interrupt state changed after ANY
		register access
	*/
	CheckSCCInterruptFlag();
#endif
}

 uint32_t SCCDevice::access(uint32_t Data, bool WriteMem, uint32_t addr)
{
#if EmLocalTalk
	/*
		Determine channel, data, and access type from address.  The bus
		for the 8350 is non-standard, so the Macintosh connects address
		bus lines to various signals on the 8350 as shown below. The
		68K will use the upper byte of the data bus for odd addresses,
		and the 8350 is only wired to the upper byte, therefore use
		only odd addresses or you risk resetting the 8350.

		68k   8350
		----- ------
		a1    a/b
		a2    d/c
		a21   wr/rd
	*/
#endif
	uint8_t SCC_Reg;
	int chan = (~ addr) & 1; /* 0=modem, 1=printer */
	if (((addr >> 1) & 1) == 0) {
		/* Channel Control */
		SCC_Reg = s_scc.PointerBits;
		s_scc.PointerBits = 0;
	} else {
		/* Channel Data */
		SCC_Reg = 8;
	}
	if (WriteMem) {
		SCC_PutReg(Data, chan, SCC_Reg);
	} else {
		Data = SCC_GetReg(chan, SCC_Reg);
	}

	return Data;
}

/* --- Serial backend support --- */

void SCCDevice::setBackend(int chan, std::unique_ptr<SerialBackend> backend)
{
	if (chan < 0 || chan > 1) return;
	s_serialBackend[chan] = std::move(backend);
}

void SCCDevice::serialTick()
{
	for (int chan = 0; chan < 2; ++chan) {
		if (!s_serialBackend[chan]) continue;

		s_serialBackend[chan]->poll();

		/* Deliver one received byte if the channel is ready. */
		if (s_scc.a[chan].RxEnable
			&& !s_scc.a[chan].RxChrAvail
			&& s_serialBackend[chan]->rxReady())
		{
			s_scc.a[chan].RxBuff = s_serialBackend[chan]->rxByte();
			s_scc.a[chan].RxChrAvail = true;
			CheckSCCInterruptFlag();
		}
	}
}