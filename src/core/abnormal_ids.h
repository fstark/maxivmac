/*
	abnormal_ids.h

	Central registry of ReportAbnormalID codes.
	Auto-generated — do not edit by hand.
*/

#pragma once

#include <cstdint>

namespace AbnormalID
{

/* ADB */
inline constexpr uint16_t kADB_Reserved_ADB_command = 0x0C01;
inline constexpr uint16_t kADB_Reserved_ADB_command_2 = 0x0C02;
inline constexpr uint16_t kADB_ADB_listen_too_much = 0x0C03;
inline constexpr uint16_t kADB_ADB_idle_follows_listen = 0x0C04;
inline constexpr uint16_t kADB_idle_when_not_done_talking = 0x0C05;

/* ASC */
inline constexpr uint16_t kASC_full_flag_A_not_already_clear = 0x0F02;
inline constexpr uint16_t kASC_Channel_B_for_Mono = 0x0F03;
inline constexpr uint16_t kASC_Channel_B_Overflow = 0x0F04;
inline constexpr uint16_t kASC_full_flag_B_not_already_clear = 0x0F05;
inline constexpr uint16_t kASC_writing_VERSION = 0x0F06;
inline constexpr uint16_t kASC_unexpected_ENABLE = 0x0F07;
inline constexpr uint16_t kASC_unexpected_CONTROL_value = 0x0F09;
inline constexpr uint16_t kASC_reading_CONTROL_value = 0x0F0A;
inline constexpr uint16_t kASC_unexpected_FIFO_MODE = 0x0F0B;
inline constexpr uint16_t kASC_set_clear_FIFO_again = 0x0F0C;
inline constexpr uint16_t kASC_reading_WAVE_CONTROL_register = 0x0F11;
inline constexpr uint16_t kASC_unexpected_volume_value = 0x0F12;
inline constexpr uint16_t kASC_reading_volume_register = 0x0F13;
inline constexpr uint16_t kASC_nonstandard_CLOCK_RATE = 0x0F14;
inline constexpr uint16_t kASC_reading_CLOCK_RATE = 0x0F15;
inline constexpr uint16_t kASC_write_to_808 = 0x0F16;
inline constexpr uint16_t kASC_write_to_80A = 0x0F17;
inline constexpr uint16_t kASC_unknown_ASC_reg = 0x0F18;
inline constexpr uint16_t kASC_unknown_ASC_reg_2 = 0x0F19;
inline constexpr uint16_t kASC_half_flag_A_not_already_clear = 0x0F1A;
inline constexpr uint16_t kASC_full_flag_A_not_already_set = 0x0F1B;
inline constexpr uint16_t kASC_half_flag_B_not_already_clear = 0x0F1C;
inline constexpr uint16_t kASC_full_flag_B_not_already_set = 0x0F1D;

/* CPU */
inline constexpr uint16_t kCPU_Extension_Word_dp_reserved = 0x0101;
inline constexpr uint16_t kCPU_Extension_Word_reserved_dp_form = 0x0102;
inline constexpr uint16_t kCPU_not_kLazyFlagsDefault_in_NeedDefaultLazy = 0x0103;
inline constexpr uint16_t kCPU_not_kLazyFlagsDefault_in_NeedDefaultLazy_2 = 0x0104;
inline constexpr uint16_t kCPU_t0_flag_set_in_m68k_setSR = 0x0105;
inline constexpr uint16_t kCPU_m_flag_set_in_m68k_setSR = 0x0106;
inline constexpr uint16_t kCPU_rte_stack_frame_format_1 = 0x0107;
inline constexpr uint16_t kCPU_rte_stack_frame_format_2 = 0x0108;
inline constexpr uint16_t kCPU_rte_stack_frame_format_9 = 0x0109;
inline constexpr uint16_t kCPU_rte_stack_frame_format_10 = 0x010A;
inline constexpr uint16_t kCPU_rte_stack_frame_format_11 = 0x010B;
inline constexpr uint16_t kCPU_unknown_rte_stack_frame_format = 0x010C;
inline constexpr uint16_t kCPU_CALLM_or_RTM_instruction = 0x010D;
inline constexpr uint16_t kCPU_illegal_opsize_in_CHK2_or_CMP2 = 0x010E;
inline constexpr uint16_t kCPU_CAS_instruction = 0x010F;
inline constexpr uint16_t kCPU_illegal_opsize_in_DoCAS = 0x0110;
inline constexpr uint16_t kCPU_DoCAS2_instruction = 0x0111;
inline constexpr uint16_t kCPU_MoveS_instruction = 0x0112;
inline constexpr uint16_t kCPU_DoMoveToControl_usp = 0x0113;
inline constexpr uint16_t kCPU_DoMoveToControl_isp = 0x0114;
inline constexpr uint16_t kCPU_DoMoveToControl_unknown_reg = 0x0115;
inline constexpr uint16_t kCPU_DoMoveFromControl_usp = 0x0116;
inline constexpr uint16_t kCPU_DoMoveFromControl_isp = 0x0117;
inline constexpr uint16_t kCPU_DoMoveFromControl_unknown_reg = 0x0118;
inline constexpr uint16_t kCPU_BKPT_instruction = 0x0119;
inline constexpr uint16_t kCPU_Link_L = 0x011A;
inline constexpr uint16_t kCPU_TRAPcc_trapping = 0x011B;
inline constexpr uint16_t kCPU_TRAPcc_word_data = 0x011C;
inline constexpr uint16_t kCPU_TRAPcc_long_data = 0x011D;
inline constexpr uint16_t kCPU_TRAPcc_illegal_format = 0x011E;
inline constexpr uint16_t kCPU_PACK = 0x011F;
inline constexpr uint16_t kCPU_UNPK = 0x0120;
inline constexpr uint16_t kCPU_MMU_op = 0x0121;
inline constexpr uint16_t kCPU_Recalc_PC_Block_fails = 0x0122;
inline constexpr uint16_t kCPU_Bad_rounding_precision_in_myfp_SetFPCR = 0x0201;
inline constexpr uint16_t kCPU_Reserved_bits_not_zero_in_myfp_SetFPCR = 0x0202;
inline constexpr uint16_t kCPU_Bad_rounding_precision_in_myfp_GetFPCR = 0x0203;

/* FPU */
inline constexpr uint16_t kFPU_unimplemented_Floating_Point_Instruction = 0x0301;
inline constexpr uint16_t kFPU_Invalid_FTRAPcc = 0x0302;
inline constexpr uint16_t kFPU_FTRAPcc_trapping = 0x0303;
inline constexpr uint16_t kFPU_Packed_Decimal_in_GetFPSource = 0x0304;
inline constexpr uint16_t kFPU_Packed_Decimal_in_FMOVE = 0x0305;

/* VIDEO */
inline constexpr uint16_t kVIDEO_vidROMSize_too_small = 0x0A01;
inline constexpr uint16_t kVIDEO_SetVidMode_not_page_0 = 0x0A02;
inline constexpr uint16_t kVIDEO_kCmndVideoControl_unknown_csCode = 0x0A04;
inline constexpr uint16_t kVIDEO_GetEntries_not_implemented = 0x0A05;
inline constexpr uint16_t kVIDEO_Video_Access_kCmndVideoStatus = 0x0A06;
inline constexpr uint16_t kVIDEO_Video_Access_unknown_commnd = 0x0A07;

/* MACH */
inline constexpr uint16_t kMACH_ATT_list_not_big_enough = 0x1101;
inline constexpr uint16_t kMACH_VIA1_word = 0x1106;
inline constexpr uint16_t kMACH_VIA1_odd = 0x1107;
inline constexpr uint16_t kMACH_VIA1_nonstandard_address = 0x1108;
inline constexpr uint16_t kMACH_VIA2_word = 0x1109;
inline constexpr uint16_t kMACH_VIA2_odd = 0x110A;
inline constexpr uint16_t kMACH_VIA2_nonstandard_address = 0x110B;
inline constexpr uint16_t kMACH_SCC_unassigned_address = 0x110C;
inline constexpr uint16_t kMACH_Attemped_Phase_Adjust = 0x110D;
inline constexpr uint16_t kMACH_SCC_even_odd = 0x110E;
inline constexpr uint16_t kMACH_SCC_wr_rd_base_wrong = 0x110F;
inline constexpr uint16_t kMACH_SCC_nonstandard_address = 0x1110;
inline constexpr uint16_t kMACH_Sony_byte = 0x1111;
inline constexpr uint16_t kMACH_Sony_odd = 0x1112;
inline constexpr uint16_t kMACH_Sony_read = 0x1113;
inline constexpr uint16_t kMACH_ASC_word = 0x1114;
inline constexpr uint16_t kMACH_SCSI_word = 0x1115;
inline constexpr uint16_t kMACH_SCSI_even_odd = 0x1116;
inline constexpr uint16_t kMACH_SCSI_nonstandard_address = 0x1117;
inline constexpr uint16_t kMACH_IWM_unassigned_address = 0x1118;
inline constexpr uint16_t kMACH_IWM_word = 0x1119;
inline constexpr uint16_t kMACH_IWM_odd = 0x111A;
inline constexpr uint16_t kMACH_IWM_even = 0x111B;
inline constexpr uint16_t kMACH_IWM_nonstandard_address = 0x111C;

/* PMU */
inline constexpr uint16_t kPMU_PMU_BuffL_too_small_for_kPMUxPramWrite = 0x0E01;
inline constexpr uint16_t kPMU_bad_range_for_kPMUxPramWrite = 0x0E02;
inline constexpr uint16_t kPMU_Wrong_PMU_i_for_kPMUpramWrite = 0x0E03;
inline constexpr uint16_t kPMU_kPMUpMgrADBoff_nonzero_length = 0x0E04;
inline constexpr uint16_t kPMU_Unknown_kPMUxPramRead_op = 0x0E05;
inline constexpr uint16_t kPMU_Unknown_kPMUtimeRead_op = 0x0E06;
inline constexpr uint16_t kPMU_Unknown_kPMUpramWrite_op = 0x0E07;
inline constexpr uint16_t kPMU_Wrong_PMU_i_for_kPMUpramWrite_2 = 0x0E08;
inline constexpr uint16_t kPMU_Unknown_kPMUpramRead_op = 0x0E09;
inline constexpr uint16_t kPMU_Wrong_PMU_i_for_kPMUpramRead = 0x0E0A;
inline constexpr uint16_t kPMU_Unknown_PMU_op = 0x0E0B;
inline constexpr uint16_t kPMU_PmuToReady_ChangeNtfy_while_PMU_Sending = 0x0E0C;
inline constexpr uint16_t kPMU_PMU_p_null_while_kPMUStateRecievingBuffe = 0x0E0D;

/* PMU */
inline constexpr uint16_t kPMU_Talk_to_unknown_mouse_register = 0x0D01;
inline constexpr uint16_t kPMU_unknown_listen_op_to_mouse_register_3 = 0x0D02;
inline constexpr uint16_t kPMU_listen_to_unknown_mouse_register = 0x0D03;
inline constexpr uint16_t kPMU_Talk_to_unknown_keyboard_register = 0x0D04;
inline constexpr uint16_t kPMU_unknown_listen_op_to_keyboard_register_3 = 0x0D05;
inline constexpr uint16_t kPMU_listen_to_unknown_keyboard_register = 0x0D06;
inline constexpr uint16_t kPMU_Unhandled_ADB_Flush = 0x0D07;

/* RTC */
inline constexpr uint16_t kRTC_Write_RTC_Reg_unknown = 0x0801;
inline constexpr uint16_t kRTC_Read_RTC_Reg_unknown = 0x0802;
inline constexpr uint16_t kRTC_RTC_aborting = 0x0803;
inline constexpr uint16_t kRTC_write_RTC_Data_unexpected_direction = 0x0804;

/* SCC */
inline constexpr uint16_t kSCC_Already_CTSpacketPending = 0x0701;
inline constexpr uint16_t kSCC_EndOfFrame_true_in_rx_complete = 0x0702;
inline constexpr uint16_t kSCC_RxChrAvail_false_in_rx_complete = 0x0703;
inline constexpr uint16_t kSCC_SyncHunt_true_in_rx_complete = 0x0704;
inline constexpr uint16_t kSCC_RR_3 = 0x0706;
inline constexpr uint16_t kSCC_read_rr8_when_RxEnable = 0x0707;
inline constexpr uint16_t kSCC_RR_12 = 0x0708;
inline constexpr uint16_t kSCC_RR_13 = 0x0709;
inline constexpr uint16_t kSCC_RR_15 = 0x070A;
inline constexpr uint16_t kSCC_Reset_Rx_CRC_Checker = 0x070B;
inline constexpr uint16_t kSCC_Send_Abort_SDLC = 0x070C;
inline constexpr uint16_t kSCC_Rx_INT_on_special_condition_only = 0x070D;
inline constexpr uint16_t kSCC_interrupt_vector_0 = 0x070E;
inline constexpr uint16_t kSCC_interrupt_vector_1 = 0x070F;
inline constexpr uint16_t kSCC_interrupt_vector_2 = 0x0710;
inline constexpr uint16_t kSCC_interrupt_vector_3 = 0x0711;
inline constexpr uint16_t kSCC_interrupt_vector_6 = 0x0712;
inline constexpr uint16_t kSCC_interrupt_vector_7 = 0x0713;
inline constexpr uint16_t kSCC_Auto_Enables = 0x0714;
inline constexpr uint16_t kSCC_16_bit_sync_char = 0x0715;
inline constexpr uint16_t kSCC_External_sync_mode = 0x0716;
inline constexpr uint16_t kSCC_Clock_Rate_X32 = 0x0717;
inline constexpr uint16_t kSCC_Clock_Rate_X64 = 0x0718;
inline constexpr uint16_t kSCC_SDLC_CRC_16 = 0x0719;
inline constexpr uint16_t kSCC_Tx_Bits_Character_5 = 0x071A;
inline constexpr uint16_t kSCC_Tx_Bits_Character_7 = 0x071B;
inline constexpr uint16_t kSCC_Tx_Bits_Character_6 = 0x071C;
inline constexpr uint16_t kSCC_unexpect_flag_character_for_SDLC = 0x071D;
inline constexpr uint16_t kSCC_WR7_and_not_SDLC = 0x071E;
inline constexpr uint16_t kSCC_write_when_Transmit_Buffer_not_Enabled = 0x071F;
inline constexpr uint16_t kSCC_VIS = 0x0720;
inline constexpr uint16_t kSCC_packet_too_small_in = 0x0721;
inline constexpr uint16_t kSCC_unexpected_size_of_control_packet_in = 0x0722;
inline constexpr uint16_t kSCC_DLC = 0x0723;
inline constexpr uint16_t kSCC_Status_high_low = 0x0724;
inline constexpr uint16_t kSCC_WR9_b5_should_be_0 = 0x0725;
inline constexpr uint16_t kSCC_SCC_Reset = 0x0726;
inline constexpr uint16_t kSCC_6_bit_8_bit_sync = 0x0727;
inline constexpr uint16_t kSCC_loop_mode = 0x0728;
inline constexpr uint16_t kSCC_abort_flag_on_underrun = 0x0729;
inline constexpr uint16_t kSCC_mark_flag_idle = 0x072A;
inline constexpr uint16_t kSCC_go_active_on_poll = 0x072B;
inline constexpr uint16_t kSCC_Data_Encoding_NRZI = 0x072C;
inline constexpr uint16_t kSCC_Data_Encoding_FM1 = 0x072D;
inline constexpr uint16_t kSCC_TRxC_OUT_transmit_clock = 0x072E;
inline constexpr uint16_t kSCC_TRxC_OUT_BR_generator_output = 0x072F;
inline constexpr uint16_t kSCC_TRxC_OUT_dpll_output = 0x0730;
inline constexpr uint16_t kSCC_TRxC_O_I = 0x0731;
inline constexpr uint16_t kSCC_transmit_clock_RTxC_pin = 0x0732;
inline constexpr uint16_t kSCC_transmit_clock_dpll_output = 0x0733;
inline constexpr uint16_t kSCC_receive_clock_RTxC_pin = 0x0734;
inline constexpr uint16_t kSCC_RTxC_XTAL_NO_XTAL = 0x0735;
inline constexpr uint16_t kSCC_BR_generator_source = 0x0736;
inline constexpr uint16_t kSCC_DTR_request_function = 0x0737;
inline constexpr uint16_t kSCC_auto_echo = 0x0738;
inline constexpr uint16_t kSCC_local_loopback = 0x0739;
inline constexpr uint16_t kSCC_disable_dpll = 0x073A;
inline constexpr uint16_t kSCC_set_source_br_generator = 0x073B;
inline constexpr uint16_t kSCC_set_source_RTxC = 0x073C;
inline constexpr uint16_t kSCC_set_NRZI_mode = 0x073D;
inline constexpr uint16_t kSCC_WR15_b0_should_be_0 = 0x073E;
inline constexpr uint16_t kSCC_zero_count_IE = 0x073F;
inline constexpr uint16_t kSCC_WR15_b2_should_be_0 = 0x0740;
inline constexpr uint16_t kSCC_not_DCD_IE = 0x0741;
inline constexpr uint16_t kSCC_SYNC_HUNT_IE = 0x0742;
inline constexpr uint16_t kSCC_Tx_underrun_EOM_IE = 0x0743;
inline constexpr uint16_t kSCC_RR_4 = 0x0744;
inline constexpr uint16_t kSCC_RR_5 = 0x0745;
inline constexpr uint16_t kSCC_RR_6 = 0x0746;
inline constexpr uint16_t kSCC_RR_7 = 0x0747;
inline constexpr uint16_t kSCC_RR_9 = 0x0748;
inline constexpr uint16_t kSCC_RR_11 = 0x0749;
inline constexpr uint16_t kSCC_RR_14 = 0x074A;
inline constexpr uint16_t kSCC_unexpected_SCC_Reg_in_SCC_GetReg = 0x074B;
inline constexpr uint16_t kSCC_unexpected_SCC_Reg_in_SCC_PutReg = 0x074C;

/* KYBD */
inline constexpr uint16_t kKYBD_KybdState_kKybdStateRecievingCommand = 0x0B01;
inline constexpr uint16_t kKYBD_KybdState_kKybdStateRecievingEndCommand = 0x0B02;

/* SONY */
inline constexpr uint16_t kSONY_MyMoveBytesVM_fails = 0x0901;
inline constexpr uint16_t kSONY_Failed_to_find_dataChecksum = 0x0902;
inline constexpr uint16_t kSONY_Failed_to_find_tagChecksum = 0x0903;
inline constexpr uint16_t kSONY_bad_dataChecksum = 0x0904;
inline constexpr uint16_t kSONY_bad_tagChecksum = 0x0905;
inline constexpr uint16_t kSONY_not_blockwise_in_Sony_Prime = 0x0908;
inline constexpr uint16_t kSONY_unexpected_OpCode_in_Sony_Control = 0x0909;
inline constexpr uint16_t kSONY_unexpected_OpCode_in_Sony_Control_2 = 0x090A;

/* VIA2 */
inline constexpr uint16_t kVIA2_IWM_Data_Read = 0x0601;
inline constexpr uint16_t kVIA2_IWM_Handshake_Read = 0x0602;

/* VIA base offsets (used by via_base.cpp) */
inline constexpr uint16_t kVIA1_Base = 0x0400;
inline constexpr uint16_t kVIA2_Base = 0x0500;

} // namespace AbnormalID
