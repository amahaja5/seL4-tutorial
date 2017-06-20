deps_config := \
	/home/abu/sel4-tutorials/tools/common/Kconfig \
	tools/elfloader/Kconfig \
	libs/libplatsupport/Kconfig \
	libs/libutils/Kconfig \
	libs/libsel4simple-default/Kconfig \
	libs/libsel4simple/Kconfig \
	libs/libsel4platsupport/Kconfig \
	libs/libsel4debug/Kconfig \
	libs/libsel4utils/Kconfig \
	libs/libelf/Kconfig \
	libs/libcpio/Kconfig \
	libs/libsel4allocman/Kconfig \
	libs/libsel4vspace/Kconfig \
	libs/libsel4vka/Kconfig \
	libs/libsel4muslcsys/Kconfig \
	libs/libmuslc/Kconfig \
	libs/libsel4/Kconfig \
	apps/hello-3-nolibs/Kconfig \
	apps/hello-2-nolibs/Kconfig \
	apps/hello-timer-client/Kconfig \
	apps/hello-timer/Kconfig \
	apps/hello-4-app/Kconfig \
	apps/hello-4/Kconfig \
	apps/hello-3/Kconfig \
	apps/hello-2/Kconfig \
	apps/hello-1/Kconfig \
	/home/abu/sel4-tutorials/kernel/src/plat/pc99/Kconfig \
	/home/abu/sel4-tutorials/kernel/src/arch/arm/Kconfig \
	kernel/Kconfig \
	Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(KERNEL_ROOT_PATH)" "/home/abu/sel4-tutorials/kernel"
include/config/auto.conf: FORCE
endif
ifneq "$(COMMON_PATH)" "/home/abu/sel4-tutorials/tools/common"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
