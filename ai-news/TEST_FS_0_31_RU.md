# Проверки MultiOS 0.31-dev-writeops

Результаты относятся к перечисленным сценариям, а не к полной совместимости ФС или сертификации реального оборудования. Окончательные журналы находятся в reports/ поставки.

## Воспроизведение прежней ошибки

На неизменённых файловых адаптерах 0.30 VFS mode 4 (атомарное копирование) возвращал READONLY для обычного файла с атрибутом ReadOnly на каждой из FAT32/exFAT/NTFS. После исправления проверки повторены с источником 8193 байта, ReadOnly + System.

Отдельная проверка точного написания имён обнаружила принудительное приведение NTFS listing к нижнему регистру: upstream ntfs_readdir делал это при ignore-case mount. Теперь только перечисление в одноразовом read-only volume использует case-preserving режим. Поиск и разрешённые мутации сохраняют прежнюю политику сравнения имён. Нельзя считать это полной поддержкой case-sensitive NTFS-каталогов.

## Положительные и защитные сценарии

`tests/fs_writeops.py` создаёт обычные расходные копии образов, используя настоящий HAL/VFS и текущие файловые библиотеки. Атрибуты источника задаются независимыми fixture-средствами: mtools для FAT32, системной libntfs-3g для NTFS и пересчётом exFAT entry-set checksum в тестовом fixture.

На всех трёх ФС, ordinary и ASan/UBSan:

- Атомарная копия защищённого источника с точной проверкой всех 8193 байтов.
- Отдельная запись в копию через границу сектора; исходные данные и ReadOnly/System остаются прежними.
- Отказ replace/append/write-at/resize/delete/rename защищённого файла.
- Отказ перезаписи защищённого существующего назначения; допустимы READONLY или EXISTS в зависимости от контракта адаптера, но не успешная запись.
- Переименование файла и непустого каталога с изменением регистра; проверка точного написания имён, а не только успешного возврата.
- Сохранность дочернего файла, отказ при занятом имени назначения и отказ удаления непустого каталога.
- Независимые fsck.fat -n, fsck.exfat -n и ntfsfix -n. FAT32/NTFS также прочитаны mtype/ntfscat с побайтовым сравнением. Для exFAT независимая утилита проверяет структуру; содержимое проверяет production VFS, это не независимый exFAT DATA reader.

## Новые аварийные матрицы

`tests/writeops_fault.py` использует общий persistence/undo model из storage_tx_test.c с отдельным inventory операций: копирование ReadOnly/System источника в корень и подкаталог, case-only rename файла и непустого каталога, обычная копия, append, write-at и resize с zero-fill.

Каждая ФС проверяется ordinary и sanitizer-сборкой. Проверяются write/flush границы с crash и I/O failure, torn/partial/reordered persistence, последующее восстановление, запрет записи до восстановления и точное старое либо завершённое новое состояние секторов вне журнала. Отдельно проверяются прерывания восстановления. Размеры матриц приведены в журналах; повтор ordinary/sanitizer не удваивает число уникальных сценариев.

Для побайтового сравнения NTFS метаданных модель использует существующий `MULTIOS_NTFS_TEST_TIME=1`: время фиксировано только в тестовом процессе. Иначе два корректных завершения одной операции могли бы отличаться timestamps и давать ложный отказ сравнения. Время реальной ОС этим не фиксируется. Модель не заменяет физическое отключение питания.

## Общие регрессии

Повторяются общие FAT32/exFAT/NTFS transaction matrices, NTFS compressed fault matrix, почти заполненные тома всех трёх ФС и active FAT0/FAT1/single-FAT регрессии 0.30. Проверка active FAT независимыми инструментами использует временную проекцию выбранной таблицы, а не ремонт исходного тестируемого тома; подробнее в TEST_FS_0_30_RU.md.

## BIOS и UEFI

Оба бинарника собраны из текущего кода.

- Новые операции: каждая из FAT32/exFAT/NTFS в BIOS/ATA (QEMU 486) и UEFI/firmware (QEMU qemu64 + OVMF). Реальные команды copyatomic, cmp, writeat, ren, dir и чтение дочернего файла; после остановки — точное чтение исходника и копии, независимые проверки структуры.
- Общая загрузочная регрессия: UEFI всех трёх ФС, обычные create/append/copy/cmp/resize/write-at/delete/sync/lock/poweroff; BIOS/EHCI USB-only boot, extended write suite, reboot persistence и неизменённый добавленный хвост носителя. Для NTFS extended suite включает ADS/hardlink/COPYADS.
- UEFI использует firmware Block I/O и сохраняет Boot Services. Emulated xHCI в этом тесте — функция firmware, не собственный драйвер xHCI MultiOS.
- Повторён supplemental UEFI ISA audit. Это не формальное доказательство совместимости со всеми CPU.

Реальное оборудование, физическое извлечение/отключение питания, Windows chkdsk и новая полная UHCI/OHCI матрица в этой поставке не заявляются.

## Воспроизведение

Из каталога MultiOS после распаковки FULL-SOURCES рядом с images/:

```sh
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/multios-host build/fs-read-probe build/fs-writeops-probe build/fs-writeops-probe-sanitize build/writeops-tx-test build/writeops-tx-sanitize
python3 tests/fs_writeops.py ../images
python3 tests/writeops_fault.py
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/storage-tx-test build/storage-tx-sanitize build/fs-full-probe build/fs-full-probe-sanitize build/fat32-active-probe build/fat32-active-probe-sanitize
python3 tests/storage_tx.py
python3 tests/upstream_ntfs_fault.py
MULTIOS_TEST_COMPRESSED=1 python3 tests/upstream_ntfs_fault.py
python3 tests/fs_full.py ../images
python3 tests/fat32_active.py ../images/MultiOS-0.31-usb-fat32.img
make bios-rw
make FS_PROFILE=profiles/filesystems-ntfs3g.mk uefi
python3 tests/uefi_isa.py
python3 tests/fs_compat_boot.py ../images
```

Для отдельного writeops boot сначала установите текущие бинарники в новые копии build/writeops31/{fat32,exfat,ntfs}.img через tools/build_bios_rw_image.py с --efi build/BOOTX64.EFI. Передайте каталог новых копий в `python3 tests/writeops_boot.py КАТАЛОГ`. Для BIOS используется штатный ATA-профиль installer; для остальных тестов используются USB-образы поставки.

Host-тестам нужны dosfstools, mtools, exfatprogs, ntfs-3g, его shared library и sanitizer runtime. QEMU_BIOS/QEMU_UEFI выбирают эмулятор; BIOS_ROM/QEMU_ROM_DIR/VGA_ROM и OVMF_CODE/OVMF_VARS — firmware.
