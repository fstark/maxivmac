/*
	SONYEMDV.c

	Copyright (C) 2009 Philip Cummins, Jesus A. Alvarez, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	SONY floppy disk EMulated DeVice

	The Sony hardware is not actually emulated. Instead the
	ROM is patched to replace the Sony disk driver with
	code that calls Mini vMac extensions implemented in
	the file.

	Information neeeded to better support the Disk Copy 4.2
	format was found in libdc42.c of the Lisa Emulator Project
	by Ray A. Arachelian, and adapted to Mini vMac
	by Jesus A. Alvarez.
*/

#include "core/common.h"
#include "cpu/cpu.h"
#include "cpu/m68k.h"

#include "devices/sony.h"
#include "core/machine_obj.h"


/*
	ReportAbnormalID unused 0x090B - 0x09FF
*/


static uint32_t vSonyMountedMask = 0;

#define vSonyIsLocked(Drive_No) \
	((vSonyWritableMask & ((uint32_t)1 << (Drive_No))) == 0)
#define vSonyIsMounted(Drive_No) \
	((vSonyMountedMask & ((uint32_t)1 << (Drive_No))) != 0)

static bool vSonyNextPendingInsert0(tDrive *Drive_No)
{
	/* find next drive to Mount */
	uint32_t MountPending = vSonyInsertedMask & (~ vSonyMountedMask);
	if (MountPending != 0) {
		tDrive i;
		for (i = 0; i < NumDrives; ++i) {
			if ((MountPending & ((uint32_t)1 << i)) != 0) {
				*Drive_No = i;
				return true; /* only one disk at a time */
			}
		}
	}

	return false;
}

static tMacErr CheckReadableDrive(tDrive Drive_No)
{
	tMacErr result;

	if (Drive_No >= NumDrives) {
		result = mnvm_nsDrvErr;
	} else if (! vSonyIsMounted(Drive_No)) {
		result = mnvm_offLinErr;
	} else {
		result = mnvm_noErr;
	}

	return result;
}

static tMacErr vSonyTransferVM(bool IsWrite,
	uint32_t Buffera, tDrive Drive_No,
	uint32_t Sony_Start, uint32_t Sony_Count, uint32_t *Sony_ActCount)
{
	/*
		Transfer data between emulated disk and emulated memory. Taking
		into account that the emulated memory may not be contiguous in
		real memory. (Though it generally is for macintosh emulation.)
	*/
	tMacErr result;
	uint32_t contig;
	uint32_t actual;
	uint8_t * Buffer;
	uint32_t offset = Sony_Start;
	uint32_t n = Sony_Count;

label_1:
	if (0 == n) {
		result = mnvm_noErr;
	} else {
		Buffer = get_real_address0(n, ! IsWrite, Buffera, &contig);
		if (0 == contig) {
			result = mnvm_miscErr;
		} else {
			result = vSonyTransfer(IsWrite, Buffer, Drive_No,
				offset, contig, &actual);
			offset += actual;
			Buffera += actual;
			n -= actual;
			if (mnvm_noErr == result) {
				goto label_1;
			}
		}
	}

	if (nullptr != Sony_ActCount) {
		*Sony_ActCount = Sony_Count - n;
	}
	return result;
}

static void MyMoveBytesVM(uint32_t srcPtr, uint32_t dstPtr, int32_t byteCount)
{
	uint8_t * src;
	uint8_t * dst;
	uint32_t contigSrc;
	uint32_t contigDst;
	uint32_t contig;

label_1:
	if (0 != byteCount) {
		src = get_real_address0(byteCount, false, srcPtr,
			&contigSrc);
		dst = get_real_address0(byteCount, true,  dstPtr,
			&contigDst);
		if ((0 == contigSrc) || (0 == contigDst)) {
			ReportAbnormalID(0x0901, "MyMoveBytesVM fails");
		} else {
			contig = (contigSrc < contigDst) ? contigSrc : contigDst;
			MyMoveBytes(src, dst, contig);
			srcPtr += contig;
			dstPtr += contig;
			byteCount -= contig;
			goto label_1;
		}
	}
}

static uint32_t ImageDataOffset[NumDrives];
	/* size of any header in disk image file */
static uint32_t ImageDataSize[NumDrives];
	/* size of disk image file contents */

#if Sony_SupportTags
static uint32_t ImageTagOffset[NumDrives];
	/* offset to disk image file tags */
#endif

#if Sony_SupportDC42
#define kDC42offset_diskName      0
#define kDC42offset_dataSize     64
#define kDC42offset_tagSize      68
#define kDC42offset_dataChecksum 72
#define kDC42offset_tagChecksum  76
#define kDC42offset_diskFormat   80
#define kDC42offset_formatByte   81
#define kDC42offset_private      82
#define kDC42offset_userData     84
#endif

#define ChecksumBlockSize 1024

#if Sony_SupportDC42 && Sony_WantChecksumsUpdated
static tMacErr DC42BlockChecksum(tDrive Drive_No,
	uint32_t Sony_Start, uint32_t Sony_Count, uint32_t *r)
{
	tMacErr result;
	uint32_t n;
	uint8_t Buffer[ChecksumBlockSize];
	uint8_t *p;
	uint32_t sum = 0;
	uint32_t offset = Sony_Start;
	uint32_t remaining = Sony_Count;

	while (0 != remaining) {
		/* read a block */
		if (remaining > ChecksumBlockSize) {
			n = ChecksumBlockSize;
		} else {
			n = remaining;
		}

		result = vSonyTransfer(false, Buffer, Drive_No, offset,
			n, nullptr);
		if (mnvm_noErr != result) {
			return result;
		}

		offset += n;
		remaining -= n;

		/* add to Checksum */
		p = Buffer;
		n >>= 1; /* n = number of words */
		while (0 != n) {
			--n;
			/* ROR.l sum+word */
			sum += do_get_mem_word(p);
			p += 2;
			sum = (sum >> 1) | ((sum & 1) << 31);
		}
	}

	*r = sum;
	return mnvm_noErr;
}
#endif

#if Sony_SupportDC42 && Sony_WantChecksumsUpdated
#if Sony_SupportTags
#define SizeCheckSumsToUpdate 8
#else
#define SizeCheckSumsToUpdate 4
#endif
#endif

