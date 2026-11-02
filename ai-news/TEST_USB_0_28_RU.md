# Проверки MultiOS 0.28-dev-usb-hardening

Статус: неопубликованная разработка. Проверки выполнены в QEMU 9.2 и программных моделях, без реального оборудования. PASS относится к перечисленному сценарию, а не ко всей спецификации USB или файловой системы. Изменения перечислены в `USB_STORAGE_0_28_RU.md`.

## Сборка

Расширенное BIOS-ядро: freestanding C/ASM, i386 code generation, 280648 байт `kernel.bin`. Проверена сборка с запретом implicit function declarations и int-to-pointer conversions в BIOS NTFS runtime. Оставшиеся предупреждения: адреса packed-полей upstream NTFS-3G и RWX LOAD segment ELF. Они не объявлены исправленными.

UEFI x64 бинарник сохранён из прежней поставки, не является новой нативной реализацией USB. При упаковке проверяется его побайтовое совпадение с 0.27. Обычная `make all` и расширенная `make bios-rw` — разные профили.

## USB-накопители: 9 из 9

| Контроллер | DATA FAT32 | DATA exFAT | DATA NTFS |
| --- | --- | --- | --- |
| UHCI, USB full-speed | PASS | PASS | PASS |
| OHCI, USB full-speed | PASS | PASS | PASS |
| EHCI, USB high-speed | PASS | PASS | PASS |

Каждый тест: BIOS EDD-загрузка с USB без IDE, затем нативный `pc-usb-bot`; создание каталога, write/append/type/copy/cmp/rename/resize/writeat/delete/sync, перезапуск и чтение сохранённого `PERSIST.TXT`. Расходный диск дополнительно увеличен на 16 МиБ; проверено сохранение контрольного хвоста за исходным образом. Ввод команд здесь идёт через serial, а не через USB-клавиатуру.

После выключения VM независимо проверены все 9 дисков: read-only `fsck.fat -n`, `fsck.exfat -n` либо `ntfsfix -n`, а также точные байты `alpha\nbeta\n` в `PERSIST.TXT`. FAT32 читается mtype, NTFS — ntfscat, exFAT — отдельным минимальным read-only разборщиком корневого каталога. `ntfsfix -n` не заменяет Windows chkdsk и не доказывает совместимость со всеми NTFS-томами.

## Дополнительные QEMU-сценарии

- EHCI, все три ФС: файл 8193 байта, запись на смещениях 509 и 4093, копирование/сравнение, уменьшение до 513 байт и повторное расширение с проверкой нулевого заполнения — PASS, 3/3.
- NTFS через EHCI: именованные DATA-потоки, изменение/расширение потока, регистронезависимый поиск имени потока, жёсткая ссылка, COPYADS и сохранение общего содержимого после удаления первого имени — PASS в проверенном наборе.
- Извлечение выбранного системного диска на UHCI/OHCI/EHCI — PASS, 3/3: отказ дальнейшей записи, без автоматического переназначения системного носителя.
- Повторный startup: OHCI 10/10 и EHCI 10/10, подключённый FAT32 BOT-диск, mount RO и чистый EXIT. Это не статистическая гарантия для любого устройства.
- Boot HID: UHCI, OHCI, EHCI, второй root-порт OHCI/EHCI и две companion-конфигурации — PASS, 7/7. Проверены Shift, редактор строки, WRITE/TYPE/SYNC, unplug/replug и EXIT.
- Одновременно отдельные UHCI-клавиатура и EHCI-системный диск без IDE — PASS. Это не снимает ограничение одного устройства на host.
- Принудительный readonly и несовпадающий boot MBR ID — PASS: операции отклонены, хеши образов не изменены.
- Повреждённые размер/CRC BIOS payload и недостаточная E820 RAM — PASS: загрузчик не запускает файловую систему, образ не изменён.
- ATA-профиль на всех трёх ФС — PASS: mount/write/read/sync/lock/exit, провайдер `pc-ata-pio`.

## Модельные и sanitizer-тесты

- BOT/SCSI, ASan/UBSan: roundtrip RW, flush, CBW/CSW, sense, geometry, write protection, disconnect; data/CBW/CSW STALL, отказ clear-halt, один повтор только CSW, ограниченная готовность носителя. WRITE автоматически не повторяется — PASS.
- Реальные bulk-функции каждого HCD с моделируемыми регистрами/DMA: full, short, zero-length, STALL, partial STALL, error, timeout, disconnect — PASS, 24 сценария. Для OHCI дополнительно неподвижный и запаздывающий счётчик кадров. Это не модель всей электрической шины.
- NTFS RO-view cache: 100000 случайных чтений, LRU-коллизии, invalidation/epoch, partial/positive errors, переполнение часов и границы аргументов — PASS.
- USB enumeration: 23 сценария дескрипторов, скорости/MPS, SET_PROTOCOL, optional SET_IDLE, interval/arm failures — PASS.
- Write guard: 120 попыток тихой порчи записи, 56 ошибок чтения, 30 тихих ошибок recovery-write; ATA guard — 17 проверок — PASS.
- Private undo: по 40546 проверок в обычной и sanitizer-сборках; по 272 crash, 272 I/O, 130 recovery interruption и 48 corruption/read-failure сценариев — PASS.
- Повторные сбои: по 13440 цепочек из двух отказов в обычной и sanitizer-сборках; low и >32-bit LBA, прерывания единственной оставшейся копии заголовка — PASS.
- HID fuzz, PS/2 и редактор строки — PASS. Двухкнопочный OSK: все 56 ячеек и 95 печатных ASCII, shift/actions, hide/reopen, wrap, опциональный PREVIOUS, held/chord/release — PASS. HAL binding/bounds/RO/lifecycle/sticky failure — PASS.

