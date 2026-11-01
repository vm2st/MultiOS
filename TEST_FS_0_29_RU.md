# Проверки MultiOS 0.29-dev-fs-safety

Неопубликованная разработка. Проверки ниже подтверждают перечисленные сценарии, а не полную совместимость форматов, Windows NTFS или оборудования. Запись остаётся экспериментальной.

## Изменения рабочего кода

- `lib/fatfs_adapter.c`: первая ошибка дискового bridge сохраняется; до окончания операции прекращаются дальнейшие HAL read/write/flush и UpCase-запросы; завершение не маскирует ошибку успешным remount.
- `lib/fs.c`: unlock проверяет ответ и конечное состояние драйвера, geometry, writable provider и привязку undo. При нарушении контракта запись не разрешается.
- Оба regression-теста сначала воспроизвели ошибку на исходниках 0.28. После исправления прошли. Новых исправлений внутри `ntfs3g_adapter.c` в этом патче нет; прежние изменения NTFS сохранены.

## Новые регрессии

| Проверка | Результат и область |
| --- | --- |
| FatFs bridge, ASan/UBSan | 9 сочетаний first-error read/write/flush и NOSPACE/IO/READONLY; нет последующих HAL вызовов и успешного remount; bounds/null guards |
| VFS unlock contract, ASan/UBSan | Некорректный ответ, poison, unmounted, RO, подмена драйвера/geometry, потеря provider/binding не дают RW |
| Unlock реальных ФС | FAT32: 8, exFAT: 8, NTFS: 9 выбранных read/flush failure points; 0 записей, исходный SHA256 неизменен |
| Почти полный DATA | 3 ФС × обычная/ASan/UBSan; заполнение ненулевыми append до остатка около 32 КиБ (64 кластера FAT32, 8 кластеров exFAT/NTFS в этих fixtures), отказ create/append/copy и FAT resize либо NTFS sparse materialization; побайтовое сравнение всех секторов вне изменяемого журнала и неизменяемых footer; remount, прежнее содержимое сохранено |

NTFS sparse resize законно увеличивает логический размер без физического выделения всех кластеров. Тест явно допускает это и проверяет нули, затем отказ при материализации ненулевой записи. Первоначальное ожидание ENOSPC от самого sparse resize было исправлено в тесте, не в драйвере.

## Аварийные матрицы реальных файловых движков

Числа для каждой из обычной и ASan/UBSan-сборок; два запуска не складываются в число уникальных сценариев.

| ФС | Проверки C | Прерывания | Ошибки I/O | Прерывания восстановления |
| --- | ---: | ---: | ---: | ---: |
| FAT32 | 16 838 961 | 1336 | 1336 | 81 |
| exFAT | 1 006 328 | 1164 | 1164 | 65 |
| NTFS-3G | 10 847 228 | 4288 | 4288 | 177 |

Эти bounded-модели перебирают границы конкретного набора операций; это не перебор всех возможных файлов, повреждений или физических состояний устройства. В NTFS sanitizer-сборке инструментирована и библиотека NTFS-3G. FAT32/exFAT дополнительно: 96 seeded операций и 12 внешних fsck checkpoints на ФС.

## Прочие проверки записи

- Random I/O всех трёх ФС: границы сектора/кластера, shrink/grow, нули после расширения, stale views, RO, no-op и overflow. Выбранные raw-write точки отказа: 19/19 FAT32, 19/19 exFAT, 19/18 NTFS для двух операций, каждая с I/O failure и process termination. Обычная и sanitizer-сборки.
- exFAT ValidDataLength: zero masking, материализация append/write-at/shrink, откат слишком большой материализации и независимая проверка raw metadata/data.
- Staged copy DATA более 4 МиБ: cancel/remount/resume, prefix mismatch без модификации, no-overwrite, пустые/Unicode/nested пути, staging alias, финальная ошибка чтения и RO. На каждой ФС 33 выбранные точки raw-write × I/O failure/process termination, затем recovery/resume и внешний базовый checker; обычная и sanitizer-сборки. Это перенос DATA, не клонирование всех NTFS-метаданных.
- Бюджет private undo 4096 секторов: 1 МиБ atomic create/copy, полное чтение, rename/delete на каждой ФС.
- Сохранены и повторены unit-проверки read cache, USB BOT, USB enumeration и bulk host adapters, write guards, repeated undo crashes, file64 epochs, HID, PS/2 и редактора строки. Unit-модели USB сами по себе не подтверждают аппаратную совместимость.

