# MultiOS 0.11-dev — отчёт по добавлению файловых систем

**MultiPulti · vm2 studios | t.me/v2vproj · UNPUBLISHED DEVELOPMENT**

## Изменения

1. Общий VFS: shell/path lookup больше не привязаны к exFAT.
2. Автоопределение exFAT/FAT32/NTFS по VBR с последующей проверкой backend.
3. Выбор primary MBR partitions и raw/superfloppy через MOUNT; FS показывает тип.
4. Data-раздел FAT32 предпочитается ESP при AUTO; сохранён приоритет type 07.
5. FAT32 geometry, chain traversal, active FAT и кластерные границы.
6. VFAT LFN ordinal/checksum/padding, короткие имена, lowercase flags и 8.3 alias lookup.
7. FAT32 чтение файлов и всех применимых команд консоли через существующий API.
8. FAT32 создание, замена, append, copy, rename, delete; readonly attributes.
9. FAT32 создание/удаление каталогов, dot entries, расширение root/subdirectories.
10. FAT32 bounded write preflight: ownership/crosslinks/chains/FAT mirrors/FSInfo/backup.
11. FAT32 dirty/clean protocol и отказ последующих writes после I/O/flush ошибок.
12. NTFS 3.x boot/$MFT bootstrap и USA fixups; структура записей/атрибутов.
13. NTFS resident и nonresident unnamed data, относительные runlists, sparse и VDL.
14. NTFS nested directories и hardlinks через bounded MFT scan; $Bitmap для DF.
15. NTFS всегда RO; неподдержанные EFS/compression/attribute lists/reparse отвергаются.
16. Исправлены общие сообщения ошибок и STAT: MFT record вместо «кластера» NTFS.
17. Новые backend включены в BIOS i386 kernel и x64 UEFI firmware app.
18. Boot bundle MP11: 240 секторов/120 KiB без BSS, проверка несовместимого старого MBR.
19. Builder создаёт новый unified image с exFAT/FAT32/NTFS data по выбору.
20. imagectl понимает все три data signatures; прежние transactional/ESP safeguards сохранены.
21. Добавлены независимые format/content tests, повреждённые fixtures, sanitizers,
    mutation/fault tests, sparse 8 GiB FAT32 image, BIOS/UEFI read/write tests.
22. Полная инструкция и матрица ограничений в FILESYSTEMS_RU.md; брендинг сохранён.

## Отчёты

- `build/dev11-filesystems-report.txt`: основной новый black-box suite.
- `build/dev11-filesystems-sanitize-report.txt`: те же сценарии ASan/UBSan.
- `build/dev11-filesystems-fuzz-report.txt`: 160 детерминированных bit mutations.
- `build/dev11-fat32-fault-report.txt`: инъекции ошибок записи/flush, запрет обхода MOUNT.
- `build/dev11-fat32-large-report.txt`: sparse 8 GiB FAT32 ordinary file, не физический диск.
- `build/dev11-filesystems-boot-report.txt`: FAT32/NTFS в BIOS и UEFI.
- Старые exFAT и platform regression suites запускаются повторно; свежие отчёты в build.

**Не называем это полноценным NTFS RW.** Он не реализован. FAT32 RW тоже остаётся
экспериментальным subset с ASCII writes и ограниченным preflight. Полная матрица
и оставшиеся задачи — FILESYSTEMS_RU.md. Физические диски/оборудование, power loss,
все процессоры и все допустимые/недопустимые тома не сертифицированы.