#if Sony_WantChecksumsUpdated
static void Drive_UpdateChecksums(tDrive Drive_No)
{
	if (! vSonyIsLocked(Drive_No)) {
		uint32_t DataOffset = ImageDataOffset[Drive_No];
#if Sony_SupportDC42
		if (kDC42offset_userData == DataOffset) {
			/* a disk copy 4.2 image */
			tMacErr result;
			uint32_t dataChecksum;
			uint8_t Buffer[SizeCheckSumsToUpdate];
			uint32_t Sony_Count = SizeCheckSumsToUpdate;
			uint32_t DataSize = ImageDataSize[Drive_No];

			/* Checksum image data */
			result = DC42BlockChecksum(Drive_No,
				DataOffset, DataSize, &dataChecksum);
			if (mnvm_noErr != result) {
				ReportAbnormalID(0x0902, "Failed to find dataChecksum");
				dataChecksum = 0;
			}
			do_put_mem_long(Buffer, dataChecksum);
#if Sony_SupportTags
			{
				uint32_t tagChecksum;
				uint32_t TagOffset = ImageTagOffset[Drive_No];
				uint32_t TagSize =
					(0 == TagOffset) ? 0 : ((DataSize >> 9) * 12);
				if (TagSize < 12) {
					tagChecksum = 0;
				} else {
					/*
						Checksum of tags doesn't include first block.
						presumably because of bug in original disk
						copy program.
					*/
					result = DC42BlockChecksum(Drive_No,
						TagOffset + 12, TagSize - 12, &tagChecksum);
					if (mnvm_noErr != result) {
						ReportAbnormalID(0x0903,
							"Failed to find tagChecksum");
						tagChecksum = 0;
					}
				}
				do_put_mem_long(Buffer + 4, tagChecksum);
			}
#endif

			/* write Checksums */
			vSonyTransfer(true, Buffer, Drive_No,
				kDC42offset_dataChecksum, Sony_Count, nullptr);
		}
#endif
	}
}
#endif

#define checkheaderblocks 64

#define checkheaderoffset 0
#if NonDiskProtect
#define checkheadersize (checkheaderblocks * 512)
#else
#define checkheadersize 128
#endif

#define Sony_SupportOtherFormats Sony_SupportDC42

static tMacErr vSonyNextPendingInsert(tDrive *Drive_No)
{
	tDrive i;
	tMacErr result;
	uint32_t L;

	if (! vSonyNextPendingInsert0(&i)) {
		result = mnvm_nsDrvErr;
	} else {
		result = vSonyGetSize(i, &L);
		if (mnvm_noErr == result) {
			/* first, set up for default format */
			uint32_t DataOffset = 0;
			uint32_t DataSize = L;
#if Sony_SupportTags
			uint32_t TagOffset = 0;
#endif

#if Sony_SupportOtherFormats || NonDiskProtect
#if IncludeSonyRawMode
			if (! vSonyRawMode)
#endif
			{
				uint8_t Temp[checkheadersize];
				uint32_t Sony_Count = checkheadersize;
				bool gotFormat = false;

#if NonDiskProtect
				if (L < checkheadersize) {
					WarnMsgUnsupportedDisk();
					result = -1;
				} else
#endif
				if (mnvm_noErr == (result = vSonyTransfer(false,
					Temp, i, checkheaderoffset, Sony_Count, nullptr)))
				{
#if Sony_SupportDC42
					/* Detect Disk Copy 4.2 image */
					if (0x0100 == do_get_mem_word(
						&Temp[kDC42offset_private]))
					{
						/* DC42 signature found, check sizes */
						uint32_t DataSize0 = do_get_mem_long(
							&Temp[kDC42offset_dataSize]);
						uint32_t TagSize0 = do_get_mem_long(
							&Temp[kDC42offset_tagSize]);
						uint32_t DataOffset0 = kDC42offset_userData;
						uint32_t TagOffset0 = DataOffset0 + DataSize0;
						if (L >= (TagOffset0 + TagSize0))
						if (0 == (DataSize0 & 0x01FF))
						if ((DataSize0 >> 9) >= 4)
						if (Temp[kDC42offset_diskName] < 64)
							/* length of pascal string */
						{
							if (0 == TagSize0) {
								/* no tags */
								gotFormat = true;
							} else if ((DataSize0 >> 9) * 12
								== TagSize0)
							{
								/* 12 byte tags */
								gotFormat = true;
							}
							if (gotFormat) {
#if Sony_VerifyChecksums /* mostly useful to check the Checksum code */
								uint32_t dataChecksum;
								uint32_t tagChecksum;
								uint32_t dataChecksum0 = do_get_mem_long(
									&Temp[kDC42offset_dataChecksum]);
								uint32_t tagChecksum0 = do_get_mem_long(
									&Temp[kDC42offset_tagChecksum]);
								result = DC42BlockChecksum(i,
									DataOffset0, DataSize0,
									&dataChecksum);
								if (TagSize0 >= 12) {
									result = DC42BlockChecksum(i,
										TagOffset0 + 12, TagSize0 - 12,
										&tagChecksum);
								} else {
									tagChecksum = 0;
								}
								if (dataChecksum != dataChecksum0) {
									ReportAbnormalID(0x0904,
										"bad dataChecksum");
								}
								if (tagChecksum != tagChecksum0) {
									ReportAbnormalID(0x0905,
										"bad tagChecksum");
								}
#endif
								DataOffset = DataOffset0;
								DataSize = DataSize0;
#if Sony_SupportTags
								TagOffset =
									(0 == TagSize0) ? 0 : TagOffset0;
#endif

#if (! Sony_SupportTags) || (! Sony_WantChecksumsUpdated)
								if (! vSonyIsLocked(i)) {
#if ! Sony_WantChecksumsUpdated
									/* unconditionally revoke */
#else
									if (0 != TagSize0)
#endif
									{
										DiskRevokeWritable(i);
									}
								}
#endif
							}
						}
					}
#endif /* Sony_SupportDC42 */
#if NonDiskProtect
					if (! gotFormat) {
						uint16_t drSigWord = do_get_mem_word(
							&Temp[0x400]);

#if 0 /* don't look at boot blocks */
						uint16_t bbID = do_get_mem_word(
							&Temp[0]);
						if ((0x4C4B == bbID) || (0 == bbID))
						if ((Temp[0x0A] < 16)
								/* length System name */
							&& (Temp[0x1A] < 16)
								/* length Finder name */
							&& (Temp[0x2A] < 16)
								/* length Macsbug name */
							)
#endif
						if ((0x4244 == drSigWord) /* HFS */
							|| (0xD2D7 == drSigWord) /* MFS */
							)
						if ((0x4244 != drSigWord)
							|| (3 == do_get_mem_word(
								&Temp[0x40E]))) /* drVBMSt */
						if (0 == (0x01FF & do_get_mem_long(
							&Temp[0x414]))) /* drAlBlkSiz */
						if (0 == (0x01FF & do_get_mem_long(
							&Temp[0x418]))) /* clump size */
						if ((Temp[0x424] < 28)
								/* length Volume name */
							)
						{
							gotFormat = true;
						}
					}

					// Handle file with HFS partitions. Based on the Basilisk II find_hfs_partition implementation.
					if (! gotFormat) {
						int i;
						for (i = 0; i < checkheaderblocks; i++) {
							uint16_t drSigWord = do_get_mem_word(&Temp[512 * i]);
							if (drSigWord == 0x504D) { // HFS partition map magic number.
								uint8_t * map = &Temp[512 * i];
								if (strcmp((char *)(map + 48), "Apple_HFS") == 0) {
									DataOffset = ((map[8] << 24) | (map[9] << 16) | (map[10] << 8) | map[11]) << 9;
									DataSize = 512 * ((map[12] << 24) | (map[13] << 16) | (map[14] << 8) | map[15]);
									gotFormat = true;
									break;
								}
							}
						}
					}

					if (! gotFormat) {
						WarnMsgUnsupportedDisk();
						result = -1;
					}
#endif
				}
			}
			if (mnvm_noErr == result)
#endif /* Sony_SupportOtherFormats */
			{
				vSonyMountedMask |= ((uint32_t)1 << i);

				ImageDataOffset[i] = DataOffset;
				ImageDataSize[i] = DataSize;
#if Sony_SupportTags
				ImageTagOffset[i] = TagOffset;
#endif

				*Drive_No = i;
			}
		}

		if (mnvm_noErr != result) {
			(void) vSonyEject(i);
		}
	}

	return result;
}

