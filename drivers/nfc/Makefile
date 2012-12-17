#
# Makefile for nfc devices
#

obj-$(CONFIG_LGE_NFC_PN544)	+= pn544_lge.o
obj-$(CONFIG_LGE_NFC_SONY_CXD2235AGG)	+= snfc/
obj-$(CONFIG_NFC_PN533)		+= pn533.o
obj-$(CONFIG_NFC_WILINK)	+= nfcwilink.o
obj-$(CONFIG_BCM2079X)		+= bcm2079x-i2c.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
