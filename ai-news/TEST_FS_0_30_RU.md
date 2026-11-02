# Проверки MultiOS 0.30-dev-fat32active

Это результаты перечисленных сценариев, не подтверждение полной FAT32/exFAT/NTFS совместимости или стабильности на реальном железе.

## Воспроизведённая несовместимость

На коде 0.29 новый реальный FAT32 probe завершался ошибкой чтения многокластерного файла при выбранной FAT1. Проверка записи nonmirrored FAT32 также падала на unlock. После изменения parser/audit и FatFs bridge оба сценария проходят.

## Новые host-проверки

`tests/fat32_active.py` создаёт расходные копии заводского образа. Используются FAT0 active, FAT1 active и одна физическая FAT. Неактивная таблица в двухтабличных случаях намеренно заполнена устаревшими данными.

Обычная и ASan/UBSan-сборки проверяют:

- Чтение 8193-байтового файла через цепочку кластеров и free-space query без записей.
- Create/append/copy/rename, запись через границу сектора, resize/shrink/regrow с нулями, Unicode, каталоги и remount.
- HAL write trap: ни одной записи в неактивную FAT; полное побайтовое сравнение её области после операций.
- Неправильный active index, резервные флаги, несовпадающие backup boot sector и зеркальные FAT: отказ без изменений.
- Независимое чтение точных байтов и fsck.fat -n на **проверочной проекции выбранной FAT**: выбранная таблица копируется в оба слота отдельного временного partition-файла, флаг зеркалирования включается. Сам тестируемый том не ремонтируется. Это проверка выбранного логического состояния, не заявление о том, что все внешние утилиты понимают nonmirrored FAT32.

Для FAT0 и FAT1 отдельно выполнен существующий real-FS transaction fault matrix, в обычной и sanitizer-сборках. На каждую сборку/выбор таблицы: **5 036 876 assertions, 1140 crash cases, 1140 I/O cases, 73 прерывания восстановления**. Разные persistence-модели включают torn/partial/reordered данные; это ограниченные модели, не физический power-cut.

Новый host probe повторён сначала с FatFs-профилем, затем с полным NTFS-3G/FatFs-профилем. При повторе того же набора число уникальных сценариев не увеличивается.

## Общие регрессии

- Повторены ordinary/ASan/UBSan transaction matrices для обычных зеркальных FAT32, exFAT и NTFS; журналы в reports. FAT32/exFAT дополнительно: 96 seeded операций, 12 независимых fsck checkpoints на ФС.
- Повторены almost-full volume tests всех трёх ФС, обычные и ASan/UBSan: отказ create/append/copy/FAT resize либо NTFS sparse materialization; rollback всех секторов вне изменяемого журнала и locator footer, сохранность данных и remount.
- Ранее добавленные FatFs first-error и VFS unlock regression units повторены. Этот патч не меняет реализацию NTFS; пересборка UEFI включает текущие исправления 0.29.

## BIOS и UEFI

Оба бинарника пересобраны из текущего кода: BIOS i386 kernel.bin — 281448 байт; UEFI x64 BOOTX64.EFI — 1244790 байт.

- UEFI: FAT32/exFAT/NTFS, QEMU qemu64 + OVMF, emulated xHCI USB storage. Использован заводской BOOT.CFG, не специальный урезанный файловый драйвер. Проверены create/append/copy/cmp/resize/write-at/delete/sync/lock/poweroff и независимое чтение файла после остановки.
- BIOS: все три ФС на собственном EHCI/BOT, USB-only boot без IDE, extended write suite, повторная загрузка, неизменённый добавленный хвост носителя и независимая проверка сохранённых байтов. Для NTFS также ADS/hardlink/COPYADS из существующего extended suite.
- Отдельно active FAT0 и FAT1 на BIOS/ATA и UEFI/firmware: чтение через границу сектора и запись, неактивная FAT побайтово неизменна, независимый selected-FAT projection check.
- Supplemental UEFI ISA audit не обнаружил проверяемые post-SSE2 инструкции; одновременно выполнен runtime qemu64 test. Это не формальная верификация ISA.

UEFI сохраняет Boot Services; firmware xHCI не является собственным xHCI-драйвером MultiOS. Реальное железо, Windows chkdsk, физическое извлечение/отключение питания не проверены. Старые UHCI/OHCI результаты 0.29 остаются историческими: их новая полная матрица в 0.30 не заявляется.

## Воспроизведение основных проверок

Из распакованного FULL-SOURCES, каталог MultiOS:

```sh
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/multios-host build/fat32-active-probe build/fat32-active-probe-sanitize build/storage-tx-test build/storage-tx-sanitize
python3 tests/fat32_active.py ../images/MultiOS-0.30-usb-fat32.img --faults
python3 tests/storage_tx.py
python3 tests/upstream_ntfs_fault.py
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/fs-full-probe build/fs-full-probe-sanitize
python3 tests/fs_full.py ../images
make bios-rw
make FS_PROFILE=profiles/filesystems-ntfs3g.mk uefi
python3 tests/uefi_isa.py
python3 tests/usb_storage_qemu.py ../images/MultiOS-0.30-usb-fat32.img qemu-system-i386 ehci --usb-boot --reboot --extended --padded --keep-disk
```

Для общей проверки обеих загрузок используйте `python3 tests/fs_compat_boot.py ../images`. Опциональный `--active-dir` принимает каталог с active0.img и active1.img, подготовленными через build_bios_rw_image.py из fixtures нового FAT32-теста. Путь QEMU задаётся QEMU_BIOS/QEMU_UEFI; BIOS_ROM/QEMU_ROM_DIR/VGA_ROM и OVMF_CODE/OVMF_VARS настраивают firmware.

QEMU должен содержать нужные контроллеры и BIOS ROM. Требуются GCC/binutils/make/Python3, mtools/dosfstools, exfatprogs, ntfs-3g/ntfsprogs и sanitizer runtimes. Все операции выполняются на расходных обычных файлах, не на физических дисках. Подробные ограничения — FILESYSTEMS_0_30_RU.md. Версия не является Release 1.0.
