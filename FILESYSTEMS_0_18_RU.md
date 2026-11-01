# MultiOS 0.18-dev-unifiedfs: единый файловый стек

**MultiPulti · vm2 studios · t.me/v2vproj · НЕОПУБЛИКОВАННАЯ РАЗРАБОТКА**

Это крупный патч после 0.17, не сертификат полной поддержки NTFS и не Release 1.0. «Все ФС полностью закончены» пока не заявляется: ниже перечислены и реализованные функции, и остающиеся границы.

## Основные изменения

1. **FAT32/exFAT: FatFs теперь обслуживает чтение, перечисление каталогов, свободное место и запись.** Раньше FatFs был только движком изменения, а пользовательские данные читали другие ограниченные парсеры.
2. **NTFS x64 UEFI/host: NTFS-3G обслуживает mount, чтение, индексное перечисление каталогов, свободное место и запись.** Старый последовательный MFT-скан и собственный ограниченный run-list декодер больше не участвуют в этом профиле. Старый RO-драйвер сохранён для BIOS и legacy-профилей.
3. **NTFS compression: LZNT1-чтение, replace/append/copy/rename/delete, наследование сжатия от каталога.** Перед закрытием записанного атрибута вызывается `ntfs_attr_pclose`, чтобы закончить сжатые блоки до commit.
4. **NTFS sparse и 64-битные размеры:** проверены zero-filled holes, неинициализированные данные, файл >5 ГиБ и append после его конца. `u32` остаётся размером одного буфера записи, а не максимальным смещением NTFS append.
5. **Undo-журнал: ёмкость настраивается от 1 до 4096 разных изменённых секторов.** Новые образы создаются с 4096; старые 128-секторные образы остаются читаемыми и сохраняют свой прежний бюджет. Геометрия и MBR/CRC-binding по-прежнему проверяются. Ничего не расширяется на носителе автоматически.
6. **Записи/копии 1 МиБ:** проверены на всех трёх ФС, затем весь файл прочитан и сравнен. FAT32/NTFS дополнительно прочитаны внешними `mtype`/`ntfscat`; все три проверены внешними базовыми checkers.
7. **Чистые read-only пути:** тестовый HAL перехватывает любую попытку записи; SHA256 тестового NTFS-тома после чтения должен остаться неизменным. Неподдерживаемый reparse-путь не изображается обычным каталогом.
8. **Unicode:** FatFs API name buffer увеличен до 1020 байт, чтобы отличать длинное UTF-8 имя от короткого алиаса. VFS-путь остаётся 255 байт; непомещающееся имя отображается явно, а не усечённым доступным псевдонимом. NTFS использует тот же принцип.
9. **Sanitizer build:** отдельные `build/ntfs3g-san/` объекты, поэтому ASan/UBSan действительно инструментируют всю NTFS-библиотеку. Больше не нужно вручную подменять CC или удалять обычные объекты ради sanitizer-сборки.

## Поддержка по платформам

| Возможность | BIOS i386 | x64 UEFI |
|---|---|---|
| FAT32/exFAT чтение/запись | FatFs, статический профиль | FatFs |
| NTFS обычные файлы/каталоги RW | **Нет, прежнее RO-чтение** | NTFS-3G |
| NTFS compressed/sparse и >4 ГиБ | Не заявляется | Проверены перечисленные сценарии |
| Диск | Текущий IDE primary-master профиль | Block I/O загрузочного диска от прошивки |
| Самостоятельная загрузка | Собственное i386-ядро | EFI-приложение без гостевой ОС, **Boot Services сохранены** |

Новый NTFS-порт не следует собирать как BIOS kernel. Полная поддержка каждого процессора, контроллера и USB-устройства не заявляется. BIOS runtime здесь не проверен, если в отчёте явно не указано иное.

## Профили

- `profiles/filesystems.mk`: исторические драйверы, НЕ автоматически переключены.
- `profiles/filesystems-fatfs.mk`: единые FatFs read/write + undo; BIOS и UEFI.
- `profiles/filesystems-ntfs3g.mk`: тот же FAT-стек + единый NTFS-3G read/write; host/x64 UEFI.

Изменение C-модуля требует пересборки. Динамического ABI для произвольных `.drv` и hot replacement нет. Сохраняются независимые mount/unlock-аудиты FAT32/exFAT и чтение служебной exFAT `$UpCase` собственным проверяющим кодом: это не второй пользовательский read backend, а защитный слой, со своими ограничениями поддерживаемой геометрии.

## Готовые образы

В комплекте `images/MultiOS-0.18-dev-unifiedfs-{fat32,exfat,ntfs}.img`. Это три альтернативных полных дисковых образа, а не три раздела одного тома. Каждый содержит BIOS boot/kernel, FAT32 ESP с `EFI/BOOT/BOOTX64.EFI`, выбранный data-раздел и отдельный приватный undo-раздел примерно 4 МиБ.

Для первого запуска используйте FAT32, только на копии. УEFI x64, Secure Boot отключён (бинарник не подписан доверенным ключом). В VirtualBox сначала конвертируйте raw `.img` в VDI:

```sh
VBoxManage convertfromraw MultiOS-0.18-dev-unifiedfs-fat32.img MultiOS.vdi --format VDI
```

Из каталога исходников:

```sh
python3 tools/run_upstream.py ../../images/MultiOS-0.18-dev-unifiedfs-fat32.img
```

Запускатель создаёт временную копию: изменения при выходе НЕ сохраняются. Для постоянных экспериментов настройте VM на отдельной сохраняемой копии образа.

В консоли:

```text
VER
FS
DIR
UNLOCK YES
MD TEST
WRITE TEST/a.txt "MultiOS"
APPEND TEST/a.txt "second line"
COPY TEST/a.txt TEST/b.txt
CMP TEST/a.txt TEST/b.txt
REN TEST/b.txt final.txt
TYPE TEST/final.txt
DEL TEST/final.txt
DEL TEST/a.txt
RD TEST
LOCK
POWEROFF
```

OSK: F2/Ctrl+O; две кнопки NEXT/SELECT, PREVIOUS — при третьей кнопке. USB keyboard в UEFI работает через поддержку прошивки, это не обещание нативного универсального USB host stack.

## Пересборка и образ

Linux/WSL: GCC/binutils/make/Python, GNU-EFI headers/crt/libs, dosfstools/mtools/exfatprogs/ntfs-3g tools. Для тестов нужны QEMU/OVMF, shared `libntfs-3g` только как внешний генератор тестовых фикстур и ASan/UBSan runtime. В firmware FUSE/Linux/libc shared libraries не используются.

```sh
make upstream-image UPSTREAM_DATA_FS=fat32 UPSTREAM_OUTPUT=build/new-fat32.img
make upstream-image UPSTREAM_DATA_FS=exfat UPSTREAM_OUTPUT=build/new-exfat.img
make upstream-image UPSTREAM_DATA_FS=ntfs UPSTREAM_OUTPUT=build/new-ntfs.img
```

Существующий output не перезаписывается. Сначала собирается BIOS FatFs kernel, затем x64 UEFI с NTFS-3G. Новый образ получает 4096-секторный бюджет. Для отдельной одноразовой копии уже подготовленного исходного MBR-образа можно задать `tools/prepare_journal.py --journal-records 4096`; это не миграция существующего активного журнала, нужен исходный НЕподготовленный образ и новое имя output.

В образе редактируемая конфигурация — `MULTIOS/BOOT.CFG`; исходники — `SOURCES/PORTING.ZIP`; документация — `DOCS/FS18.MD`. BIOS читает raw-зеркало ядра: не заменяйте только `KERNEL32.BIN` обычным редактором. Используйте `imagectl.py install --help` для согласованной замены либо создайте новый образ. Не трогайте служебный journal-раздел и не редактируйте VM-диск, пока она запущена.

## Осталось до полного стабильного релиза

- **NTFS RW на BIOS ещё нет.** Нужен отдельный полноценный порт с изменением загрузки/памяти; нельзя честно объявить его готовым заменой пары строк.
- **4096 секторов — конечный бюджет, около 2 МиБ изменённых данных+метаданных за операцию.** Размер файла может быть больше; операция, которая не помещается, отменяется. Это не гарантия атомарного копирования любого файла любого размера. Стратегия для произвольно больших транзакций ещё не реализована.
- Нет полного Windows security/ACL enforcement, EFS-расшифровки, BitLocker, прозрачного обхода всех reparse types, полного набора команд ADS/hardlink/sparse management, собственного восстановления NTFS `$LogFile` или chkdsk.
- NTFS-3G умеет больше, чем выставляет наш VFS/консоль. Наличие исходника upstream не равно проверенной интеграции каждой возможности.
- 512-байтный HAL и текущая адресация до 128 ГиБ; нет GPT/4Kn/RAID-пути и универсальных контроллеров.
- Нет автоматического форматирования/ремонта повреждённых пользовательских томов. Dirty/hibernated NTFS RW блокируется намеренно.
- Не все типы отказов железа покрывает программная модель. Успешный flush должен действительно гарантировать сохранение; физические power-cut испытания и аппаратная совместимость ещё не пройдены.

После ошибки записи том может быть отмонтирован/заблокирован до повторного открытия и восстановления. Не обходите эту блокировку. Не используйте единственную копию важных данных.

## Проверки

Актуальные журналы — `reports/unifiedfs-0.18/`. Отчёты `reports/upstream-fs/` — история 0.17, не результаты повторного запуска 0.18.

```sh
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/multios-host build/upstream-growth build/fs-read-probe build/fs-mutation-probe build/fs-capacity-probe build/storage-tx-test build/storage-tx-sanitize
python3 tests/upstream_ntfs.py
python3 tests/upstream_growth.py
python3 tests/upstream_upcase.py
python3 tests/upstream_read_features.py
python3 tests/upstream_capacity.py
python3 tests/storage_tx.py
python3 tests/upstream_ntfs_fault.py
MULTIOS_TEST_JOURNAL_RECORDS=4096 python3 tests/upstream_ntfs_fault.py
MULTIOS_TEST_COMPRESSED=1 python3 tests/upstream_ntfs_fault.py
```

`storage_tx.py` — только FAT32/exFAT, несмотря на включённый NTFS-профиль. NTFS fault-скрипт создаёт свои одноразовые тома и фиксирует время для побайтового сравнения. Sanitizer-вариант требует доступных SONAME `libasan`/`libubsan`; путь при необходимости задаётся `LD_LIBRARY_PATH`.

Сложные фикстуры создаёт установленная внешняя NTFS-библиотека: attribute lists, 80 чередующихся allocation chunks, sparse >5 ГиБ, compressed LZNT1. Это отдельно от библиотечных объектов внутри MultiOS. Сравниваются байты через публичный VFS и внешнее ntfscat. Assertions не равны числу разных тестовых сценариев.

## Лицензии

Отдельные собственные исходники — MIT. FatFs сохраняет уведомления ChaN. NTFS-3G и объединённый NTFS-enabled EFI — GPL-2.0-or-later; соответствующие исходники, runtime и сборка включены. GNU-EFI сохраняет свои уведомления; использована версия 3.0.11. См. THIRD_PARTY_NOTICES.txt и vendor/*/UPSTREAM.json.