#define MinTicksBetweenInsert 240
	/*
		if call PostEvent too frequently, insert events seem to get lost
	*/

static uint16_t DelayUntilNextInsert;

static uint32_t MountCallBack = 0;

/* This checks to see if a disk (image) has been inserted */
void SonyDevice::update()
{
	if (DelayUntilNextInsert != 0) {
		--DelayUntilNextInsert;
	} else {
		if (MountCallBack != 0) {
			tDrive i;

			if (mnvm_noErr == vSonyNextPendingInsert(&i)) {
				uint32_t data = i;

				if (vSonyIsLocked(i)) {
					data |= ((uint32_t)0x00FF) << 16;
				}

				g_cpu.diskInsertedPseudoException(MountCallBack, data);

#if IncludeSonyRawMode
				if (! vSonyRawMode)
#endif
				{
					DelayUntilNextInsert = MinTicksBetweenInsert;
					/*
						but usually will reach kDriveStatus first,
						where shorten delay.
					*/
				}
			}
		}
	}
}

static tMacErr Drive_Transfer(bool IsWrite, uint32_t Buffera,
	tDrive Drive_No, uint32_t Sony_Start, uint32_t Sony_Count,
	uint32_t *Sony_ActCount)
{
	tMacErr result;

	QuietEnds();

	if (nullptr != Sony_ActCount) {
		*Sony_ActCount = 0;
	}

	result = CheckReadableDrive(Drive_No);
	if (mnvm_noErr == result) {
		if (IsWrite && vSonyIsLocked(Drive_No)) {
			result = mnvm_vLckdErr;
		} else {
			uint32_t DataSize = ImageDataSize[Drive_No];
			if (Sony_Start > DataSize) {
				result = mnvm_eofErr;
			} else {
				bool hit_eof = false;
				uint32_t L = DataSize - Sony_Start;
				if (L >= Sony_Count) {
					L = Sony_Count;
				} else {
					hit_eof = true;
				}
				result = vSonyTransferVM(IsWrite, Buffera, Drive_No,
					ImageDataOffset[Drive_No] + Sony_Start, L,
					Sony_ActCount);
				if ((mnvm_noErr == result) && hit_eof) {
					result = mnvm_eofErr;
				}
			}
		}
	}

	return result;
}

static bool QuitOnEject = false;

void SonyDevice::setQuitOnEject()
{
	QuitOnEject = true;
}

static tMacErr Drive_Eject(tDrive Drive_No)
{
	tMacErr result;

	result = CheckReadableDrive(Drive_No);
	if (mnvm_noErr == result) {
		vSonyMountedMask &= ~ ((uint32_t)1 << Drive_No);
#if Sony_WantChecksumsUpdated
		Drive_UpdateChecksums(Drive_No);
#endif
		result = vSonyEject(Drive_No);
		if (QuitOnEject != 0) {
			if (! AnyDiskInserted()) {
				ForceMacOff = true;
			}
		}
	}

	return result;
}

#if IncludeSonyNew
static tMacErr Drive_EjectDelete(tDrive Drive_No)
{
	tMacErr result;

	result = CheckReadableDrive(Drive_No);
	if (mnvm_noErr == result) {
		if (vSonyIsLocked(Drive_No)) {
			result = mnvm_vLckdErr;
		} else {
			vSonyMountedMask &= ~ ((uint32_t)1 << Drive_No);
			result = vSonyEjectDelete(Drive_No);
		}
	}

	return result;
}
#endif

void SonyDevice::ejectAllDisks()
{
	tDrive i;

	vSonyMountedMask = 0;
	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
#if Sony_WantChecksumsUpdated
			Drive_UpdateChecksums(i);
#endif
			(void) vSonyEject(i);
		}
	}
}

void SonyDevice::reset()
{
	DelayUntilNextInsert = 0;
	QuitOnEject = false;
	MountCallBack = 0;
}

/*
	Mini vMac extension for low level access to disk operations.
*/

#define kCmndDiskNDrives 1
#define kCmndDiskRead 2
#define kCmndDiskWrite 3
#define kCmndDiskEject 4
#define kCmndDiskGetSize 5
#define kCmndDiskGetCallBack 6
#define kCmndDiskSetCallBack 7
#define kCmndDiskQuitOnEject 8
#define kCmndDiskFeatures 9
#define kCmndDiskNextPendingInsert 10
#if IncludeSonyRawMode
#define kCmndDiskGetRawMode 11
#define kCmndDiskSetRawMode 12
#endif
#if IncludeSonyNew
#define kCmndDiskNew 13
#define kCmndDiskGetNewWanted 14
#define kCmndDiskEjectDelete 15
#endif
#if IncludeSonyGetName
#define kCmndDiskGetName 16
#endif

#define kFeatureCmndDisk_RawMode 0
#define kFeatureCmndDisk_New 1
#define kFeatureCmndDisk_NewName 2
#define kFeatureCmndDisk_GetName 3

#define kParamDiskNumDrives 8
#define kParamDiskStart 8
#define kParamDiskCount 12
#define kParamDiskBuffer 16
#define kParamDiskDrive_No 20

