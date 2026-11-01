# MultiOS 0.21-dev-ntfsstreams — NTFS ADS и жёсткие ссылки

MultiPulti · **vm2 studios | t.me/v2vproj**. Неопубликованная development-сборка. **Полная совместимость NTFS и готовность к Release 1.0 НЕ заявляются.**

## Что реализовано

### Именованные потоки NTFS — ADS

Системный VFS API и NTFS-3G bridge теперь умеют перечислять named `$DATA`, читать, эксклюзивно создавать, заменять, дописывать, изменять данные по 64-битному смещению, менять размер и удалять потоки. Основной безымянный поток при этом не меняется. Поддерживаются resident/nonresident потоки, нулевая семантика расширения, большие смещения и LZNT1 в поддерживаемой геометрии NTFS-3G.

Имя потока передаётся ОТДЕЛЬНО от пути файла. Формат `file:stream` намеренно не переписывается в путь: это предотвращает неоднозначность и обход обычных правил путей. Имя — до 255 UTF-16 units, с отдельным UTF-8 буфером, без ограничения 255 UTF-8 байт основного VFS-пути. Действуют ограничительные Windows-подобные правила символов: без управляющих символов, `/\:*?"<>|`, завершающих пробела/точки. Это не обещание поддержки всех имён, которые можно записать напрямую в NTFS.

Поиск ADS не зависит от регистра и использует `$UpCase` конкретного тома. NTFS-3G `ntfs_attr_open` сам по себе case-sensitive: bridge сначала находит фактическое написание и только затем открывает атрибут. Это проверено отдельным тестом.

Потоки относятся к существующим обычным файлам. ADS каталогов, произвольные NTFS metadata attributes, EFS-потоки и reparse-объекты не открываются этим API. В перечислении предел 4096 потоков и 16384 шагов атрибутов за один вызов. После изменения файла начинайте перечисление заново; stream handles между операциями не сохраняются.

### Жёсткие ссылки

`LINK source newpath` создаёт дополнительное имя для того же MFT-файла на том же томе, включая другой каталог. Данные и ADS общие, не скопированные. Удаление одного имени сохраняет содержимое, пока есть другие ссылки. Переименование не должно терять ADS. Каталоги, системные MFT-объекты, readonly/system/reparse/encrypted source исключены; существующее имя назначения не перезаписывается. Права ACL пока не являются реализованной политикой доступа MultiOS.

### COPYADS / COPYADSRESUME

Копирует безымянный DATA и все доступные named DATA в staging-файл; дописывает отдельными транзакциями по 8192 байт, сравнивает байты и лишь затем переименовывает staging в конечное имя.

При продолжении проверяются все существующие ADS staging: нет ли лишнего имени, неверного размера или несовпадающего префикса. Чужой/несовпадающий поток не удаляется и не перезаписывается автоматически. Имена, неоднозначные при сравнении без регистра, отклоняются там, где их обнаруживает проверка инвентаря.

**Это DATA+ADS, не полное клонирование NTFS-файла.** Не переносятся ACL/security policy, EA, object IDs, link relationships, timestamps, sparse/compressed физическая раскладка. Назначение может занимать полный логический размер sparse-источника. Не используйте команду для миграции Windows с сохранением всех системных метаданных.

Обычные COPY/COPYATOMIC сохраняют прежнюю data-only семантику. Для переноса Zone.Identifier используйте COPYADS; молчаливого обещания сохранения ADS у обычного COPY нет.

### Модульность и безопасность

ABI ФС **5**: optional capabilities NAMED_STREAMS/HARDLINKS и проверяемые callback-наборы. Включённые статические модули пересобраны. Старые ABI 4 и ранее отклоняются. BIOS/FAT32/exFAT не получают фиктивных NTFS-возможностей: их callback-поля пусты.

Все новые мутации проходят через VFS write guard и приватный undo-журнал. Старые файловые views инвалидируются до dispatch. Бюджет одной операции — до 4096 различных изменённых секторов в готовом образе, около 2 МиБ минус метаданные. COPYADS состоит из многих транзакций и не откатывается целиком; незавершённый staging сохраняется для явного продолжения.

Dirty/hibernated NTFS не должны принудительно открываться RW. Приватный undo не является NTFS `$LogFile` recovery и не исправляет повреждённые/незавершённые Windows-транзакции. После ошибки изменяющей операции сначала reboot/remount/recovery, затем UNLOCK YES. Не обходите блокировку записи.

## Команды

```text
UNLOCK YES
WRITE /FILE.TXT "Main contents"
SNEW /FILE.TXT Notes "Additional data"
STREAMS /FILE.TXT
SREAD /FILE.TXT Notes 0 15
SWRITEAT /FILE.TXT Notes 0 "Changed"
SAPPEND /FILE.TXT Notes " tail"
SRESIZE /FILE.TXT Notes 128
SHEX /FILE.TXT Notes 0 32
LINK /FILE.TXT /ALIAS.TXT
COPYADS /FILE.TXT /COPY.TXT
SDEL /ALIAS.TXT Notes
LOCK
POWEROFF
```