## Реальная NTFS-3G библиотека, матрица отказов

Обычная сборка и сборка с ASan/UBSan, инструментирована вся встроенная библиотека. Отдельный расходный NTFS-том, private journal capacity 128 записей:

| Проверка | Обычная | ASan/UBSan |
| --- | --- | --- |
| Assertions | 10847228 | 10847228 |
| Crash cases | 4288 | 4288 |
| I/O error cases | 4288 | 4288 |
| Recovery interruptions | 177 | 177 |
| Результат | PASS | PASS |

Проверяются точный откат, привязка журнала, RO-поведение и исчерпание capacity. Это блочная fault-модель, не отключение питания физической флешки. Она не доказывает реализацию всего NTFS и не является тестом USB-транспорта.

## Ошибки, найденные до принятой сборки

1. NTFS adapter мог подменить первую ошибку исчерпания журнала последующим IO/READONLY и пропустить ошибку, возникшую при закрытии тома. Теперь сохраняется первая ошибка, дальнейший I/O текущего представления прекращается, ошибка закрытия возвращается вызывающему слою. Матрица capacity/rollback пройдена без ослабления ожидаемых кодов.
2. OHCI мог ложно сообщить `frame timer stopped` при единственной выборке через 3 мс. Теперь startup ожидает наблюдаемое продвижение трёх кадров в ограниченном цикле; повторное использование DMA после SKIP требует двух наблюдаемых кадров. После пересборки пройдены матрица ФС, HID и десять последовательных запусков OHCI. Остановленный таймер по-прежнему приводит к отказу, не к ложному успеху.
3. В BIOS runtime добавлены отсутствовавшие объявления функций; строгая компиляция оставлена включённой.

Первоначальные неуспешные диагностические прогоны не представлены как успешные. `reports/` содержит принятые прогоны новой сборки. Время отдельных команд в логах зависит от эмуляции и конкуренции тестов; это не аппаратный benchmark.

## Воспроизведение

На внешней Linux-машине нужны GCC/binutils с i386 support, make, Python, mtools, dosfstools, exfatprogs, ntfs-3g/ntfsprogs и QEMU с классическими USB host-моделями. Работайте только с копиями файлов-образов. Не запускайте два сценария с одинаковым именем рабочего каталога одновременно.

Из распакованного каталога `MultiOS`:

```sh
make bios-rw
make test-usb-bot test-read-cache test-usb-bulk-hosts test-usb-enum
make test-write-guard test-repeated-crash test-hid test-ps2 test-editor
```

Согласованный новый образ собирается по инструкции `USB_STORAGE_0_28_RU.md`. Для каждого fs=fat32/exfat/ntfs и host=uhci/ohci/ehci:

```sh
python3 tests/usb_storage_qemu.py ../images/MultiOS-0.28-usb-fat32.img qemu-system-i386 uhci --usb-boot --reboot --padded --keep-disk
python3 tests/usb_result_check.py build/usb28-storage-MultiOS-0.28-usb-fat32-uhci-usbboot-padded/disk.img fat32
```

Дополнительные флаги storage-теста: `--extended`, `--disconnect`, `--boot-only`. Boot-only не разрешает запись. Для HID подготовьте новый ATA-образ редактором с `bios_input=usb-only`, затем используйте `tests/usb_hosts_qemu.py IMAGE QEMU`. Остальные сценарии: `usb_combo_qemu.py`, `usb_storage_guards.py`, `usb_loader_guards.py`. Точная среда каждого прогона отражена в отчёте; обычная desktop-сборка NTFS не подменяет freestanding BIOS-ядро.

## Что остаётся за пределами проверки

Реальное железо, другие CPU/chipset/firmware, USB electrical compliance, физический power-cut, Windows chkdsk, все варианты повреждённых/грязных/гибернированных томов, сложные companion-топологии и устройства с нестандартными quirks. Нет xHCI, внешних USB-хабов, split transactions, UAS, multi-LUN, 4Kn, произвольных HID/composite и других USB-классов. Запись остаётся экспериментальной, начальный режим — RO. Полной поддержки USB 1.1/2.0 и релизной стабильности данный отчёт не заявляет.
