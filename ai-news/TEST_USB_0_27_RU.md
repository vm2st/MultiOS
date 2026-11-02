# Проверки MultiOS 0.27-dev-usb-bot

Неопубликованная разработка. Статус не равен стабильному Release 1.0. Ниже разделены нативные проверки ядра, host-модели и внешние проверки; они не подменяют испытания железа.

## Среда и метод

QEMU 9.2, TCG, машина PC, CPU 486, 64 МиБ RAM, SeaBIOS. Каждый USB-диск — обычная расходная копия файла, не физический накопитель. BIOS ядро собиралось freestanding i386, бинарник 279720 байт, stage-2 1170 байт. Нет зависимости нативного runtime от Linux/Windows.

Во всех `--usb-boot` сценариях **ATA/IDE-диск вообще не подключён**. SeaBIOS сначала читает MBR; stage-2 читает ядро через BIOS EDD, после входа в MultiPulti файловый ввод-вывод идёт через нативный UHCI/OHCI/EHCI BOT, что проверяется по `modules` и `usb`.

Файловые сценарии передают команды через COM1 для воспроизводимости. Это не доказательство USB-клавиатуры; она отдельно тестируется настоящими эмулируемыми HID-пакетами без serial/PS2 input injection.

## Основная матрица окончательного ядра

`tests/usb_storage_qemu.py IMAGE QEMU HOST --usb-boot --reboot --padded --keep-disk`.

| Системный том | UHCI | OHCI | EHCI |
| --- | --- | --- | --- |
| FAT32 | PASS | PASS | PASS |
| exFAT | PASS | PASS | PASS |
| NTFS | PASS | PASS | PASS |

Каждый принятый сценарий включает:

- обнаружение MSC BOT и выбор `pc-usb-bot`, RO при старте;
- `unlock YES`, MKDIR, WRITE, APPEND, TYPE;
- COPY/CMP, REN, RESIZE, WRITEAT и проверку содержимого;
- DELETE/RMDIR, SYNC, LOCK, штатный EXIT;
- вторую загрузку и чтение `PERSIST.TXT` с точными `alpha\nbeta\n`;
- USB-диск на 16 МиБ больше исходного образа с контрольным хвостом: граница образа найдена через проверенные MBR/footer, хвост не изменился.

Это проверка существующих операций, не всех состояний всех файловых систем. Отдельный probe с точно выровненным увеличенным носителем повторён для всех девяти пар host/ФС: mount, DIR, guarded UNLOCK/LOCK и неизменность хвоста — PASS.

Размеры тестов невелики; они не доказывают работу на многотерабайтных томах.

## Расширенные операции

`--extended --reboot --usb-boot`, EHCI, FAT32/exFAT/NTFS:

**PASS для всех трёх ФС на окончательном ядре.**

Создание 8193-байтового файла, запись с пересечением границ секторов/кластеров (offset 509 и 4093), точное чтение, COPY/CMP, уменьшение до 513 и расширение до 8193 с проверкой нулей, удаление.

Для NTFS дополнительно: именованный DATA-поток, SWRITEAT, SAPPEND, SRESIZE и точный hex; hardlink; COPYADS; изменение потока через alias; удаление исходного имени и чтение потока через оставшуюся ссылку. Это не обещание копирования ACL и всей Windows metadata.

## USB-ввод и совместная работа

- Семь сценариев `usb_hosts_qemu.py`: UHCI, OHCI, EHCI, второй root-порт OHCI/EHCI после неподдержанного устройства, EHCI→UHCI companion, EHCI→OHCI companion. Все PASS на окончательном ядре.
- Настоящий QMP→USB HID ввод, Shift, Left/Delete редактора, WRITE/TYPE/SYNC, извлечение/переподключение клавиатуры и EXIT. Не serial-эмуляция клавиш.
- `usb_combo_qemu.py`: **PASS**, UHCI USB-клавиатура + EHCI системный USB BOT диск, без ATA; команды WRITE/TYPE/SYNC введены через HID.
- На одном host диск и клавиатура одновременно пока не выбираются. Успешный combo-тест использует два разных контроллера.

## Отказы и регрессии

- Извлечение EHCI USB-диска: **PASS**, статус disconnected, TYPE возвращает ошибку, повторный UNLOCK и WRITE запрещены. Тест выключает VM через QMP: обычный EXIT требует успешного flush и не должен подделывать его после потери диска.
- Принудительный `pc-usb-bot-ro`: **PASS**, UNLOCK/WRITE отклонены, весь образ неизменен. Несовпадение boot MBR-ID: **PASS**, отказ старта модулей до монтирования, IDE-источник и USB-диск неизменны.
- BIOS EDD guards: **PASS**, неверный размер payload, CRC и недостаточная E820 RAM. ФС не запускалась, тестовые образы побайтово не изменились.
- ATA на окончательном ядре: **PASS** для FAT32/exFAT/NTFS — выбор `pc-ata-pio`, WRITE/TYPE/SYNC/LOCK/EXIT.
- Редактор настроек: **PASS**, четыре preset на всех трёх образах, создание только новых файлов, неизменность исходника, отказ corrupt mirror/header/kernel/footer/journal/FAT crosslink/symlink/device, принятие CRLF и старого INI, roundtrip нового USB-профиля.

