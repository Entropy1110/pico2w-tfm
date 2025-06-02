# Directions

## Building SPE
❯ `cmake -S ../../pico2w-trusted-firmware-m -B ./spe   -DTFM_PLATFORM=rpi/rp2350 -DPICO_BOARD=pico2_w   -DTFM_PROFILE=profile_medium   -DPICO_SDK_PATH=/home/yrina/pico2w-tfm/pico-sdk   -DPLATFORM_DEFAULT_PROVISIONING=OFF`
❯ `cmake --build ./spe -- -j8 install`

## Building NSPE
❯ `cmake -S ../../pico2w-tfm-exmaple       -B ./nspe       -DTFM_PLATFORM=rpi/rp2350       -DPICO_SDK_PATH=/home/yrina/pico2w-tfm/pico-sdk       -DCONFIG_SPE_PATH=/home/yrina/pico2w-tfm/pico2w-tfm-exmaple/build/spe/api_ns       -DPICO_BOARD=pico2_w       -DTFM_TOOLCHAIN_FILE=/home/yrina/pico2w-tfm/pico2w-tfm-exmaple/build/spe/api_ns/cmake/toolchain_ns_GNUARM.cmake`

❯ `cmake --build ./nspe -- -j8 install`

## Convert SPE/NSPE into a UF2 for Flashing
❯ `cd </path/to/pico2w-tfm>`
❯ `./pico_uf2.sh pico2w-tfm-exmaple build`
