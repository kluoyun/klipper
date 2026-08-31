// ESP32-S3 USB register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_USB_REGS_H
#define __ESP32S3_USB_REGS_H

#define USB_SERIAL_JTAG_CONF0      (ESP_USB_SERIAL_JTAG_BASE + 0x018u)
#define USB_SERIAL_JTAG_PAD_ENABLE BIT32(14)
#define USB_WRAP_OTG_CONF          (ESP_USB_WRAP_BASE + 0x000u)
#define USB_WRAP_PAD_ENABLE        BIT32(18)
#define USB_WRAP_AHB_CLK_FORCE_ON  BIT32(19)
#define USB_WRAP_PHY_CLK_FORCE_ON  BIT32(20)
#define USB_WRAP_PHY_SELECT_EXT    BIT32(2)
#define USB_WRAP_EXCHG_PINS        BIT32(6)
#define USB_WRAP_EXCHG_OVERRIDE    BIT32(5)
#define USB_WRAP_PAD_PULL_OVERRIDE BIT32(12)
#define USB_WRAP_DP_PULLUP         BIT32(13)
#define USB_WRAP_DP_PULLDOWN       BIT32(14)
#define USB_WRAP_DM_PULLUP         BIT32(15)
#define USB_WRAP_DM_PULLDOWN       BIT32(16)
#define RTC_USB_CONF               (ESP_RTC_BASE + 0x120u)
#define RTC_SW_USB_PHY_SELECT      BIT32(19)
#define RTC_SW_HW_USB_PHY_SELECT   BIT32(20)

#define USB_REG(offset)            REG32(ESP_USB_DWC_BASE + (offset))
#define USB_FIFO(ep)               (ESP_USB_DWC_BASE + 0x1000u * ((ep) + 1u))

#define USB_GOTGCTL                USB_REG(0x000u)
#define USB_GAHBCFG                USB_REG(0x008u)
#define USB_GUSBCFG                USB_REG(0x00cu)
#define USB_GRSTCTL                USB_REG(0x010u)
#define USB_GINTSTS                USB_REG(0x014u)
#define USB_GINTMSK                USB_REG(0x018u)
#define USB_GRXSTSR                USB_REG(0x01cu)
#define USB_GRXSTSP                USB_REG(0x020u)
#define USB_GRXFSIZ                USB_REG(0x024u)
#define USB_GNPTXFSIZ              USB_REG(0x028u)
#define USB_GSNPSID                USB_REG(0x040u)
#define USB_GHWCFG2                USB_REG(0x048u)
#define USB_GDFIFOCFG              USB_REG(0x05cu)
#define USB_DIEPTXF(ep)            USB_REG(0x100u + 4u * (ep))

#define USB_DCFG                   USB_REG(0x800u)
#define USB_DCTL                   USB_REG(0x804u)
#define USB_DSTS                   USB_REG(0x808u)
#define USB_DIEPMSK                USB_REG(0x810u)
#define USB_DOEPMSK                USB_REG(0x814u)
#define USB_DAINT                  USB_REG(0x818u)
#define USB_DAINTMSK               USB_REG(0x81cu)
#define USB_DIEPEMPMSK             USB_REG(0x834u)

#define USB_DIEPCTL(ep)            USB_REG(0x900u + 0x20u * (ep))
#define USB_DIEPINT(ep)            USB_REG(0x908u + 0x20u * (ep))
#define USB_DIEPTSIZ(ep)           USB_REG(0x910u + 0x20u * (ep))
#define USB_DTXFSTS(ep)            USB_REG(0x918u + 0x20u * (ep))
#define USB_DOEPCTL(ep)            USB_REG(0xb00u + 0x20u * (ep))
#define USB_DOEPINT(ep)            USB_REG(0xb08u + 0x20u * (ep))
#define USB_DOEPTSIZ(ep)           USB_REG(0xb10u + 0x20u * (ep))
#define USB_PCGCCTL                USB_REG(0xe00u)