void SonyDevice::extnDiskAccess(uint32_t p)
{
	tMacErr result = mnvm_controlErr;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 2);
			result = mnvm_noErr;
			break;
		case kCmndDiskNDrives: /* count drives */
			put_vm_word(p + kParamDiskNumDrives, NumDrives);
			result = mnvm_noErr;
			break;
		case kCmndDiskRead:
			{
				uint32_t Sony_ActCount;
				uint32_t Buffera = get_vm_long(p + kParamDiskBuffer);
				tDrive Drive_No = get_vm_word(p + kParamDiskDrive_No);
				uint32_t Sony_Start = get_vm_long(p + kParamDiskStart);
				uint32_t Sony_Count = get_vm_long(p + kParamDiskCount);

				result = Drive_Transfer(false, Buffera, Drive_No,
					Sony_Start, Sony_Count, &Sony_ActCount);

				put_vm_long(p + kParamDiskCount, Sony_ActCount);
			}
			break;
		case kCmndDiskWrite:
			{
				uint32_t Sony_ActCount;
				uint32_t Buffera = get_vm_long(p + kParamDiskBuffer);
				tDrive Drive_No = get_vm_word(p + kParamDiskDrive_No);
				uint32_t Sony_Start = get_vm_long(p + kParamDiskStart);
				uint32_t Sony_Count = get_vm_long(p + kParamDiskCount);

				result = Drive_Transfer(true, Buffera, Drive_No,
					Sony_Start, Sony_Count, &Sony_ActCount);

				put_vm_long(p + kParamDiskCount, Sony_ActCount);
			}
			break;
		case kCmndDiskEject:
			{
				tDrive Drive_No = get_vm_word(p + kParamDiskDrive_No);
				result = Drive_Eject(Drive_No);
			}
			break;
		case kCmndDiskGetSize:
			{
				tDrive Drive_No = get_vm_word(p + kParamDiskDrive_No);

				result = CheckReadableDrive(Drive_No);
				if (mnvm_noErr == result) {
					put_vm_long(p + kParamDiskCount,
						ImageDataSize[Drive_No]);
					result = mnvm_noErr;
				}
			}
			break;
		case kCmndDiskGetCallBack:
			put_vm_long(p + kParamDiskBuffer, MountCallBack);
			result = mnvm_noErr;
			break;
		case kCmndDiskSetCallBack:
			MountCallBack = get_vm_long(p + kParamDiskBuffer);
			result = mnvm_noErr;
			break;
		case kCmndDiskQuitOnEject:
			QuitOnEject = true;
			result = mnvm_noErr;
			break;
		case kCmndDiskFeatures:
			{
				uint32_t v = (0
#if IncludeSonyRawMode
					| ((uint32_t)1 << kFeatureCmndDisk_RawMode)
#endif
#if IncludeSonyNew
					| ((uint32_t)1 << kFeatureCmndDisk_New)
#endif
#if IncludeSonyNameNew
					| ((uint32_t)1 << kFeatureCmndDisk_NewName)
#endif
#if IncludeSonyGetName
					| ((uint32_t)1 << kFeatureCmndDisk_GetName)
#endif
					);

				put_vm_long(p + ExtnDat_params + 0, v);
				result = mnvm_noErr;
			}
			break;
		case kCmndDiskNextPendingInsert:
			{
				tDrive i;

				result = vSonyNextPendingInsert(&i);
				if (mnvm_noErr == result) {
					put_vm_word(p + kParamDiskDrive_No, i);
				}
			}
			break;
#if IncludeSonyRawMode
		case kCmndDiskGetRawMode:
			put_vm_word(p + kParamDiskBuffer, vSonyRawMode);
			result = mnvm_noErr;
			break;
		case kCmndDiskSetRawMode:
			vSonyRawMode = get_vm_word(p + kParamDiskBuffer);
			result = mnvm_noErr;
			break;
#endif
#if IncludeSonyNew
		case kCmndDiskNew:
			{
				uint32_t count = get_vm_long(p + ExtnDat_params + 0);
				tPbuf Pbuf_No = get_vm_word(p + ExtnDat_params + 4);
				/* reserved word at offset 6, should be zero */

				result = mnvm_noErr;

#if IncludePbufs
				if (Pbuf_No != NotAPbuf) {
					result = CheckPbuf(Pbuf_No);
					if (mnvm_noErr == result) {
						vSonyNewDiskWanted = true;
						vSonyNewDiskSize = count;
#if IncludeSonyNameNew
						if (vSonyNewDiskName != NotAPbuf) {
							PbufDispose(vSonyNewDiskName);
						}
						vSonyNewDiskName = Pbuf_No;
#else
						PbufDispose(Pbuf_No);
#endif
					}
				} else
#endif
				{
					vSonyNewDiskWanted = true;
					vSonyNewDiskSize = count;
				}
			}
			break;
		case kCmndDiskGetNewWanted:
			put_vm_word(p + kParamDiskBuffer, vSonyNewDiskWanted);
			result = mnvm_noErr;
			break;
		case kCmndDiskEjectDelete:
			{
				tDrive Drive_No = get_vm_word(p + kParamDiskDrive_No);
				result = Drive_EjectDelete(Drive_No);
			}
			break;
#endif
#if IncludeSonyGetName
		case kCmndDiskGetName:
			{
				tDrive Drive_No = get_vm_word(p + ExtnDat_params + 0);
				/* reserved word at offset 2, should be zero */
				result = CheckReadableDrive(Drive_No);
				if (mnvm_noErr == result) {
					tPbuf Pbuf_No;
					result = vSonyGetName(Drive_No, &Pbuf_No);
					put_vm_word(p + ExtnDat_params + 4, Pbuf_No);
				}
			}
			break;
#endif
	}

	put_vm_word(p + ExtnDat_result, result);
}


/*
	Mini vMac extension that implements most of the logic
	of the replacement disk driver patched into the emulated ROM.
	(sony_driver in ROMEMDEV.c)

	This logic used to be completely contained in the 68k code
	of the replacement driver, using only the low level
	disk access extension.
*/

/* Sony Variable Drive Setting Offsets */

#define kTrack       0 /* Current Track */
#define kWriteProt   2 /* FF if Write Protected, 00 if readable */
#define kDiskInPlace 3
	/*
		00 = No Disk, 01 = Disk In,
		2 = MacOS Read, FC-FF = Just Ejected
	*/
#define kInstalled   4
	/* 00 = Unknown, 01 = Installed, FF = Not Installed */
#define kSides       5
	/* 00 if Single Sided Drive, FF if Doubled Sided Drive */
#define kQLink       6 /* Link to Next Drive */
#define kQType      10 /* Drive Type (0 = Size Saved, 1 = Very Large) */
#define kQDriveNo   12 /* Drive Number (1 = Internal, 2 = External) */
#define kQRefNum    14
	/* Driver Reference Number (-5 for .Sony, FFFB) */
#define kQFSID      16 /* File System ID (0 = MacOS) */
#define kQDrvSz     18 /* size, low-order word */
#define kQDrvSz2    20 /* size, hi-order word */

#define kTwoSideFmt 18
	/* FF if double-sided format, 00 if single-sided format */
#define kNewIntf    19
	/* FF if new 800K interface or 00 if old 400K interface */
#define kDriveErrs  20 /* Drive Soft Errors */

/* Sony Driver Control Call csCodes */

