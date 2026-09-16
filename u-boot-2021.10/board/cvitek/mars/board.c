// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2013
 * David Feng <fenghua@phytium.com.cn>
 * Sharma Bhupesh <bhupesh.sharma@freescale.com>
 */

#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/compiler.h>
#if defined(__aarch64__)
#include <asm/armv8/mmu.h>
#endif
#include <usb/dwc2_udc.h>
#include <usb.h>
#include <spi.h>
#include "mars_reg.h"
#include "mmio.h"
#include "mars_reg_fmux_gpio.h"
#include "mars_pinlist_swconfig.h"
#include <linux/delay.h>
#include <bootstage.h>

#if defined(__riscv)
#include <asm/csr.h>
#endif

DECLARE_GLOBAL_DATA_PTR;
#define SD1_SDIO_PAD

int dw_spi_get_clk(struct udevice *bus, ulong *rate)
{
	if (!rate)
		return -EINVAL;

	*rate = 75000000;
	return 0;
}

#if defined(__aarch64__)
static struct mm_region cv181x_mem_map[] = {
	{
		.virt = 0x0UL,
		.phys = 0x0UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		.virt = PHYS_SDRAM_1,
		.phys = PHYS_SDRAM_1,
		.size = PHYS_SDRAM_1_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = cv181x_mem_map;
#endif

// #define PINMUX_CONFIG(PIN_NAME, FUNC_NAME) printf ("%s\n", PIN_NAME ##_ ##FUNC_NAME);
#define PINMUX_CONFIG(PIN_NAME, FUNC_NAME) \
		mmio_clrsetbits_32(PINMUX_BASE + FMUX_GPIO_FUNCSEL_##PIN_NAME, \
			FMUX_GPIO_FUNCSEL_##PIN_NAME##_MASK << FMUX_GPIO_FUNCSEL_##PIN_NAME##_OFFSET, \
			PIN_NAME##__##FUNC_NAME)

void pinmux_config(int io_type)
{
		switch (io_type) {
		case PINMUX_UART0:
			PINMUX_CONFIG(UART0_RX, UART0_RX);
			PINMUX_CONFIG(UART0_TX, UART0_TX);
		break;
		case PINMUX_SDIO0:
			PINMUX_CONFIG(SD0_CD, SDIO0_CD);
			// on licheervnano, this pin use for led
			//PINMUX_CONFIG(SD0_PWR_EN, SDIO0_PWR_EN);
			PINMUX_CONFIG(SD0_CMD, SDIO0_CMD);
			PINMUX_CONFIG(SD0_CLK, SDIO0_CLK);
			PINMUX_CONFIG(SD0_D0, SDIO0_D_0);
			PINMUX_CONFIG(SD0_D1, SDIO0_D_1);
			PINMUX_CONFIG(SD0_D2, SDIO0_D_2);
			PINMUX_CONFIG(SD0_D3, SDIO0_D_3);
			break;
		case PINMUX_SDIO1:
#if defined(SD1_SDIO_PAD)
			/*
			 * Name            Address            SD1  MIPI
			 * reg_sd1_phy_sel REG_0x300_0294[10] 0x0  0x1
			 */
			mmio_write_32(TOP_BASE + 0x294,
				      (mmio_read_32(TOP_BASE + 0x294) & 0xFFFFFBFF));
			PINMUX_CONFIG(SD1_CMD, PWR_SD1_CMD_VO36);
			PINMUX_CONFIG(SD1_CLK, PWR_SD1_CLK_VO37);
			PINMUX_CONFIG(SD1_D0, PWR_SD1_D0_VO35);
			PINMUX_CONFIG(SD1_D1, PWR_SD1_D1_VO34);
			PINMUX_CONFIG(SD1_D2, PWR_SD1_D2_VO33);
			PINMUX_CONFIG(SD1_D3, PWR_SD1_D3_VO32);
#elif defined(SD1_MIPI_PAD)
			/*
			 * Name            Address            SD1  MIPI
			 * reg_sd1_phy_sel REG_0x300_0294[10] 0x0  0x1
			 */
			mmio_write_32(TOP_BASE + 0x294,
				      (mmio_read_32(TOP_BASE + 0x294) & 0xFFFFFBFF) | BIT(10));
			PINMUX_CONFIG(PAD_MIPI_TXM4, SD1_CLK);
			PINMUX_CONFIG(PAD_MIPI_TXP4, SD1_CMD);
			PINMUX_CONFIG(PAD_MIPI_TXM3, SD1_D0);
			PINMUX_CONFIG(PAD_MIPI_TXP3, SD1_D1);
			PINMUX_CONFIG(PAD_MIPI_TXM2, SD1_D2);
			PINMUX_CONFIG(PAD_MIPI_TXP2, SD1_D3);
#endif
			break;
		case PINMUX_EMMC:
			PINMUX_CONFIG(EMMC_CLK, EMMC_CLK);
			PINMUX_CONFIG(EMMC_RSTN, EMMC_RSTN);
			PINMUX_CONFIG(EMMC_CMD, EMMC_CMD);
			PINMUX_CONFIG(EMMC_DAT1, EMMC_DAT_1);
			PINMUX_CONFIG(EMMC_DAT0, EMMC_DAT_0);
			PINMUX_CONFIG(EMMC_DAT2, EMMC_DAT_2);
			PINMUX_CONFIG(EMMC_DAT3, EMMC_DAT_3);
			break;
		case PINMUX_SPI_NAND:
			PINMUX_CONFIG(EMMC_DAT2, SPINAND_HOLD);
			PINMUX_CONFIG(EMMC_CLK, SPINAND_CLK);
			PINMUX_CONFIG(EMMC_DAT0, SPINAND_MOSI);
			PINMUX_CONFIG(EMMC_DAT3, SPINAND_WP);
			PINMUX_CONFIG(EMMC_CMD, SPINAND_MISO);
			PINMUX_CONFIG(EMMC_DAT1, SPINAND_CS);
		break;
		case PINMUX_DSI:
			PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
			PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
			PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
			PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
			PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
			PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
			PINMUX_CONFIG(PAD_MIPI_TXM3, XGPIOC_20);
			PINMUX_CONFIG(PAD_MIPI_TXP3, XGPIOC_21);
			PINMUX_CONFIG(PAD_MIPI_TXM4, XGPIOC_18);
			PINMUX_CONFIG(PAD_MIPI_TXP4, XGPIOC_19);
		break;
		case PINMUX_LVDS:
			PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
			PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
			PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
			PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
			PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
			PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
			PINMUX_CONFIG(PAD_MIPI_TXM3, XGPIOC_20);
			PINMUX_CONFIG(PAD_MIPI_TXP3, XGPIOC_21);
			PINMUX_CONFIG(PAD_MIPI_TXM4, XGPIOC_18);
			PINMUX_CONFIG(PAD_MIPI_TXP4, XGPIOC_19);
		break;
		default:
			break;
	}
}

#include "../cvi_board_init.c"

#if defined(CONFIG_PHY_CVITEK) /* config cvitek cv181x eth internal phy on ASIC board */
static void cv181x_ephy_id_init(void)
{
	// set rg_ephy_apb_rw_sel 0x0804@[0]=1/APB by using APB interface
	mmio_write_32(0x03009804, 0x0001);

	// Release 0x0800[0]=0/shutdown
	mmio_write_32(0x03009800, 0x0900);

	// Release 0x0800[2]=1/dig_rst_n, Let mii_reg can be accessabile
	mmio_write_32(0x03009800, 0x0904);

	// ANA INIT (PD/EN), switch to MII-page5
	mmio_write_32(0x0300907c, 0x0500);
	// Release ANA_PD p5.0x10@[13:8] = 6'b001100
	mmio_write_32(0x03009040, 0x0c00);
	// Release ANA_EN p5.0x10@[7:0] = 8'b01111110
	mmio_write_32(0x03009040, 0x0c7e);

	// Wait PLL_Lock, Lock_Status p5.0x12@[15] = 1
	//mdelay(1);

	// Release 0x0800[1] = 1/ana_rst_n
	mmio_write_32(0x03009800, 0x0906);

	// ANA INIT
	// @Switch to MII-page5
	mmio_write_32(0x0300907c, 0x0500);

	// PHY_ID
	mmio_write_32(0x03009008, 0x0043);
	mmio_write_32(0x0300900c, 0x5649);

	// switch to MDIO control by ETH_MAC
	mmio_write_32(0x03009804, 0x0000);
}

static void cv181x_ephy_led_pinmux(void)
{
	// LED PAD MUX
	mmio_write_32(0x030010e0, 0x05);
	mmio_write_32(0x030010e4, 0x05);
	//(SD1_CLK selphy)
	mmio_write_32(0x050270b0, 0x11111111);
	//(SD1_CMD selphy)
	mmio_write_32(0x050270b4, 0x11111111);
}
#endif

void cpu_pwr_ctrl(void)
{
#if defined(CONFIG_RISCV)
	mmio_write_32(0x01901008, 0x30001);// cortexa53_pwr_iso_en
#elif defined(CONFIG_ARM)
	mmio_write_32(0x01901004, 0x30001);// c906_top_pwr_iso_en
#endif
}

int board_init(void)
{
#ifndef CONFIG_TARGET_CVITEK_CV181X_FPGA
	extern volatile u32 BOOT0_START_TIME;
	u16 start_time = DIV_ROUND_UP(BOOT0_START_TIME, SYS_COUNTER_FREQ_IN_SECOND / 1000);

	// Save uboot start time. time is from boot0.h
	mmio_write_16(TIME_RECORDS_FIELD_UBOOT_START, start_time);
#endif

	cpu_pwr_ctrl();

#if defined(CONFIG_PHY_CVITEK) /* config cvitek cv181x eth internal phy on ASIC board */
	cv181x_ephy_id_init();
	cv181x_ephy_led_pinmux();
#endif

#if defined(CONFIG_NAND_SUPPORT)
	pinmux_config(PINMUX_SPI_NAND);
#elif defined(CONFIG_SPI_FLASH)
	pinmux_config(PINMUX_SPI_NOR);
#elif defined(CONFIG_EMMC_SUPPORT)
	pinmux_config(PINMUX_EMMC);
#endif
#ifdef CONFIG_DISPLAY_CVITEK_MIPI
	pinmux_config(PINMUX_DSI);
#elif defined(CONFIG_DISPLAY_CVITEK_LVDS)
	pinmux_config(PINMUX_LVDS);
#endif
	pinmux_config(PINMUX_SDIO1);
	cvi_board_init();
	return 0;
}

#define ST7789_SLPOUT	0x11
#define ST7789_INVON	0x21
#define ST7789_DISPON	0x29
#define ST7789_CASET	0x2A
#define ST7789_RASET	0x2B
#define ST7789_RAMWR	0x2C

#define GPIO0_BASE	0x03020000

#define ST7789_WIDTH	240
#define ST7789_HEIGHT	240

#include "rvclaw_logo.c"

static inline void gpio_set_output_high(u32 base, u32 bit)
{
	u32 val;

	val = mmio_read_32(base + 0x4);
	val |= BIT(bit);
	mmio_write_32(base + 0x4, val);

	val = mmio_read_32(base + 0x0);
	val |= BIT(bit);
	mmio_write_32(base + 0x0, val);
}

static inline void gpio_set_value(u32 base, u32 bit, int high)
{
	u32 val;

	val = mmio_read_32(base + 0x0);
	if (high)
		val |= BIT(bit);
	else
		val &= ~BIT(bit);
	mmio_write_32(base + 0x0, val);
}

static int st7789_spi_xfer(struct spi_slave *slave, const u8 *buf, int len)
{
	if (!len)
		return 0;

	return spi_xfer(slave, len * 8, buf, NULL, SPI_XFER_BEGIN | SPI_XFER_END);
}

static int st7789_write_cmd(struct spi_slave *slave, u8 cmd)
{
	gpio_set_value(GPIO0_BASE, 28, 0);
	return st7789_spi_xfer(slave, &cmd, 1);
}

static int st7789_write_data(struct spi_slave *slave, const u8 *buf, int len)
{
	gpio_set_value(GPIO0_BASE, 28, 1);
	return st7789_spi_xfer(slave, buf, len);
}

static int st7789_write_reg(struct spi_slave *slave, u8 cmd, const u8 *data, int len)
{
	int ret;

	ret = st7789_write_cmd(slave, cmd);
	if (ret)
		return ret;

	if (data && len)
		return st7789_write_data(slave, data, len);

	return 0;
}

static int st7789_init_and_show_logo(struct spi_slave *slave)
{
	static const u8 init_b2[] = {0x1F, 0x1F, 0x00, 0x33, 0x33};
	static const u8 init_36[] = {0x00};
	static const u8 init_3a[] = {0x05};
	static const u8 init_b7[] = {0x00};
	static const u8 init_bb[] = {0x36};
	static const u8 init_c0[] = {0x2C};
	static const u8 init_c2[] = {0x01};
	static const u8 init_c3[] = {0x13};
	static const u8 init_c4[] = {0x20};
	static const u8 init_c6[] = {0x13};
	static const u8 init_d6[] = {0xA1};
	static const u8 init_d0[] = {0xA4, 0xA1};
	static const u8 init_e0[] = {0xF0, 0x08, 0x0E, 0x09, 0x08, 0x04, 0x2F, 0x33, 0x45, 0x36, 0x13, 0x12, 0x2A, 0x2D};
	static const u8 init_e1[] = {0xF0, 0x0E, 0x12, 0x0C, 0x0A, 0x15, 0x2E, 0x32, 0x44, 0x39, 0x17, 0x18, 0x2B, 0x2F};
	static const u8 init_e4[] = {0x1D, 0x00, 0x00};
	u8 window_col[] = {0x00, 0x00, 0x00, ST7789_WIDTH - 1};
	u8 window_row[] = {0x00, 0x00, 0x00, ST7789_HEIGHT - 1};
	const int line_bytes = ST7789_WIDTH * 2;
	const int frame_bytes = ST7789_WIDTH * ST7789_HEIGHT * 2;
	const u8 *logo = rvclaw_logo;
	int i, ret;

	mmio_write_32(0x03001070, 0x3);
	gpio_set_output_high(GPIO0_BASE, 28);

	mmio_write_32(0x03001058, 0x3);
	gpio_set_output_high(GPIO0_BASE, 27);
	gpio_set_value(GPIO0_BASE, 27, 1);
	mdelay(50);
	gpio_set_value(GPIO0_BASE, 27, 0);
	mdelay(50);
	gpio_set_value(GPIO0_BASE, 27, 1);
	mdelay(50);

	ret = st7789_write_cmd(slave, ST7789_SLPOUT);
	if (ret)
		return ret;
	mdelay(120);

	ret = st7789_write_reg(slave, 0xB2, init_b2, sizeof(init_b2));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0x36, init_36, sizeof(init_36));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0x3A, init_3a, sizeof(init_3a));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xB7, init_b7, sizeof(init_b7));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xBB, init_bb, sizeof(init_bb));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xC0, init_c0, sizeof(init_c0));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xC2, init_c2, sizeof(init_c2));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xC3, init_c3, sizeof(init_c3));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xC4, init_c4, sizeof(init_c4));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xC6, init_c6, sizeof(init_c6));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xD6, init_d6, sizeof(init_d6));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xD0, init_d0, sizeof(init_d0));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xE0, init_e0, sizeof(init_e0));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xE1, init_e1, sizeof(init_e1));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, 0xE4, init_e4, sizeof(init_e4));
	if (ret)
		return ret;

	ret = st7789_write_cmd(slave, ST7789_INVON);
	if (ret)
		return ret;
	ret = st7789_write_cmd(slave, ST7789_SLPOUT);
	if (ret)
		return ret;
	ret = st7789_write_cmd(slave, ST7789_DISPON);
	if (ret)
		return ret;
	mdelay(100);

	ret = st7789_write_reg(slave, ST7789_CASET, window_col, sizeof(window_col));
	if (ret)
		return ret;
	ret = st7789_write_reg(slave, ST7789_RASET, window_row, sizeof(window_row));
	if (ret)
		return ret;
	ret = st7789_write_cmd(slave, ST7789_RAMWR);
	if (ret)
		return ret;

	if (sizeof(rvclaw_logo) < frame_bytes)
		return -EINVAL;

	for (i = 0; i < ST7789_HEIGHT; i++) {
		ret = st7789_write_data(slave, logo + i * line_bytes, line_bytes);
		if (ret)
			return ret;
	}

	mmio_write_32(0x03001064, 0x3);
	gpio_set_output_high(GPIO0_BASE, 19);
	gpio_set_value(GPIO0_BASE, 19, 0);

	return 0;
}

