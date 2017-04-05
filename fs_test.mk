
ifeq ($(OS),Windows_NT)
LWEXT4_CLIENT = @build_generic\\fs_test\\lwext4-client
LWEXT4_SERVER = @build_generic\\fs_test\\lwext4-server
else
LWEXT4_CLIENT = @build_generic/fs_test/lwext4-client
LWEXT4_SERVER = @build_generic/fs_test/lwext4-server
endif

TEST_DIR = /test

t0:
	@echo "T0: Device register test:" 
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"

t1:
	@echo "T1: Single mount-umount test:" 
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "umount /"

t2:
	@echo "T2: Multiple mount-umount test:" 
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "umount /"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "umount /"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "umount /"

t3:
	@echo "T3: Test dir create/remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 0"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"
	
t4:
	@echo "T4: 10 files create + write + read + remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 10"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 10 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 10 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 10"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	$(LWEXT4_CLIENT) -c "multi_fremove  $(TEST_DIR) /f 10"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"
	
t5:
	@echo "T5: 100 files create + write + read + remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 100"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 100 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 100 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 100"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	$(LWEXT4_CLIENT) -c "multi_fremove  $(TEST_DIR) /f 100"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"

t6:
	@echo "T6: 1000 files create + write + read + remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 1000"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 1000 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 1000 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 1000"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	$(LWEXT4_CLIENT) -c "multi_fremove  $(TEST_DIR) /f 1000"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"

t7:
	@echo "T7: 10 dirs create + remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 10"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 10"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	$(LWEXT4_CLIENT) -c "multi_dremove $(TEST_DIR) /d 10"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"
	
t8:
	@echo "T8: 100 dirs create + remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 100"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 100"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	$(LWEXT4_CLIENT) -c "multi_dremove $(TEST_DIR) /d 100"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"

t9:
	@echo "T9: 1000 dirs create + remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 1000"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 1000"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	$(LWEXT4_CLIENT) -c "multi_dremove $(TEST_DIR) /d 1000"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t10:
	@echo "T10: 10 entries (dir) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 10"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 10"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t11:
	@echo "T11: 100 entries (dir) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 100"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 100"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	

t12:
	@echo "T12: 1000 entries (dir) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 1000"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 1000"
	$(LWEXT4_CLIENT) -c "dir_close 0"

	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t13:
	@echo "T13: 10 entries (files) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 10"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 10 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 10 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 10"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t14:
	@echo "T14: 100 entries (files) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 100"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 100 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 100 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 100"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t15:
	@echo "T15: 1000 entries (files) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 1000"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 1000 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 1000 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_entry_get 0 1000"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	


t16:	
	@echo "T16: 8kB file write/read:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "fopen 0 $(TEST_DIR)/test.txt wb+"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 0"
	
	$(LWEXT4_CLIENT) -c "fwrite 0 0 8192 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 8192"
	$(LWEXT4_CLIENT) -c "fsize 0 8192"
	
	$(LWEXT4_CLIENT) -c "fseek 0 0 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 8192"
	
	$(LWEXT4_CLIENT) -c "fread 0 0  8192 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 8192"
	$(LWEXT4_CLIENT) -c "fsize 0 8192"
	
	$(LWEXT4_CLIENT) -c "fclose 0"
	$(LWEXT4_CLIENT) -c "fremove $(TEST_DIR)/test.txt"	
		
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"		
	
