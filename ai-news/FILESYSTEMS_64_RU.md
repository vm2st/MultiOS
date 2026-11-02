> Исторический этап 0.15: текущие Unicode-пути и файловый ABI 3 описаны в FILESYSTEMS_UNICODE_RU.md. Возможности wide-read ниже сохранены; прежний ABI 2 уже не принимается.

# Общий 64-битный интерфейс чтения FAT32, exFAT и NTFS

**MultiOS 0.15-dev-fs64 / MultiPulti — vm2 studios | t.me/v2vproj**

НЕОПУБЛИКОВАННАЯ РАЗРАБОТКА. Это не Release 1.0 и не заявление о полной поддержке ФС. NTFS по-прежнему только чтение; запись FAT32/exFAT экспериментальная.

## Что изменилось в каждом слое

1. **VFS**: `MultiFile64`, `fs_open64` и `fs_read64` — отдельный 64-битный read-only путь. Смещение и DataLength/ValidDataLength не обрезаются до 32 бит. Старая запись не получает новых разрешений.
2. **Модульный ABI файловой системы 2**: обязательный callback `read64` у всех трёх модулей. Модули ABI 1 не принимаются. Все статические профили/бинарники нужно пересобирать вместе; динамического загрузчика драйверов не добавлено.
3. **NTFS**: DATA-экстенты собираются с 64-битными size/initialized; run reader принимает 64-битный offset. Sparse runs, resident DATA, extension records и списки атрибутов из предыдущего этапа сохранены. В новом API доступны проверенные 5-ГиБ sparse файлы на небольшом томе. `$Bitmap`/bootstrap всё ещё имеют прежние ограничения.
4. **exFAT**: полные DataLength/ValidDataLength из stream extension; wide-reader для NoFatChain и FAT chain. Часть после ValidDataLength обнуляется, даже если физические секторы содержат ненулевые байты. Проверяются размер относительно вместимости тома и весь диапазон contiguous-аллокации.
5. **FAT32**: тот же API работает с обычными файлами, но размер выше `0xffffffff` отвергается. Ограничение формата не выдаётся за «64-битные FAT32-файлы». Протестирован файл 4 ГиБ минус 64 КиБ на 8-ГиБ томе.
6. **Защита legacy API**: большие файлы получают `MULTI_STREAM_WIDE`; старое `fs_read` и legacy backend readers отказывают вместо чтения с усечёнными полями. Старые TYPE/CRC32/COPY не переведены на большие файлы; используйте новые команды диапазона.
7. **Срок действия вида файла**: mount epoch и kind. Любая попытка mount, а также допущенные PUT/REMOVE/RENAME инвалидируют прежние виды перед callback, включая последующий отказ/частичную ошибку. Переполнение epoch не переиспользует старые номера. Отклонённая до dispatch операция не меняет вид. Это не многопоточный файловый дескриптор и не security boundary: структуры предназначены доверенному коду ядра.
8. **Консоль**: STAT64, READAT, HEXAT, 64-битный вывод размера в DIR, десятичный и шестнадцатеричный разбор без silent wrap. Один диапазон ограничен 4096 байт. На i386 новые вычисления не требуют сторонней runtime-библиотеки 64-битного деления.
9. **Тесты**: независимый mkntfs/libntfs-3g sparse NTFS; mkfs.exfat + test-only serializer и независимый fsck.exfat; mkfs.fat + test-only FAT fixture и независимый fsck.fat. Проверяются contiguous/chained exFAT, initialized tail, stale views, EOF, переполнения и ABI. Большие тестовые образы разреженные и в архив не входят. Копирование загрузочных тестов сохраняет дыры; производственный код от этого Linux-инструмента не зависит.

## Использование

Для обычной загрузки и редактирования образа — `START_HERE_RU.md`. Готовая ОС находится в одном `build/MultiOS-MultiPulti.img`, с FAT32 ESP и FAT32 DATA. NTFS/exFAT примеры ниже требуют соответствующего тома с файлом; готовый образ не содержит многогигабайтный wide.bin.

```text
HELP
FS DRIVERS
STAT64 WELCOME.TXT
READAT WELCOME.TXT 0 64
HEXAT WELCOME.TXT 0 32
```

На тестовом NTFS/exFAT-томе:

```text
STAT64 wide.bin
READAT wide.bin 0xfffffff8 16
HEXAT wide.bin 0x100000000 32
READAT wide.bin 5368709227 16
```

Offset/count — без знака, десятичные либо `0x...`. Диапазон за EOF, отрицательные значения, переполнение uint64 и count >4096 отклоняются. EOF с count=0 допустим. READAT заменяет нетекстовые байты точкой, HEXAT показывает точные байты и 16-разрядный hex-адрес. Это просмотр, не редактор. Каталоги через fs_open64/STAT64 не открываются.

Для кода портирования:

```c
MultiFile64 file;
unsigned char bytes[512];
int r = fs_open64("/wide.bin", &file);
if (r == 0)
    r = fs_read64(&file, UINT64_C(0x100000000), bytes, sizeof bytes);
```