int board_late_init(void)
{
	int ret;
	struct spi_slave *slave;
	struct udevice *dev;

	ret = spi_get_bus_and_cs(1, 0, 30000000, SPI_MODE_0,
				 "spi_generic_drv", "spidev", &dev, &slave);
	if (ret) {
		printf("SPI1 init failed: %d\n", ret);
		return 0;
	}

	ret = spi_claim_bus(slave);
	if (ret) {
		printf("SPI1 claim failed: %d\n", ret);
		return 0;
	}

	ret = st7789_init_and_show_logo(slave);
	if (ret)
		printf("ST7789 init/logo failed: %d\n", ret);
	else
		printf("ST7789 logo done\n");

	spi_release_bus(slave);
	spi_free_slave(slave);

	return 0;
}

#if defined(__aarch64__)
int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}
#endif

#ifdef CV_SYS_OFF
static void cv_system_off(void)
{
	mmio_write_32(REG_RTC_BASE + RTC_EN_SHDN_REQ, 0x01);
	while (mmio_read_32(REG_RTC_BASE + RTC_EN_SHDN_REQ) != 0x01)
		;
	mmio_write_32(REG_RTC_CTRL_BASE + RTC_CTRL0_UNLOCKKEY, 0xAB18);
	mmio_setbits_32(REG_RTC_CTRL_BASE + RTC_CTRL0, 0xFFFF0800 | (0x1 << 0));

	while (1)
		;
}
#endif

