> Историческая инструкция. Актуальный файловый ABI 3 и Unicode-пути описаны в FILESYSTEMS_UNICODE_RU.md. NTFS остаётся read-only.

> **Обновление 0.15-dev-fs64:** файловые модули теперь ABI 2; добавлен отдельный 64-битный интерфейс чтения. Текущие возможности, новые команды и ограничения описаны в `FILESYSTEMS_64_RU.md`. Ниже сохранено описание прежнего этапа/legacy API. NTFS RW не появилась.

# MultiOS 0.11-dev — FAT32, NTFS и общий VFS

**MultiPulti · vm2 studios | t.me/v2vproj · UNPUBLISHED DEVELOPMENT**

Это промежуточная dev-версия. Запрос на полноценную поддержку NTFS **ещё не выполнен**:
NTFS реализован только для чтения. Никаких обещаний стабильного Release 1.0.

## Матрица возможностей

| Возможность | exFAT | FAT32 / VFAT | NTFS 3.0/3.1 |
|---|---|---|---|
| DIR, TREE, CD, TYPE, MORE, HEX, STAT, CRC32, CMP, FIND, SOURCE | Да, поддержанный subset | Да, поддержанный subset | Да, поддержанный subset |
| DF, подсчёт свободных кластеров | Bitmap | FAT, без доверия FSInfo hints | $Bitmap |
| TOUCH / WRITE / APPEND / COPY | Экспериментально | Экспериментально | **Нет** |
| MD / RD / DEL / REN | Экспериментально | Экспериментально | **Нет** |
| Имена | ASCII lookup/write | ASCII LFN и 8.3 aliases; Unicode показывается с ? | ASCII lookup; Unicode показывается с ? |
| Выделение / освобождение / расширение каталогов | См. существующий драйвер | Да, включая рост root и подкаталогов | Только чтение существующих данных |
| Фрагментированные файлы | Да | FAT chains | Nonresident runlists, до 32 runs |
| Sparse / неинициализированные данные | VDL zero fill | Нет sparse-формата | Да, holes и InitializedSize zero fill |
| Resident data / hardlinks / вложенные каталоги | Не NTFS | Не NTFS | Да; DOS namespace aliases не дублируются в DIR |
| Журнал / восстановление после питания | Нет | Нет | Нет replay; запись запрещена |

Запись FAT32 не является «полной совместимостью с Windows» или сертифицированной
реализацией VFAT. Нет Unicode/OEM codepages, сохранения всех временных меток при
редактировании, ACL, форматирования из консоли или гарантии power-loss safety.
При записи используются ASCII LFN и генерируемые уникальные 8.3 aliases.

## Команды выбора файловой системы

```text
FS
MOUNT AUTO
MOUNT RAW
MOUNT 1
MOUNT 2
MOUNT 3
MOUNT 4
```

- Один активный том; несколько одновременных mount points пока не реализованы.
- AUTO: узнаёт raw/superfloppy; для MBR предпочитает type 07 data (exFAT/NTFS),
  затем FAT32. На штатном образе это data-раздел, не загрузочный ESP.
- 1..4 — номер primary MBR partition; extended partitions и GPT не поддержаны.
- RAW — файловая система непосредственно в LBA 0.
- Перед переключением выполняется flush. Ошибка/poisoned mount не обходятся
  через MOUNT; нужен перезапуск и внешняя проверка.
- После переключения режим RO, путь сбрасывается в `/`. При неудаче чтения/проверки
  нового тома старый том уже не смонтирован; это не автоматический fallback.
- MOUNT не выбирает другой физический диск и ничего не форматирует.
- ESP доступен через MOUNT 1, но не используй shell-запись для обновления boot
  bundle: она не синхронизирует raw mirror ядра. Для этого предназначен imagectl.

`STAT` для NTFS выводит номер MFT record, а не ошибочно называемый кластер.
`FS` явно сообщает, что NTFS backend не поддерживает запись.

## FAT32: границы и защита записи

512-byte logical sectors, cluster 1..128 sectors; именно FAT32 определяется
геометрией и количеством кластеров (>=65525), не только текстовой меткой.
1 или 2 FAT; чтение active FAT допускается, но запись — только при mirroring.
Поддержаны обычные FAT32 chains, LFN checksum/ordinal/padding, короткие имена и
lowercase flags, атрибуты read-only/hidden/system/archive и dot entries каталогов.

Перед `UNLOCK YES`: не более **1048576 кластеров**, **16 уровней каталогов** и
**32768 directory records**. Проверяются FAT mirrors, backup VBR, FSInfo signatures,
цепочки, кластеры с несколькими владельцами, dot entries, ASCII write subset.
Непривязанные выделенные кластеры не освобождаются. Это не полный fsck;
полный поиск duplicate names и проверка каждой семантики метаданных не реализованы.