- `SNEW file stream text` — только новый поток, существующий не перезаписывает.
- `SWRITE file stream text` — заменить существующий поток; не создаёт отсутствующий.
- `SAPPEND file stream text` — дописать в существующий поток.
- `SWRITEAT file stream offset text` — точные байты внутри текущего размера, без автоматического расширения и без перевода строки.
- `SRESIZE file stream size` — размер, логические нули при росте.
- `SDEL file stream` — удалить только названный поток.
- `SREAD/SHEX file stream offset count` — текст/hex, максимум 4096 байт за команду. Для бинарных данных безопаснее SHEX.
- `STREAMS file` — имена, размер, initialized length, flags.
- `LINK source newpath` — ещё одно имя файла; не символическая ссылка.
- `COPYADS source dest` — DATA+ADS, конечное имя должно отсутствовать.
- `COPYADSRESUME source dest staging` — продолжить по фактическому staging-пути, напечатанному после ошибки/сбоя. Например `/MC000001.TMP`, но имя нельзя угадывать.

Пробелы в путях/именах/тексте заключайте в кавычки. Потоки с длинными Unicode-именами удобнее адресовать C API: редактор команд и основной VFS-путь имеют собственные пределы. ADS могут содержать произвольные байты; текстовые команды не заменяют бинарный API.

## Комплект и запуск

`images/MultiOS-0.21-dev-ntfsstreams-{fat32,exfat,ntfs}.img` — три альтернативных raw-образа с BIOS bootstrap/kernel, FAT32 ESP, выбранным data-разделом и отдельным undo-разделом. NTFS ADS/hardlinks доступны **в x64 UEFI/host upstream-профиле**, не в BIOS reader.

Для NTFS-проверки из `source/MultiOS`:

```sh
python3 tools/run_upstream.py ../../images/MultiOS-0.21-dev-ntfsstreams-ntfs.img
```

Нужны QEMU/OVMF, Secure Boot отключён. Launcher использует временную копию: изменения после выхода удаляются. Для сохранения подключите отдельную сохраняемую копию к VM. Для VirtualBox сначала `VBoxManage convertfromraw image.img image.vdi --format VDI`, затем включите EFI в настройках VM.

Внутри ESP: `MULTIOS/BOOT.CFG`, `DOCS/FS21.MD`, `SOURCES/PORTING.ZIP`. Редактируйте только выключенные копии. C-драйверы требуют пересборки; изменение исходника на флешке само по себе не меняет бинарник. BIOS использует raw-зеркало ядра: не меняйте только KERNEL32.BIN общим редактором, применяйте imagectl install или новый образ.

Физические диски с данными для этих испытаний не использовались. Сначала тестируйте в VM. Запись raw-образа на носитель уничтожает его прежнюю разметку/данные; автоматического установщика физических дисков в этом патче нет.

## Сборка и воспроизведение тестов

Linux/WSL: GCC/binutils/make/Python, dosfstools/mtools/exfatprogs/ntfs-3g; QEMU/OVMF для boot. GNU-EFI, FatFs и NTFS-3G исходники включены. В guest Linux/FUSE не требуется. GNU-EFI собирается baseline x86-64 из включённого архива.

```sh
make upstream-image UPSTREAM_DATA_FS=ntfs UPSTREAM_OUTPUT=build/new-ntfs.img
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/ntfs-stream-probe build/ntfs-stream-probe-sanitize
python3 tests/ntfs_streams.py
python3 tests/ntfs_casefold_ads.py
MULTIOS_STREAM_PROBE="$PWD/build/ntfs-stream-probe-sanitize" python3 tests/ntfs_streams.py
make test-file64-epoch test-fs-registry test-copy64-model check-core
```

При необходимости задайте LD_LIBRARY_PATH для ASan/UBSan runtime своей toolchain. Отчёты запуска входят в `reports/ntfsstreams-0.21/`. Модели I/O/process-termination и ntfsfix -n — не физическая сертификация и не Windows chkdsk.

## Нельзя твёрдо назвать релизной полной поддержкой NTFS

Незакрытые блокеры именно заявленной **полной** совместимости:

- RW NTFS в BIOS/i386 low-memory kernel не реализован; там остаётся RO.
- Нет полноценного NTFS `$LogFile` replay/recovery/chkdsk и гарантии ремонта повреждённых Windows-томов.
- Нет Windows security token/SID/ACL inheritance/access-check модели; сохранение дескриптора в тесте не означает реализацию контроля доступа.
- Нет полной семантики EA, ADS каталогов, reparse/junction/symlink/cloud placeholders, per-directory case sensitivity/WSL и всех атрибутов/имён.
- EFS требует криптографии и ключей пользователя, BitLocker — отдельного encrypted-volume слоя; они не становятся поддержанными от подключения NTFS-3G.
- HAL по-прежнему MBR/512-byte sectors/до 128 ГиБ, нет полного GPT/4Kn; UEFI сохраняет Boot Services. Нет универсальных native storage/USB драйверов.
- Не проведена матрица Windows 10/11 + chkdsk /scan, реальные dirty/shutdown/hibernation сценарии Windows, питание/flush на физических контроллерах и длительное hardware QA.

**Вердикт: закрыта существенная часть недостающих файловых операций NTFS, но полная совместимость и Release 1.0 не достигнуты.** Не подменяем эти требования переименованием development-сборки.