void cv_system_reset(void)
{
	mmio_write_32(REG_RTC_BASE + RTC_EN_WARM_RST_REQ, 0x01);
	while (mmio_read_32(REG_RTC_BASE + RTC_EN_WARM_RST_REQ) != 0x01)
		;
	mmio_write_32(REG_RTC_CTRL_BASE + RTC_CTRL0_UNLOCKKEY, 0xAB18);
	mmio_setbits_32(REG_RTC_CTRL_BASE + RTC_CTRL0, 0xFFFF0800 | (0x1 << 4));

	while (1)
		;
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(void)
{
	cv_system_reset();
}

#ifdef CONFIG_USB_GADGET_DWC2_OTG
struct dwc2_plat_otg_data cv182x_otg_data = {
	.regs_otg = USB_BASE,
	.usb_gusbcfg    = 0x40081400,
	.rx_fifo_sz     = 512,
	.np_tx_fifo_sz  = 512,
	.tx_fifo_sz     = 512,
};

int board_usb_init(int index, enum usb_init_type init)
{
	u32 value;

	value = mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) & (~BIT_TOP_SOFT_RST_USB);
	mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST, value);
	udelay(50);
	value = mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) | BIT_TOP_SOFT_RST_USB;
	mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST, value);

	/* Set USB phy configuration */
	value = mmio_read_32(REG_TOP_USB_PHY_CTRL);
	mmio_write_32(REG_TOP_USB_PHY_CTRL, value | BIT_TOP_USB_PHY_CTRL_EXTVBUS
					| USB_PHY_ID_OVERRIDE_ENABLE
					| USB_PHY_ID_VALUE);

	/* Enable ECO RXF */
	mmio_write_32(REG_TOP_USB_ECO, mmio_read_32(REG_TOP_USB_ECO) | BIT_TOP_USB_ECO_RX_FLUSH);

	printf("cvi_usb_hw_init done\n");

	return dwc2_udc_probe(&cv182x_otg_data);
}
#endif

void board_save_time_record(uintptr_t saveaddr)
{
	u64 boot_us = 0;
#if defined(__aarch64__)
	boot_us = timer_get_boot_us();
#elif defined(__riscv)
	// Read from CSR_TIME directly. RISC-V timers is initialized later.
	boot_us = csr_read(CSR_TIME) / (SYS_COUNTER_FREQ_IN_SECOND / 1000000);
#else
#error "Unknown ARCH"
#endif

	mmio_write_16(saveaddr, DIV_ROUND_UP(boot_us, 1000));
}
