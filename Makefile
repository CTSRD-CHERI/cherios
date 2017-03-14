BUILDDIR=build

default: all

run: all
	../output/sdk256/bin/qemu-system-cheri -M malta -kernel ${BUILDDIR}/boot/cherios.elf -m 2048 -nographic -hda ${BUILDDIR}/boot/fs.img -net nic -net user -redir tcp:12818::22

all: build/build.ninja
	cd ${BUILDDIR} && ninja

build:
	mkdir -p ${BUILDDIR}
	ln -sf ${BUILDDIR}/boot/cherios.elf .

build/build.ninja: build
	cd ${BUILDDIR} && cmake -GNinja ..

clean:
	rm -rfv ${BUILDDIR}