#define kKillIO             1
#define kVerifyDisk         5
#define kFormatDisk         6
#define kEjectDisk          7
#define kSetTagBuffer       8
#define kTrackCacheControl  9
#define kGetIconID         20
#define kDriveIcon         21
#define kMediaIcon         22
#define kDriveInfo         23
#define kFormatCopy     21315

/* Sony Driver Status Call csCodes */

#define kReturnFormatList  6
#define kDriveStatus       8
#define kMFMStatus        10
#define kDuplicatorVersionSupport  17494

/* Parameter Block Offsets */

#define kqLink         0
#define kqType         4
#define kioTrap        6
#define kioCmdAddr     8
#define kioCompletion 12
#define kioResult     16
#define kioNamePtr    18
#define kioVRefNum    22
#define kioRefNum     24
#define kcsCode       26
#define kcsParam      28
#define kioBuffer     32 /* Buffer to store data into */
#define kioReqCount   36 /* Requested Number of Bytes */
#define kioActCount   40 /* Actual Number of Bytes obtained */
#define kioPosMode    44 /* Positioning Mode */
#define kioPosOffset  46 /* Position Offset */

/* Positioning Modes */

#define kfsAtMark    0 /* At Mark (Ignore PosOffset) */
#define kfsFromStart 1 /* At Start (PosOffset is absolute) */
#define kfsFromLEOF  2 /* From Logical End of File - PosOffset */
#define kfsFromMark  3 /* At Mark + PosOffset */

/* Device Control Entry Offsets */

#define kdCtlPosition 16

#if 0
struct MyDriverDat_R {
	uint32_t zeroes[4];  /*  0 */
	uint32_t checkval;   /* 16 */
	uint32_t pokeaddr;   /* 20 */
	uint16_t NumDrives;  /* 24 */
	uint16_t DiskExtn;   /* 26 */
	TMTask NullTask; /* 28 */
	VBLTask NullVBLTask; /* 48 */
	/* total size must be <= FirstDriveVarsOffset */
};

typedef struct MyDriverDat_R MyDriverDat_R;
#endif


static uint32_t sony_SonyVarsPtr() {
	auto m = g_machine->config().model;
	if (static_cast<int>(m) <= static_cast<int>(MacModel::Twiggy)) return 0x0128;
	return 0x0134;
}
#define SonyVarsPtr sony_SonyVarsPtr()

static uint32_t sony_FirstDriveVarsOffset() {
	auto m = g_machine->config().model;
	if (m == MacModel::Twiggy) return 0x004C;
	return 0x004A;
}
#define FirstDriveVarsOffset sony_FirstDriveVarsOffset()

static uint32_t sony_EachDriveVarsSize() {
	auto m = g_machine->config().model;
	if (m == MacModel::Twiggy) return 0x002E;
	return 0x0042;
}
#define EachDriveVarsSize sony_EachDriveVarsSize()

static uint32_t sony_MinSonVarsSize() {
	auto m = g_machine->config().model;
	if (m == MacModel::Twig43) return 0x000000FA;
	if (m == MacModel::Twiggy) return 0x000000E6;
	if (m == MacModel::Mac128K) return 0x000000FA;
	return 0x00000310;
}
#define MinSonVarsSize sony_MinSonVarsSize()

#define kcom_checkval 0x841339E2

#define Sony_dolog (dbglog_HAVE && 0)

#if Sony_SupportTags
static uint32_t TheTagBuffer;
#endif

static uint32_t DriveVarsLocation(tDrive Drive_No)
{
	uint32_t SonyVars = get_vm_long(SonyVarsPtr);

	if (Drive_No < NumDrives) {
		return SonyVars + FirstDriveVarsOffset
			+ EachDriveVarsSize * Drive_No;
	} else {
		return 0;
	}
}

static tMacErr Sony_Mount(uint32_t p)
{
	uint32_t data = get_vm_long(p + ExtnDat_params + 0);
	tMacErr result = mnvm_miscErr;
	tDrive i = data & 0x0000FFFF;
	uint32_t dvl = DriveVarsLocation(i);

	if (0 == dvl) {
#if Sony_dolog
		dbglog_WriteNote("Sony : Mount : no dvl");
#endif

		result = mnvm_nsDrvErr;
	} else if (get_vm_byte(dvl + kDiskInPlace) == 0x00) {
		uint32_t L = ImageDataSize[i] >> 9; /* block count */

#if Sony_dolog
		dbglog_StartLine();
		dbglog_writeCStr("Sony : Mount : Drive=");
		dbglog_writeHex(i);
		dbglog_writeCStr(", L=");
		dbglog_writeHex(L);
		dbglog_writeReturn();
#endif

		auto mdl = g_machine->config().model;
		if (static_cast<int>(mdl) <= static_cast<int>(MacModel::Twiggy)
			&& L == 1702)
		{
			put_vm_byte(dvl + kTwoSideFmt, 0xFF);
				/* Drive i Single Format */
			put_vm_byte(dvl + kNewIntf, 0x00);
				/* Drive i doesn't use new interface */
			put_vm_word(dvl + kQType, 0x00); /* Drive Type */
			put_vm_word(dvl + kDriveErrs, 0x0000);
				/* Drive i has no errors */
		} else if ((L == 800)
			|| (static_cast<int>(mdl) > static_cast<int>(MacModel::Mac128K)
				&& L == 1600))
		{
			if (static_cast<int>(mdl) <= static_cast<int>(MacModel::Mac128K)) {
				put_vm_byte(dvl + kTwoSideFmt, 0x00);
					/* Drive i Single Format */
				put_vm_byte(dvl + kNewIntf, 0x00);
					/* Drive i doesn't use new interface */
			} else {
				if (L == 800) {
					put_vm_byte(dvl + kTwoSideFmt, 0x00);
						/* Drive i Single Format */
				} else {
					put_vm_byte(dvl + kTwoSideFmt, 0xFF);
						/* Drive Double Format */
				}
				put_vm_byte(dvl + kNewIntf, 0xFF);
					/* Drive i uses new interface */
			}
			put_vm_word(dvl + kQType, 0x00); /* Drive Type */
			put_vm_word(dvl + kDriveErrs, 0x0000);
				/* Drive i has no errors */
		} else
		{
			put_vm_word(dvl + kQRefNum, 0xFFFE);  /* Driver */
			put_vm_word(dvl + kQType, 0x01); /* Drive Type */
			put_vm_word(dvl + kQDrvSz , L);
			put_vm_word(dvl + kQDrvSz2, L >> 16);
		}

		if (static_cast<int>(g_machine->config().model) <= static_cast<int>(MacModel::Twiggy)) {
			put_vm_word(dvl + kQFSID, 0x00); /* kQFSID must be 0 for 4.3T */
		}

		put_vm_byte(dvl + kWriteProt, data >> 16);
		put_vm_byte(dvl + kDiskInPlace, 0x01); /* Drive Disk Inserted */

		put_vm_long(p + ExtnDat_params + 4, i + 1);
			/* PostEvent Disk Inserted eventMsg */
		result = mnvm_noErr;
	} else {
		/* disk already in place, a mistake has been made */
#if Sony_dolog
		dbglog_WriteNote("Sony : Mount : already in place");
#endif
	}

	return result;
}