t17:	
	@echo "T17: 64kB file write/read:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "fopen 0 $(TEST_DIR)/test.txt wb+"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 0"
	
	$(LWEXT4_CLIENT) -c "fwrite 0 0 65536 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 65536"
	$(LWEXT4_CLIENT) -c "fsize 0 65536"
	
	$(LWEXT4_CLIENT) -c "fseek 0 0 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 65536"
	
	$(LWEXT4_CLIENT) -c "fread 0 0  65536 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 65536"
	$(LWEXT4_CLIENT) -c "fsize 0 65536"
	
	$(LWEXT4_CLIENT) -c "fclose 0"
	$(LWEXT4_CLIENT) -c "fremove $(TEST_DIR)/test.txt"	
		
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t18:	
	@echo "T18: 512kB file write/read:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "fopen 0 $(TEST_DIR)/test.txt wb+"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 0"
	
	$(LWEXT4_CLIENT) -c "fwrite 0 0 524288 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 524288"
	$(LWEXT4_CLIENT) -c "fsize 0 524288"
	
	$(LWEXT4_CLIENT) -c "fseek 0 0 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 524288"
	
	$(LWEXT4_CLIENT) -c "fread 0 0  524288 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 524288"
	$(LWEXT4_CLIENT) -c "fsize 0 524288"
	
	$(LWEXT4_CLIENT) -c "fclose 0"
	$(LWEXT4_CLIENT) -c "fremove $(TEST_DIR)/test.txt"	
		
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t19:	
	@echo "T19: 4MB file write/read:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "fopen 0 $(TEST_DIR)/test.txt wb+"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 0"
	
	$(LWEXT4_CLIENT) -c "fwrite 0 0 4194304 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 4194304"
	$(LWEXT4_CLIENT) -c "fsize 0 4194304"
	
	$(LWEXT4_CLIENT) -c "fseek 0 0 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 4194304"
	
	$(LWEXT4_CLIENT) -c "fread 0 0  4194304 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 4194304"
	$(LWEXT4_CLIENT) -c "fsize 0 4194304"
	
	$(LWEXT4_CLIENT) -c "fclose 0"
	$(LWEXT4_CLIENT) -c "fremove $(TEST_DIR)/test.txt"	
		
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t20:	
	@echo "T20: 32MB file write/read:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "fopen 0 $(TEST_DIR)/test.txt wb+"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 0"
	
	$(LWEXT4_CLIENT) -c "fwrite 0 0 33554432 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 33554432"
	$(LWEXT4_CLIENT) -c "fsize 0 33554432"
	
	$(LWEXT4_CLIENT) -c "fseek 0 0 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 33554432"
	
	$(LWEXT4_CLIENT) -c "fread 0 0  33554432 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 33554432"
	$(LWEXT4_CLIENT) -c "fsize 0 33554432"
	
	$(LWEXT4_CLIENT) -c "fclose 0"
	$(LWEXT4_CLIENT) -c "fremove $(TEST_DIR)/test.txt"	

	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t21:	
	@echo "T21: 128MB file write/read:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "fopen 0 $(TEST_DIR)/test.txt wb+"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 0"
	
	$(LWEXT4_CLIENT) -c "fwrite 0 0 134217728 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 134217728"
	$(LWEXT4_CLIENT) -c "fsize 0 134217728"
	
	$(LWEXT4_CLIENT) -c "fseek 0 0 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 134217728"
	
	$(LWEXT4_CLIENT) -c "fread 0 0  134217728 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 134217728"
	$(LWEXT4_CLIENT) -c "fsize 0 134217728"
	
	$(LWEXT4_CLIENT) -c "fclose 0"

	$(LWEXT4_CLIENT) -c "fremove $(TEST_DIR)/test.txt"	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t22:	
	@echo "T22: 512MB file write/read:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "fopen 0 $(TEST_DIR)/test.txt wb+"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 0"
	
	$(LWEXT4_CLIENT) -c "fwrite 0 0 536870912 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 536870912"
	$(LWEXT4_CLIENT) -c "fsize 0 536870912"
	
	$(LWEXT4_CLIENT) -c "fseek 0 0 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 0"
	$(LWEXT4_CLIENT) -c "fsize 0 536870912"
	
	$(LWEXT4_CLIENT) -c "fread 0 0  536870912 0"
	
	$(LWEXT4_CLIENT) -c "ftell 0 536870912"
	$(LWEXT4_CLIENT) -c "fsize 0 536870912"
	
	$(LWEXT4_CLIENT) -c "fclose 0"
	
	$(LWEXT4_CLIENT) -c "fremove $(TEST_DIR)/test.txt"	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t23:
	@echo "T23: 10000 entries (files) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 10000"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 10000 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 10000 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	

