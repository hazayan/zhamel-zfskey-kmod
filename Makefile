# SPDX-License-Identifier: BSD-2-Clause

KMOD=	zhamel_zfskey
SRCS=	zhamel_zfskey.c vnode_if.h

SYSDIR?=	/usr/src/sys
SRCTOP?=	${SYSDIR:H}

OPENZFSDIR=	${SRCTOP}/sys/contrib/openzfs
OPENZFSINCDIR=	${OPENZFSDIR}/include
OPENZFSMODDIR=	${OPENZFSDIR}/module

CFLAGS+= -I${OPENZFSINCDIR}
CFLAGS+= -I${OPENZFSINCDIR}/os/freebsd
CFLAGS+= -I${OPENZFSINCDIR}/os/freebsd/spl
CFLAGS+= -I${OPENZFSINCDIR}/os/freebsd/zfs
CFLAGS+= -I${OPENZFSMODDIR}/icp/include
CFLAGS+= -I${OPENZFSMODDIR}/zstd/include

CFLAGS+= -D__KERNEL__ -DFREEBSD_NAMECACHE -DBUILDING_ZFS \
	-DHAVE_UIO_ZEROCOPY -DWITHOUT_NETDUMP -D__KERNEL \
	-D_SYS_CONDVAR_H_ -D_SYS_VMEM_H_

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "powerpcspe" || ${MACHINE_ARCH} == "arm"
CFLAGS+= -DBITS_PER_LONG=32
.else
CFLAGS+= -DBITS_PER_LONG=64
.endif

.include <bsd.kmod.mk>