#if Sony_SupportTags
static tMacErr Sony_PrimeTags(tDrive Drive_No,
	uint32_t Sony_Start, uint32_t Sony_Count, bool IsWrite)
{
	tMacErr result = mnvm_noErr;
	uint32_t TagOffset = ImageTagOffset[Drive_No];

	if ((0 != TagOffset) && (Sony_Count > 0)) {
		uint32_t block = Sony_Start >> 9;
		uint32_t n = Sony_Count >> 9; /* is >= 1 if get here */

		TagOffset += block * 12;

		if (0 != TheTagBuffer) {
			uint32_t count = 12 * n;
			result = vSonyTransferVM(IsWrite, TheTagBuffer, Drive_No,
				TagOffset, count, nullptr);
			if (mnvm_noErr == result) {
				MyMoveBytesVM(TheTagBuffer + count - 12, 0x02FC, 12);
			}
		} else {
			if (! IsWrite) {
				/* only need to read the last block tags */
				uint32_t count = 12;
				TagOffset += 12 * (n - 1);
				result = vSonyTransferVM(false, 0x02FC, Drive_No,
					TagOffset, count, nullptr);
			} else {
				uint32_t count = 12;
				uint16_t BufTgFBkNum = get_vm_word(0x0302);
				do {
					put_vm_word(0x0302, BufTgFBkNum);
					result = vSonyTransferVM(true, 0x02FC, Drive_No,
						TagOffset, count, nullptr);
					if (mnvm_noErr != result) {
						goto label_fail;
					}
					BufTgFBkNum += 1;
					TagOffset += 12;
				} while (--n != 0);
			}
		}
	}

label_fail:
	return result;
}
#endif

/* Handles I/O to disks */
static tMacErr Sony_Prime(uint32_t p)
{
	tMacErr result;
	uint32_t Sony_Count;
	uint32_t Sony_Start;
	uint32_t Sony_ActCount = 0;
	uint32_t ParamBlk = get_vm_long(p + ExtnDat_params + 0);
	uint32_t DeviceCtl = get_vm_long(p + ExtnDat_params + 4);
	tDrive Drive_No = get_vm_word(ParamBlk + kioVRefNum) - 1;
	uint16_t IOTrap = get_vm_word(ParamBlk + kioTrap);
	uint32_t dvl = DriveVarsLocation(Drive_No);

	if (0 == dvl) {
#if Sony_dolog
		dbglog_WriteNote("Sony : Prime : no dvl");
#endif

		result = mnvm_nsDrvErr;
	} else if (static_cast<int>(g_machine->config().model) >= static_cast<int>(MacModel::Twiggy)
		&& 0xA002 != (IOTrap & 0xF0FE))
	{
#if Sony_dolog
		dbglog_WriteNote("Sony : Prime : "
			"not read (0xA002) or write (0xA003)");
#endif

		result = mnvm_controlErr;
	} else
	{
		bool IsWrite = (0 != (IOTrap & 0x0001));
		uint8_t DiskInPlaceV = get_vm_byte(dvl + kDiskInPlace);

		if (DiskInPlaceV != 0x02) {
			if (DiskInPlaceV == 0x01) {
				put_vm_byte(dvl + kDiskInPlace, 0x02); /* Clamp Drive */
			} else {
				result = mnvm_offLinErr;
				goto label_fail;
				/*
					if don't check for this, will go right
					ahead and boot off a disk that hasn't
					been mounted yet by Sony_Update.
					(disks other than the boot disk aren't
					seen unless mounted by Sony_Update)
				*/
			}
		}

#if 0
		uint16_t PosMode = get_vm_word(ParamBlk + kioPosMode);

		if (0 != (PosMode & 64)) {
#if ExtraAbnormalReports
			/*
				This is used when copy to floppy
				disk with Finder. But not implemented
				yet.
			*/
			ReportAbnormalID(0x0906, "read verify mode requested");
#endif
			PosMode &= ~ 64;
		}

		/*
			Don't use the following code, because
			according to Apple's Technical Note FL24
			the Device Manager takes care of this,
			and puts the result in dCtlPosition.
			(The RAMDisk example in Apple's sample
			code serves to confirm this. Further
			evidence found in Basilisk II emulator,
			and disassembly of Mac Plus disk driver.)
		*/
		uint32_t PosOffset = get_vm_long(ParamBlk + kioPosOffset);
		switch (PosMode) {
			case kfsAtMark:
				Sony_Start = get_vm_long(DeviceCtl + kdCtlPosition);
				break;
			case kfsFromStart:
				Sony_Start = PosOffset;
				break;
#if 0
			/*
				not valid for device driver.
				actually only kfsFromStart seems to be used.
			*/
			case kfsFromLEOF:
				Sony_Start = ImageDataSize[Drive_No]
					+ PosOffset;
				break;
#endif
			case kfsFromMark:
				Sony_Start = PosOffset
					+ get_vm_long(DeviceCtl + kdCtlPosition);
				break;
			default:
				ReportAbnormalID(0x0907, "unknown PosMode");
				result = mnvm_paramErr;
				goto label_fail;
				break;
		}
#endif
		Sony_Start = get_vm_long(DeviceCtl + kdCtlPosition);

		Sony_Count = get_vm_long(ParamBlk + kioReqCount);

#if Sony_dolog
		dbglog_StartLine();
		dbglog_writeCStr("Sony : Prime : Drive=");
		dbglog_writeHex(Drive_No);
		dbglog_writeCStr(", IsWrite=");
		dbglog_writeHex(IsWrite);
		dbglog_writeCStr(", Start=");
		dbglog_writeHex(Sony_Start);
		dbglog_writeCStr(", Count=");
		dbglog_writeHex(Sony_Count);
		dbglog_writeReturn();
#endif

		if ((0 != (Sony_Start & 0x1FF))
			|| (0 != (Sony_Count & 0x1FF)))
		{
			/* only whole blocks allowed */
#if ExtraAbnormalReports
			ReportAbnormalID(0x0908, "not blockwise in Sony_Prime");
#endif
			result = mnvm_paramErr;
		} else if (IsWrite && (get_vm_byte(dvl + kWriteProt) != 0)) {
			result = mnvm_wPrErr;
		} else {
			uint32_t Buffera = get_vm_long(ParamBlk + kioBuffer);
			result = Drive_Transfer(IsWrite, Buffera, Drive_No,
					Sony_Start, Sony_Count, &Sony_ActCount);
#if Sony_SupportTags
			if (mnvm_noErr == result) {
				result = Sony_PrimeTags(Drive_No,
					Sony_Start, Sony_Count, IsWrite);
			}
#endif
			put_vm_long(DeviceCtl + kdCtlPosition,
				Sony_Start + Sony_ActCount);
		}
	}

label_fail:
	put_vm_word(ParamBlk + kioResult, result);
	put_vm_long(ParamBlk + kioActCount, Sony_ActCount);

	if (mnvm_noErr != result) {
		put_vm_word(0x0142 /* DskErr */, result);
	}
	return result;
}