Не конструируйте MultiFile64 вручную и не меняйте size/valid/epoch. После remount или допущенной мутации откройте файл заново. Неверные диапазоны/устаревшие виды отклоняются до дискового чтения. При ошибке I/O в середине реального чтения буфер может быть частично заполнен: атомарность чтения и транзакция этим API не обещаются.

## Воспроизведение

```sh
make all uefi multipulti
make test-file64 test-file64-epoch
make build/multios-sanitize-host
python3 tests/file64_shell_test.py
QEMU=qemu-system-x86_64 python3 tests/file64_boot.py
make test-filesystems-sanitize test-filesystems-faults
make test-ntfs-meta test-block64 test-fs-registry test-ntfs-attr test-ntfs-attrlist test-ntfs-stream
```

Нужны Linux/WSL, C/Python, dosfstools, exfatprogs, ntfs-3g/ntfsprogs с development headers, ASan/UBSan; GNU-EFI/OVMF/QEMU для загрузочных тестов. BIOS требует QEMU с `machine pc`, UEFI — `QEMU_UEFI` с `q35`. На хосте нужны 64-битные файловые смещения и поддержка SEEK_DATA/SEEK_HOLE для sparse-утилит тестов. Тесты не должны запускаться на физических дисках.

## Результаты текущего прогона

- По 91 offset/bounds комбинации на FAT32, contiguous exFAT, chained exFAT, initialized-tail exFAT и sparse NTFS: **455 случаев в каждой из normal и ASan/UBSan сборок**. Дополнительно EOF/null/stale/formatting/legacy-refusal проверки.
- 44 консольные проверки в normal + ASan/UBSan; sparse источники не изменились.
- Mock VFS проверил инвалидирование после успешных и ошибочных PUT/REMOVE/RENAME и отсутствие dispatch у запрещённой операции.
- BIOS i386/486 и UEFI x64/OVMF: новый API на обычном FAT32-файле и чтение >4 ГиБ на 5-ГиБ exFAT/NTFS-файлах. Образы после read-only испытаний побайтово не изменились; это не тест реального железа.
- Сохранены регрессии FAT32/NTFS (164 assertions, 160 mutation runs), FAT32 large-volume/fault/model, exFAT read/write/edge/WRITE+FLUSH fault, прежние NTFS attribute-list/runlist/USA и portability/audit тесты. Точные выводы — в свежих журналах; не складывать разные виды проверок в общее «число доказанных свойств».

Отчёты этого этапа: `build/fs64-*`; старые NTFS_STAGE1/2 и журналы прошлых сборок — история, не проверка этих бинарников. Точная граница FAT32 `4 ГиБ−1` не засчитана данным набором: первоначальный предельный fixture не получил чистой независимой проверки fsck.fat 4.2. Подтверждённый большой FAT32 fixture — `4 ГиБ−64 КиБ`; это намеренно не скрыто общим словом «полностью».

## Что ещё НЕ сделано

- **Это не полный 64-bit HAL/VFS**: глобальный блочный HAL и адресация тома остаются 32-битными, аппаратные ограничения прежние. Проверенные 5-ГиБ файлы находятся на томах до 8 ГиБ; диски больше 128 ГиБ, GPT и 4Kn не включены.
- Нет wide write API. Создание/копирование/изменение больших файлов средствами ОС не реализовано. Штатная запись FAT32/exFAT остаётся прежней ограниченной экспериментальной. NTFS RW всё ещё запрещена, нет транзакций `$LogFile`/recovery и согласованного обновления MFT/индексов/bitmap.
- NTFS сохраняет ограниченный MFT scan, лимиты 32 runs/32 DATA attributes/16 КиБ attribute list/64К records/4096 bytes FILE record, отсутствие `$MFT` bootstrap lists, full Unicode lookup, ADS, EFS/compression/reparse. Новое чтение не отменяет эти ограничения.
- Каталоги/пути и metadata-streams остаются ограниченными. Длинные FAT-цепочки читаются без индексного кэша: большие смещения в chained FAT32/exFAT могут быть медленными. Максимальная ёмкость формата и все виды фрагментации не сертифицированы.
- Нет обещания стабильности при внезапном отключении питания, тестов реального железа или запуска на каждом процессоре. BIOS — собственное i386-ядро; UEFI x64 сохраняет Boot Services, не является собственным post-firmware x64-ядром.

## Дальше до 1.0

1. Перевести блочный HAL/тома и оставшиеся metadata-streams на 64 бита, проверить большие/4Kn тома и платформенные провайдеры.
2. Расширить NTFS-индексы/имена/bootstrap, затем транзакции и recovery до разрешения NTFS-записи.
3. Согласованная wide-запись exFAT, усиление FAT32/exFAT аудита и fault/power-loss модели; независимая проверка совместимости.
4. Только после закрытия обязательных ФС и аппаратной матрицы принимать решение о стабильном Release 1.0.
