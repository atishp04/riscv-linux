/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __ASM_IMAGE_H
#define __ASM_IMAGE_H

#define RISCV_IMAGE_MAGIC	"RISCV"


#define RISCV_IMAGE_FLAG_BE_SHIFT	0
#define RISCV_IMAGE_FLAG_BE_MASK	0x1

#define RISCV_IMAGE_FLAG_LE		0
#define RISCV_IMAGE_FLAG_BE		1


#ifdef CONFIG_CPU_BIG_ENDIAN
#define __HEAD_FLAG_BE		RISCV_IMAGE_FLAG_BE
#else
#define __HEAD_FLAG_BE		RISCV_IMAGE_FLAG_LE
#endif

#define __HEAD_FLAG(field)	(__HEAD_FLAG_##field << \
				RISCV_IMAGE_FLAG_##field##_SHIFT)

#define __HEAD_FLAGS		(__HEAD_FLAG(BE))

#define RISCV_HEADER_VERSION_MAJOR 0
#define RISCV_HEADER_VERSION_MINOR 1

#define RISCV_HEADER_VERSION (RISCV_HEADER_VERSION_MAJOR << 16 | \
			      RISCV_HEADER_VERSION_MINOR)

#ifndef __ASSEMBLY__
/*
 * struct riscv_image_header - riscv kernel image header
 *
 * @code0:		Executable code
 * @code1:		Executable code
 * @text_offset:	Image load offset
 * @image_size:		Effective Image size
 * @flags:		kernel flags
 * @version:		version
 * @reserved:		reserved
 * @reserved:		reserved
 * @magic:		Magic number
 * @reserved:		reserved (will be used for additional RISC-V specific header)
 * @reserved:		reserved (will be used for PE COFF offset)
 */

struct riscv_image_header {
	u32 code0;
	u32 code1;
	u64 text_offset;
	u64 image_size;
	u64 flags;
	u32 version;
	u32 res1;
	u64 res2;
	u64 magic;
	u32 res3;
	u32 res4;
};
#endif /* __ASSEMBLY__ */
#endif /* __ASM_IMAGE_H */