Мутация: dirty bit в FAT[1] и flush до изменения данных; все FAT copies обновляются;
FSInfo free/next hints делаются unknown; clean bit возвращается лишь после flush.
После ошибки mount poisoned/RO. Это **не журнал** и не атомарность при потере питания.
Dirty/hard-error/несогласованный том не разблокируется и не ремонтируется автоматически.

Пример — только на ненужной копии:

```text
UNLOCK YES
MD DOCS
WRITE "/DOCS/Long file name.txt" "Hello FAT32"
APPEND "/DOCS/Long file name.txt" "Second line"
COPY "/DOCS/Long file name.txt" /DOCS/backup.txt
REN /DOCS/backup.txt "renamed file.txt"
TYPE "/DOCS/renamed file.txt"
DEL "/DOCS/renamed file.txt"
LOCK
```

## NTFS: что действительно реализовано

Собственный freestanding C reader, без libntfs-3g/Linux в ОС. 512-byte logical
sectors; MFT records 512..4096 bytes; bootstrap $MFT, USA fixups, resident/nonresident
unnamed $DATA, signed relative mapping pairs, sparse holes, InitializedSize.
Каталоги перечисляются **сканированием MFT**, а не через $I30 B-tree: медленнее
на больших томах и не полная проверка согласованности индекса. До **65536 MFT
records**, до **32 runs** на поддержанный поток; split $ATTRIBUTE_LIST не читается.

Внутренние records 0..15 скрыты из DIR. Это сознательное ограничение интерфейса;
MFTMirr/журнал не используются для автоматического восстановления.

Не поддержаны: NTFS writes/transactions/$LogFile replay, compression LZNT1, EFS,
reparse traversal/symlinks/junctions, ADS API, ACL enforcement, Unicode lookup/
NTFS $UpCase collation, attribute-list/extension streams, BitLocker, повреждённые
USA records и размеры за пределами 32-bit API. Неподдержанные файлы могут быть
перечислены, но открытие возвращает unsupported; чтение не превращается в запись.

**UNLOCK YES на NTFS всегда запрещён**, даже если block driver умеет писать.
Для полноценной NTFS RW ещё нужны отдельная реализация изменения MFT/индексов/
bitmap/security, журналирование/восстановление и большая независимая тестовая кампания.

## Один редактируемый загрузочный образ

В поставке один `build/MultiOS-MultiPulti.img`: FAT32 ESP + exFAT data по умолчанию.
Оба новых backend встроены и в BIOS kernel, и в x64 UEFI app. При необходимости
создай **новый** образ с другой data filesystem (Linux/WSL):

```bash
# Поставляемых boot.bin/kernel.bin/BOOTX64.EFI достаточно: компилятор не нужен.
# После правки C-исходников сначала: make all uefi multipulti
python3 tools/mkunified.py --data-fs fat32 --output build/FAT32-new.img
python3 tools/mkunified.py --data-fs ntfs --output build/NTFS-new.img
```

Нужны Python 3, mtools, dosfstools; для создания NTFS — mkntfs и ntfscp (ntfsprogs).
ОС сама от этих программ не зависит. Скрипт отказывается перезаписывать destination;
он работает с новым обычным файлом, не форматирует физические диски.

```bash
MULTIOS_IMAGE=build/FAT32-new.img ./run-unified.sh bios
MULTIOS_IMAGE=build/NTFS-new.img ./run-unified.sh uefi
```

Формат boot bundle обновлён: 240 raw kernel sectors, максимум **120 KiB без BSS**,
marker MP11. ESP/data offsets сохранены. Старый 0.10 boot loader читает только
120 секторов и **не подходит** новому ядру. Не смешивай старый MBR с новым kernel;
собери новый образ. imagectl 0.11 отказывается редактировать старый boot bundle.
BIOS всё ещё требует EDD и IDE primary master; UEFI сохраняет Boot Services.

## Проверки

```bash
make test-filesystems
make test-filesystems-sanitize
make test-filesystems-faults
QEMU=/path/to/pc-capable-qemu make test-filesystems-boot
```

Fixture tools mkfs.fat / mkntfs / ntfscp / ntfscat / ntfsinfo проверяют независимость
формата. Дополнительный test-only Python helper использует установленную libntfs-3g
для генерации каталогов/hardlinks/sparse; библиотека не поставляется и не включена
в ОС. Все тесты выполняются на обычных ненужных файлах; физическое оборудование
и аварийное отключение питания не сертифицированы.
