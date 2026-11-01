# vm2 studios | t.me/v2vproj — NTFS library objects separate from BIOS sources.
NTFS_FLAGS = -march=x86-64 -mtune=generic -fcf-protection=none -std=gnu11 -O2 -fpic -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -ffunction-sections -fdata-sections -DHAVE_CONFIG_H -D_GNU_SOURCE -DNO_NTFS_DEVICE_DEFAULT_IO_OPS -Iports/ntfs3g -Ivendor/ntfs-3g/include/ntfs-3g -include ports/ntfs3g/rename.h
build/ntfs3g-host/%.o: vendor/ntfs-3g/libntfs-3g/%.c ports/ntfs3g/config.h ports/ntfs3g/rename.h
	mkdir -p $(dir $@)
	$(CC) $(NTFS_FLAGS) -c $< -o $@
build/ntfs3g-host/%.o: ports/ntfs3g/%.c ports/ntfs3g/config.h
	mkdir -p $(dir $@)
	$(CC) $(NTFS_FLAGS) -c $< -o $@
build/ntfs3g-efi/%.o: vendor/ntfs-3g/libntfs-3g/%.c ports/ntfs3g/config.h ports/ntfs3g/rename.h
	mkdir -p $(dir $@)
	$(CC) $(NTFS_FLAGS) -DMPNTFS_FIRMWARE -c $< -o $@
build/ntfs3g-efi/%.o: ports/ntfs3g/%.c ports/ntfs3g/config.h
	mkdir -p $(dir $@)
	$(CC) $(NTFS_FLAGS) -DMPNTFS_FIRMWARE -I$(EFIINC) -I$(EFIINC)/x86_64 -DEFI_FUNCTION_WRAPPER -c $< -o $@
# Allocator must call the underlying libc/firmware pools, not rename itself.
build/ntfs3g-host/allocator.o: ports/ntfs3g/allocator.c
	mkdir -p $(dir $@)
	$(CC) -march=x86-64 -mtune=generic -fcf-protection=none -std=c11 -O2 -ffunction-sections -fdata-sections -c $< -o $@
build/ntfs3g-efi/allocator.o: ports/ntfs3g/allocator.c
	mkdir -p $(dir $@)
	$(CC) -march=x86-64 -mtune=generic -fcf-protection=none -std=c11 -O2 -fpic -ffreestanding -mno-red-zone -fno-stack-protector -DMPNTFS_FIRMWARE -I$(EFIINC) -I$(EFIINC)/x86_64 -DEFI_FUNCTION_WRAPPER -c $< -o $@

# Separate objects: sanitizer tests must instrument the library, not only VFS.
NTFS_HOST_SAN_OBJ = $(patsubst build/ntfs3g-host/%,build/ntfs3g-san/%,$(NTFS_HOST_OBJ))
NTFS_SAN_FLAGS = -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie
build/ntfs3g-san/%.o: vendor/ntfs-3g/libntfs-3g/%.c ports/ntfs3g/config.h ports/ntfs3g/rename.h
	mkdir -p $(dir $@)
	$(CC) $(NTFS_FLAGS) $(NTFS_SAN_FLAGS) -c $< -o $@
build/ntfs3g-san/%.o: ports/ntfs3g/%.c ports/ntfs3g/config.h
	mkdir -p $(dir $@)
	$(CC) $(NTFS_FLAGS) $(NTFS_SAN_FLAGS) -c $< -o $@
build/ntfs3g-san/allocator.o: ports/ntfs3g/allocator.c
	mkdir -p $(dir $@)
	$(CC) -march=x86-64 -mtune=generic -fcf-protection=none -std=c11 -ffunction-sections -fdata-sections $(NTFS_SAN_FLAGS) -c $< -o $@

$(NTFS_HOST_OBJ) $(NTFS_HOST_SAN_OBJ) $(NTFS_EOBJ): ports/ntfs3g/build.mk