## Готовое BIOS-ядро

Пересобрано собственное freestanding i386-ядро расширенного профиля FatFs/NTFS-3G, 280936 байт. Неопределённых символов нет. Повторная сборка из распакованного FULL-SOURCES дала побайтово идентичный kernel.bin. Сохраняются отдельные предупреждения upstream packed members и RWX LOAD segment; их устранение в этом патче не заявляется.

QEMU 9.2, TCG, CPU model 486, RAM 64 МиБ:

- 3 ФС × UHCI/OHCI/EHCI: загрузка с единственного USB-диска, без IDE, собственный `pc-usb-bot`, mkdir/write/append/type/copy/cmp/rename/resize/write-at/delete/sync/lock, повторная загрузка и сохранность файла. Носитель увеличен на 16 МиБ относительно образа; добавленный хвост не изменён.
- EHCI дополнительно на трёх ФС: cross-sector/cluster write, shrink/regrow zero check; для NTFS ADS/hardlink/COPYADS.
- ATA на каждой ФС: нативная BIOS-загрузка и запись/copy/resize/cross-sector/zero-regrow; независимая проверка сохранённого файла.
- Результирующие DATA проверены `fsck.fat -n`, `fsck.exfat -n` или `ntfsfix -n`; точные байты PERSIST.TXT проверены mtype/ntfscat либо отдельным ограниченным raw exFAT-extractor. `ntfsfix -n` не равен Windows chkdsk и не сертифицирует весь NTFS.

Новые исправления находятся в BIOS-ядре и исходниках. EFI бинарник сохранён от прежней поставки; он не выдаётся за пересобранную UEFI-версию 0.29.

## Воспроизведение

Требуются GCC/binutils/make, Python3, mtools/dosfstools, exfatprogs, ntfs-3g/ntfsprogs, ASan/UBSan runtime; для boot-тестов QEMU с UHCI/OHCI/EHCI и BIOS ROM. Все тесты выполняются на обычных расходных файлах образов, не на физических устройствах.

Из FULL-SOURCES, каталог MultiOS:

```sh
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/multios-host
make test-fatfs-bridge test-unlock-contract
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/fs-unlock-probe build/fs-full-probe build/fs-full-probe-sanitize
python3 tests/fs_unlock_faults.py ../images
python3 tests/fs_full.py ../images
python3 tests/storage_tx.py
python3 tests/upstream_ntfs_fault.py
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/fs-random-probe build/fs-random-probe-sanitize build/fs-copy-probe build/fs-copy-probe-sanitize build/fs-capacity-probe
python3 tests/random_io.py
MULTIOS_RANDOM_PROBE=build/fs-random-probe-sanitize python3 tests/random_io.py
python3 tests/staged_copy.py
MULTIOS_COPY_PROBE=build/fs-copy-probe-sanitize python3 tests/staged_copy.py
python3 tests/upstream_capacity.py
make bios-rw
python3 tests/usb_storage_qemu.py ../images/MultiOS-0.29-usb-fat32.img qemu-system-i386 ehci --usb-boot --reboot --extended --padded --keep-disk
```

Повторить boot-тест для всех ФС и host-моделей. `tests/usb_result_check.py` проверяет оставленный test disk. Внешние fsck запускать только после остановки гостя. При смене FS_PROFILE не переиспользуйте несовместимые объектные файлы; отдельная BIOS-RW сборка изолирована своим каталогом.

## Что остаётся до Release 1.0

Нет проверок физического power-cut, разнородного реального железа и Windows chkdsk. Не реализованы все NTFS/Windows операции и все USB-топологии. Конечный private undo не заменяет полную семантику NTFS $LogFile. Нельзя утверждать безопасность произвольного грязного/гибернированного/повреждённого тома. Список ограничений поставки — `FILESYSTEMS_0_29_RU.md`; полный USB-статус — `USB_STORAGE_0_28_RU.md`.