t24:
	@echo "T24: 50000 entries (files) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_fcreate $(TEST_DIR) /f 50000"
	$(LWEXT4_CLIENT) -c "multi_fwrite $(TEST_DIR) /f 50000 1024"
	$(LWEXT4_CLIENT) -c "multi_fread $(TEST_DIR) /f 50000 1024"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
	
t25:
	@echo "T25: 10000 entries (dir) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 10000"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	
t26:
	@echo "T26: 50000 entries (dir) dir recursive remove:"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "stats_save /"
	$(LWEXT4_CLIENT) -c "dir_mk $(TEST_DIR)"
	
	$(LWEXT4_CLIENT) -c "multi_dcreate $(TEST_DIR) /d 50000"
	$(LWEXT4_CLIENT) -c "dir_open 0 $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "dir_close 0"
	
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "stats_check /"
	$(LWEXT4_CLIENT) -c "umount /"	
	

ct:
	@echo "Clean test directory"
	$(LWEXT4_CLIENT) -c "device_register 0 0 bdev"
	$(LWEXT4_CLIENT) -c "mount bdev /"
	$(LWEXT4_CLIENT) -c "dir_rm $(TEST_DIR)"
	$(LWEXT4_CLIENT) -c "umount /"	

server_ext2:
	$(LWEXT4_SERVER) -i ext_images/ext2

server_ext3:
	$(LWEXT4_SERVER) -i ext_images/ext3
	
server_ext4:
	$(LWEXT4_SERVER) -i ext_images/ext4
	
server_kill:
	-killall lwext4-server

fsck_images:
	sudo fsck.ext2 ext_images/ext2 -v -f
	sudo fsck.ext3 ext_images/ext3 -v -f
	sudo fsck.ext4 ext_images/ext4 -v -f

images_small:
	rm -rf ext_images
	mkdir ext_images
	dd if=/dev/zero of=ext_images/ext2 bs=1M count=128
	dd if=/dev/zero of=ext_images/ext3 bs=1M count=128
	dd if=/dev/zero of=ext_images/ext4 bs=1M count=128
	sudo mkfs.ext2 ext_images/ext2
	sudo mkfs.ext3 ext_images/ext3
	sudo mkfs.ext4 ext_images/ext4

images_big:
	rm -rf ext_images
	mkdir ext_images
	dd if=/dev/zero of=ext_images/ext2 bs=1M count=1024
	dd if=/dev/zero of=ext_images/ext3 bs=1M count=1024
	dd if=/dev/zero of=ext_images/ext4 bs=1M count=1024
	sudo mkfs.ext2 ext_images/ext2
	sudo mkfs.ext3 ext_images/ext3
	sudo mkfs.ext4 ext_images/ext4
	
test_set_small: t0 t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 t18 t19 t20
test_set_full: t0 t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 t18 t19 t20 t21 t22 t23 t24 t25 t26

test_ext2_full:
	make server_kill
	$(LWEXT4_SERVER) -i ext_images/ext2 &
	sleep 1
	make test_set_full
	make server_kill


test_ext3_full:
	make server_kill
	$(LWEXT4_SERVER) -i ext_images/ext3 &
	sleep 1
	make test_set_full
	make server_kill

test_ext4_full:
	make server_kill
	$(LWEXT4_SERVER) -i ext_images/ext4 &
	sleep 1
	make test_set_full
	make server_kill

test_all: images_big test_ext2_full test_ext3_full test_ext4_full fsck_images


test_ext2_small:
	make server_kill
	$(LWEXT4_SERVER) -i ext_images/ext2 &
	sleep 1
	make test_set_small
	make server_kill


test_ext3_small:
	make server_kill
	$(LWEXT4_SERVER) -i ext_images/ext3 &
	sleep 1
	make test_set_small
	make server_kill

test_ext4_small:
	make server_kill
	$(LWEXT4_SERVER) -i ext_images/ext4 &
	sleep 1
	make test_set_small
	make server_kill
	
test: images_small test_ext2_small test_ext3_small test_ext4_small
	
	