/* Implements control csCodes for the Sony driver */
static tMacErr Sony_Control(uint32_t p)
{
	tMacErr result;
	uint32_t ParamBlk = get_vm_long(p + ExtnDat_params + 0);
	/* uint32_t DeviceCtl = get_vm_long(p + ExtnDat_params + 4); */
	uint16_t OpCode = get_vm_word(ParamBlk + kcsCode);

	if (kKillIO == OpCode) {
#if Sony_dolog
		dbglog_WriteNote("Sony : Control : kKillIO");
#endif

		result = mnvm_miscErr;
	} else if (kSetTagBuffer == OpCode) {
#if Sony_dolog
		dbglog_WriteNote("Sony : Control : kSetTagBuffer");
#endif

#if Sony_SupportTags
		TheTagBuffer = get_vm_long(ParamBlk + kcsParam);
		result = mnvm_noErr;
#else
		result = mnvm_controlErr;
#endif
	} else if (kTrackCacheControl == OpCode) {
#if Sony_dolog
		dbglog_WriteNote("Sony : Control : kTrackCacheControl");
#endif

		if (static_cast<int>(g_machine->config().model) <= static_cast<int>(MacModel::Mac128K)) {
			result = mnvm_controlErr;
		} else {
#if 0
		uint8_t Arg1 = get_vm_byte(ParamBlk + kcsParam);
		uint8_t Arg2 = get_vm_byte(ParamBlk + kcsParam + 1);
		if (0 == Arg1) {
			/* disable track cache */
		} else {
			/* enable track cache */
		}
		if (Arg2 < 0) {
			/* remove track cache */
		} else if (Arg2 > 0) {
			/* install track cache */
		}
#endif
		result = mnvm_noErr;
			/* not implemented, but pretend we did it */
		}
	} else {
		tDrive Drive_No = get_vm_word(ParamBlk + kioVRefNum) - 1;
		uint32_t dvl = DriveVarsLocation(Drive_No);

		if (0 == dvl) {
#if Sony_dolog
			dbglog_WriteNote("Sony : Control : no dvl");
#endif

			result = mnvm_nsDrvErr;
		} else if (get_vm_byte(dvl + kDiskInPlace) == 0) {
#if Sony_dolog
			dbglog_WriteNote("Sony : Control : not DiskInPlace");
#endif

			result = mnvm_offLinErr;
		} else {
			switch (OpCode) {
				case kVerifyDisk :
#if Sony_dolog
					dbglog_WriteNote("Sony : Control : kVerifyDisk");
#endif

					result = mnvm_noErr;
					break;
				case kEjectDisk :
#if Sony_dolog
					dbglog_StartLine();
					dbglog_writeCStr("Sony : Control : kEjectDisk : ");
					dbglog_writeHex(Drive_No);
					dbglog_writeReturn();
#endif

					put_vm_byte(dvl + kWriteProt, 0x00);
						/* Drive Writeable */
					put_vm_byte(dvl + kDiskInPlace, 0x00);
						/* Drive No Disk */
#if 0
					put_vm_byte(dvl + kTwoSideFmt, 0x00);
						/* Drive Single Format (Initially) */
#endif
					put_vm_word(dvl + kQRefNum, 0xFFFB);
						/* Drive i uses .Sony */

					result = Drive_Eject(Drive_No);
					break;
				case kFormatDisk :
#if Sony_dolog
					dbglog_StartLine();
					dbglog_writeCStr("Sony : Control : kFormatDisk : ");
					dbglog_writeHex(Drive_No);
					dbglog_writeReturn();
#endif

					result = mnvm_noErr;
					break;
				case kDriveIcon :
#if Sony_dolog
					dbglog_StartLine();
					dbglog_writeCStr("Sony : Control : kDriveIcon : ");
					dbglog_writeHex(Drive_No);
					dbglog_writeReturn();
#endif

					if (get_vm_word(dvl + kQType) != 0) {
						put_vm_long(ParamBlk + kcsParam,
							my_disk_icon_addr);
						result = mnvm_noErr;
					} else {
						result = mnvm_controlErr;
							/*
								Driver can't respond to
								this Control call (-17)
							*/
					}
					break;
				case kDriveInfo :
				  if (g_machine->config().isSEOrLater())
					{
						uint32_t v;

#if Sony_dolog
						dbglog_StartLine();
						dbglog_writeCStr(
							"Sony : Control : kDriveInfo : ");
						dbglog_writeHex(kDriveIcon);
						dbglog_writeReturn();
#endif

						if (get_vm_word(dvl + kQType) != 0) {
							v = 0x00000001; /* unspecified drive */
						} else {
							v = 0x00000003; /* 800K Drive */
						}
						if (Drive_No != 0) {
							v += 0x00000900;
								/* Secondary External Drive */
						}
						put_vm_long(ParamBlk + kcsParam, v);
						result = mnvm_noErr; /* No error (0) */
					}
					break;
				default :
#if Sony_dolog
					dbglog_StartLine();
					dbglog_writeCStr("Sony : Control : OpCode : ");
					dbglog_writeHex(OpCode);
					dbglog_writeReturn();
#endif
#if ExtraAbnormalReports
					if ((kGetIconID != OpCode)
						&& (kMediaIcon != OpCode)
						&& (kDriveInfo != OpCode))
					{
						ReportAbnormalID(0x0909,
							"unexpected OpCode in Sony_Control");
					}
#endif
					result = mnvm_controlErr;
						/*
							Driver can't respond to
							this Control call (-17)
						*/
					break;
			}
		}
	}

	if (mnvm_noErr != result) {
		put_vm_word(0x0142 /* DskErr */, result);
	}
	return result;
}

