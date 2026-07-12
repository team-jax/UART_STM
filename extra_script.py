Import("env")

# build_flags only feeds CCFLAGS via SCons ParseFlagsExtended; the final
# link step (arm-none-eabi-gcc as the link driver) also needs -mfpu/
# -mfloat-abi so it selects the hard-float multilib, otherwise objects
# built with FPU codegen (FreeRTOS's ARM_CM4F port) won't link.
env.Append(LINKFLAGS=["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"])
