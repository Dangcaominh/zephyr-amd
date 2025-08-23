source zephyr_env/bin/activate
LOPPER_DTC_FLAGS="-b 0 -@" west lopper-command -p microblaze_riscv_0 -s ../zephyr_sdt/zephyr/system-top.dts -w .