Начальные прогоны NTFS через USB 1.1 достигали 180-секундного лимита команды, поэтому не засчитаны как успешные. Лимит повторного полного прогона увеличен до 900 секунд на команду; это изменение тестового ожидания, **не исправление производительности драйвера**. Пооперационные длительности сохранены в `reports/`. Уже простой TYPE на NTFS USB 1.1 может занимать десятки секунд в данной эмуляции. На полном прогоне COPY небольшого NTFS-файла через UHCI занял 419,30 секунды. Это неприемлемо для заявления о быстрой/готовой системе. Эти задержки — открытое ограничение, не основание заявлять готовый релиз.

## Unit и модельные проверки

- BOT/SCSI ASan/UBSan: RW roundtrip, flush, CBW/CSW, десять видов неверного/короткого/ошибочного ответа, tag wrap, границы LBA, write protection/неизвестный MODE SENSE, неподдержанный flush/геометрия, sense, disconnect, sticky failure/no blind write retry и image-prefix validation. **PASS**.
- USB enumeration ASan/UBSan: **23 случая PASS**, malformed lengths/endpoints, скорость/MPS, HID protocol/idle, interval/arm и MSC BOT.
- HAL: binding, контекст, bounds, RO, lifecycle и sticky failure только выбранного носителя. **PASS**.
- Existing write-guard model: **120** silent write attempts, **56** read failures, **30** silent recovery-write attempts. **PASS**. Это не реальные ошибки USB-флешки.
- ATA port model: **17 assertions PASS**.
- Undo model: **40546 checks**, 272 crash cases, 272 I/O error cases, 130 recovery interruptions, 48 corruption/read failures. Обычный и sanitizer-прогон **PASS**; не суммируются как независимые состояния.
- Repeated-crash model: **13440** двухошибочных цепочек, включая повторные обрывы заголовков; обычный и sanitizer-прогон **PASS**. Это ограниченная модель транзакции, не исчерпывающая модель оборудования/ФС.
- HID: **50000** fuzz reports; editor: **100000** reference-model events; PS2; OSK: все 56 ячеек, 95 печатных ASCII, NEXT/SELECT, optional PREVIOUS, hide/reopen. **PASS**.

## Независимое чтение после USB-записи

На извлечённых DATA-разделах расходных расширенных EHCI-прогонов:

- `fsck.fat -n`: PASS, без исправлений; `mtype` подтвердил точные байты PERSIST.
- `fsck.exfat -n`: PASS, clean; без исправлений.
- `ntfsfix -n`: PASS, MFT/MFTMirr и alternate boot согласованы в пределах этой утилиты. Это **не Windows chkdsk** и не полная проверка NTFS.
- `ntfscat`: точные байты PERSIST, именованного потока через hardlink и копии ADS. PASS.

Дополнительно внешние read-only проверки прошли для всех девяти окончательных matrix-дисков; точные байты PERSIST проверены mtype/ntfscat на FAT32/NTFS.

Эти внешние проверки повторены на расширенных EHCI-прогонах именно окончательного ядра 0.27: все три тома и проверяемые байты прошли.

## Повторение

```sh
make bios-rw
make test-usb-bot test-usb-enum test-write-guard test-repeated-crash test-hid test-ps2 test-editor
make build/two-buttons-test build/hal-test
./build/two-buttons-test
./build/hal-test
python3 tests/usb_storage_qemu.py IMAGE.img qemu-system-i386 ehci --usb-boot --padded --reboot
python3 tests/usb_storage_qemu.py IMAGE.img qemu-system-i386 ohci --usb-boot --reboot
python3 tests/usb_storage_qemu.py IMAGE.img qemu-system-i386 uhci --usb-boot --reboot
python3 tests/usb_storage_qemu.py IMAGE.img qemu-system-i386 ehci --usb-boot --extended --reboot
python3 tests/usb_storage_qemu.py IMAGE.img qemu-system-i386 ehci --usb-boot --disconnect
python3 tests/usb_storage_guards.py IMAGE.img qemu-system-i386
python3 tests/usb_loader_guards.py IMAGE.img qemu-system-i386
python3 tests/usb_combo_qemu.py IMAGE.img qemu-system-i386
```

Для HID-only теста нужен ATA-профиль с `bios_input=usb-only`, соответствующий `build/bios-rw/kernel.elf` и отдельная копия образа; см. `usb_hosts_qemu.py`. Нельзя параллельно запускать один и тот же сценарий с одним basename образа: каталоги результатов совпадут. Нельзя направлять тесты на физический диск.

## Что остаётся до Release 1.0

Реальное железо и разные firmware; xHCI/хабы/UAS и другие классы USB; расширение поддержанных дескрипторов/геометрий; потеря питания и физическое извлечение при записи; Windows chkdsk/взаимная проверка; стресс/длительные записи/крупные и заполненные тома; ускорение NTFS USB 1.1 и bounded responsiveness; исправление оставшихся compiler warnings upstream/port и отдельный аудит DMA/таймаутов. Отсутствие ошибок sanitizer в моделях не доказывает отсутствие ошибок в драйвере.

В сборке остаются предупреждения о packed members, недостающих прототипах в прежнем NTFS-порте и linker RWX segment. Успешная сборка с ними не является release approval. Этим патчем не проводится полная ревизия старого NTFS-порта.