/* Handles the DriveStatus call */
static tMacErr Sony_Status(uint32_t p)
{
	tMacErr result;
	uint32_t ParamBlk = get_vm_long(p + ExtnDat_params + 0);
	/* uint32_t DeviceCtl = get_vm_long(p + ExtnDat_params + 4); */
	uint16_t OpCode = get_vm_word(ParamBlk + kcsCode);

#if Sony_dolog
	dbglog_StartLine();
	dbglog_writeCStr("Sony : Sony_Status OpCode = ");
	dbglog_writeHex(OpCode);
	dbglog_writeReturn();
#endif

	if (kDriveStatus == OpCode) {
		tDrive Drive_No = get_vm_word(ParamBlk + kioVRefNum) - 1;
		uint32_t Src = DriveVarsLocation(Drive_No);
		if (Src == 0) {
			result = mnvm_nsDrvErr;
		} else {
			if (DelayUntilNextInsert > 4) {
				DelayUntilNextInsert = 4;
			}
			MyMoveBytesVM(Src, ParamBlk + kcsParam, 22);
			result = mnvm_noErr;
		}
	} else {
#if ExtraAbnormalReports
		if ((kReturnFormatList != OpCode)
			&& (kDuplicatorVersionSupport != OpCode))
		{
			ReportAbnormalID(0x090A,
				"unexpected OpCode in Sony_Control");
		}
#endif
		result = mnvm_statusErr;
	}

	if (mnvm_noErr != result) {
		put_vm_word(0x0142 /* DskErr */, result);
	}
	return result;
}

static tMacErr Sony_Close(uint32_t p)
{
#if 0
	uint32_t ParamBlk = get_vm_long(p + ExtnDat_params + 0);
	uint32_t DeviceCtl = get_vm_long(p + ExtnDat_params + 4);
#endif
	UnusedParam(p);
	return mnvm_closErr; /* Can't Close Driver */
}

static tMacErr Sony_OpenA(uint32_t p)
{
#if Sony_dolog
	dbglog_WriteNote("Sony : OpenA");
#endif

	if (MountCallBack != 0) {
		return mnvm_opWrErr; /* driver already open */
	} else {
		uint32_t L = FirstDriveVarsOffset + EachDriveVarsSize * NumDrives;

		if (L < MinSonVarsSize) {
			L = MinSonVarsSize;
		}

		put_vm_long(p + ExtnDat_params + 0, L);

		return mnvm_noErr;
	}
}

static tMacErr Sony_OpenB(uint32_t p)
{
	int16_t i;
	uint32_t dvl;

#if Sony_dolog
	dbglog_WriteNote("Sony : OpenB");
#endif

	uint32_t SonyVars = get_vm_long(p + ExtnDat_params + 4);
	/* uint32_t ParamBlk = get_vm_long(p + ExtnDat_params + 24); (unused) */
	uint32_t DeviceCtl = 0;
	if (static_cast<int>(g_machine->config().model) > static_cast<int>(MacModel::Mac128K)) {
		DeviceCtl = get_vm_long(p + ExtnDat_params + 28);
	}

	put_vm_long(SonyVars + 16 /* checkval */, kcom_checkval);
	put_vm_long(SonyVars + 20 /* pokeaddr */, kExtn_Block_Base);
	put_vm_word(SonyVars + 24 /* NumDrives */, NumDrives);
	put_vm_word(SonyVars + 26 /* DiskExtn */, kExtnDisk);

	put_vm_long(SonyVarsPtr, SonyVars);

	for (i = 0; (dvl = DriveVarsLocation(i)) != 0; ++i) {
		put_vm_byte(dvl + kDiskInPlace, 0x00); /* Drive i No Disk */
		put_vm_byte(dvl + kInstalled, 0x01);   /* Drive i Installed */
		if (static_cast<int>(g_machine->config().model) <= static_cast<int>(MacModel::Mac128K)) {
			put_vm_byte(dvl + kSides, 0x00);
				/* Drive i Single Sided */
		} else {
			put_vm_byte(dvl + kSides, 0xFF);
				/* Drive i Double Sided */
		}
		put_vm_word(dvl + kQDriveNo, i + 1);   /* Drive i is Drive 1 */
		put_vm_word(dvl + kQRefNum, 0xFFFB);   /* Drive i uses .Sony */
	}

	{
		uint32_t UTableBase = get_vm_long(0x011C);

		put_vm_long(UTableBase + 4 * 1,
			get_vm_long(UTableBase + 4 * 4));
			/* use same drive for hard disk as used for sony floppies */
	}

	if (static_cast<int>(g_machine->config().model) > static_cast<int>(MacModel::Mac128K)) {
		/* driver version in driver i/o queue header */
		put_vm_byte(DeviceCtl + 7, 1);
	}

	if (static_cast<int>(g_machine->config().model) <= static_cast<int>(MacModel::Mac128K)) {
		/* init Drive Queue */
		put_vm_word(0x308, 0);
		put_vm_long(0x308 + 2, 0);
		put_vm_long(0x308 + 6, 0);
	}

	put_vm_long(p + ExtnDat_params + 8,
		SonyVars + FirstDriveVarsOffset + kQLink);
	put_vm_word(p + ExtnDat_params + 12, EachDriveVarsSize);
	put_vm_word(p + ExtnDat_params + 14, NumDrives);
	put_vm_word(p + ExtnDat_params + 16, 1);
	put_vm_word(p + ExtnDat_params + 18, 0xFFFB);
	if (static_cast<int>(g_machine->config().model) <= static_cast<int>(MacModel::Mac128K)) {
		put_vm_long(p + ExtnDat_params + 20, 0);
	} else {
		put_vm_long(p + ExtnDat_params + 20, SonyVars + 28 /* NullTask */);
	}

#if Sony_SupportTags
	TheTagBuffer = 0;
#endif

	return mnvm_noErr;
}

static tMacErr Sony_OpenC(uint32_t p)
{
#if Sony_dolog
	dbglog_WriteNote("Sony : OpenC");
#endif

	MountCallBack = get_vm_long(p + ExtnDat_params + 0);
	if (g_machine->config().isIIFamily()) {
		MountCallBack |= 0x40000000;
	}
	return mnvm_noErr;
}

#define kCmndSonyPrime 1
#define kCmndSonyControl 2
#define kCmndSonyStatus 3
#define kCmndSonyClose 4
#define kCmndSonyOpenA 5
#define kCmndSonyOpenB 6
#define kCmndSonyOpenC 7
#define kCmndSonyMount 8

void SonyDevice::extnSonyAccess(uint32_t p)
{
	tMacErr result;

	switch (get_vm_word(p + ExtnDat_commnd)) {
		case kCmndVersion:
			put_vm_word(p + ExtnDat_version, 0);
			result = mnvm_noErr;
			break;
		case kCmndSonyPrime:
			result = Sony_Prime(p);
			break;
		case kCmndSonyControl:
			result = Sony_Control(p);
			break;
		case kCmndSonyStatus:
			result = Sony_Status(p);
			break;
		case kCmndSonyClose:
			result = Sony_Close(p);
			break;
		case kCmndSonyOpenA:
			result = Sony_OpenA(p);
			break;
		case kCmndSonyOpenB:
			result = Sony_OpenB(p);
			break;
		case kCmndSonyOpenC:
			result = Sony_OpenC(p);
			break;
		case kCmndSonyMount:
			result = Sony_Mount(p);
			break;
		default:
			result = mnvm_controlErr;
			break;
	}

	put_vm_word(p + ExtnDat_result, result);
}