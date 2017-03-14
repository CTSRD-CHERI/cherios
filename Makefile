BUILDDIR=	build

default: all

run: all
	../output/sdk256/bin/qemu-system-cheri -M malta -m 2048 -nographic -kernel ${BUILDDIR}/boot/cherios.elf -drive format=raw,file=${BUILDDIR}/boot/fs.img -net nic -net user -redir tcp:12818::22

all: ${BUILDDIR}/build.ninja
	cd ${BUILDDIR} && ninja

${BUILDDIR}:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/build.ninja: ${BUILDDIR}
	cd ${BUILDDIR} && cmake -GNinja ..

clean:
	rm -rfv ${BUILDDIR}