#define USB_GOTGCTL_VBVALID_VALUE  BIT32(3)
#define USB_GOTGCTL_BVALID_ENABLE  BIT32(6)
#define USB_GOTGCTL_BVALID_VALUE   BIT32(7)
#define USB_GAHBCFG_GLOBAL_INT     BIT32(0)
#define USB_GUSBCFG_TOCAL_MASK     0x07u
#define USB_GUSBCFG_TOCAL(value)   ((value) & USB_GUSBCFG_TOCAL_MASK)
#define USB_GUSBCFG_PHYSEL         BIT32(6)
#define USB_GUSBCFG_TRDT_MASK      (0x0fu << 10)
#define USB_GUSBCFG_TRDT(value)    ((value) << 10)
#define USB_GUSBCFG_FORCE_HOST     BIT32(29)
#define USB_GUSBCFG_FORCE_DEVICE   BIT32(30)
#define USB_GRSTCTL_CORE_RESET     BIT32(0)
#define USB_GRSTCTL_RX_FLUSH       BIT32(4)
#define USB_GRSTCTL_TX_FLUSH       BIT32(5)
#define USB_GRSTCTL_TX_FIFO(ep)    ((ep) << 6)
#define USB_GRSTCTL_ALL_TX_FIFOS   USB_GRSTCTL_TX_FIFO(0x10u)
#define USB_GRSTCTL_AHB_IDLE       BIT32(31)

#define USB_GINT_RXFLVL            BIT32(4)
#define USB_GINT_CURRENT_MODE_HOST BIT32(0)
#define USB_GINT_USBRST            BIT32(12)
#define USB_GINT_ENUMDONE          BIT32(13)
#define USB_GINT_IEPINT            BIT32(18)
#define USB_GINT_OEPINT            BIT32(19)

#define USB_GRX_EPNUM_MASK         0x0fu
#define USB_GRX_BCNT_SHIFT         4u
#define USB_GRX_BCNT_MASK          (0x7ffu << USB_GRX_BCNT_SHIFT)
#define USB_GRX_PKTSTS_SHIFT       17u
#define USB_GRX_PKTSTS_MASK        (0x0fu << USB_GRX_PKTSTS_SHIFT)

#define USB_DCFG_ADDRESS_SHIFT     4u
#define USB_DCFG_ADDRESS_MASK      (0x7fu << USB_DCFG_ADDRESS_SHIFT)
#define USB_DCFG_SPEED_MASK        0x03u
#define USB_DCFG_SPEED_FULL        0x03u
#define USB_DCFG_NZ_STATUS_STALL   BIT32(2)
#define USB_DCTL_SOFT_DISCONNECT   BIT32(1)

#define USB_EPCTL_MPS_MASK         0x7ffu
#define USB_EPCTL_ACTIVE           BIT32(15)
#define USB_EPCTL_NAKSTS           BIT32(17)
#define USB_EPCTL_TYPE(type)       ((type) << 18)
#define USB_EPCTL_STALL            BIT32(21)
#define USB_DIEPCTL_TX_FIFO(ep)    ((ep) << 22)
#define USB_EPCTL_CNAK             BIT32(26)
#define USB_EPCTL_SNAK             BIT32(27)
#define USB_EPCTL_SET_DATA0        BIT32(28)
#define USB_EPCTL_DISABLE          BIT32(30)
#define USB_EPCTL_ENABLE           BIT32(31)

#define USB_EPTSIZ_PACKET_COUNT_SHIFT 19u
#define USB_EPTSIZ_PACKET_COUNT(count) \
    ((uint32_t)(count) << USB_EPTSIZ_PACKET_COUNT_SHIFT)
#define USB_DOEPTSIZ_SETUP_COUNT   (3u << 29)
#define USB_DIEPINT_XFER_COMPLETE  BIT32(0)
#define USB_DIEPINT_EP_DISABLED    BIT32(1)
#define USB_DIEPINT_AHB_ERROR      BIT32(2)
#define USB_DIEPINT_TIMEOUT        BIT32(3)
#define USB_DIEPINT_INEPNAKEFF     BIT32(6)
#define USB_DIEPINT_TX_FIFO_UNDERRUN BIT32(8)
#define USB_DIEPINT_TX_FIFO_EMPTY  BIT32(7)
#define USB_DOEPINT_XFER_COMPLETE  BIT32(0)
#define USB_DOEPINT_SETUP          BIT32(3)
#define USB_DOEPINT_STATUS_PHASE   BIT32(5)
#define USB_DOEPINT_SETUP_RECEIVED BIT32(15)
#define USB_DIEPMSK_XFER_COMPLETE  BIT32(0)
#define USB_DIEPMSK_EPDISBLD       BIT32(1)
#define USB_DIEPMSK_AHBERR         BIT32(2)
#define USB_DIEPMSK_TIMEOUT        BIT32(3)
#define USB_DOEPMSK_XFER_COMPLETE  BIT32(0)
#define USB_DOEPMSK_SETUP          BIT32(3)

#endif // esp32s3_usb_regs.h
