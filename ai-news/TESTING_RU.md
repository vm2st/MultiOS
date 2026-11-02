> **0.11-dev:** добавлены VFS, FAT32/VFAT experimental RW и NTFS read-only.
> NTFS RW не реализован; подробности и новые команды: [FILESYSTEMS_RU.md](FILESYSTEMS_RU.md).
> Старый boot bundle 0.10 несовместим с новым ядром; создавай новый образ.

# Проверка поставки MultiOS 0.6-cpu

> Текущий снимок: **0.11-dev, не опубликован**. Первая публичная версия — только
> стабильный **Release 1.0**. См. RELEASE_POLICY_RU.md и TWO_BUTTONS_RU.md.

**vm2 studios — t.me/v2vproj | MultiOS / MultiPulti.**

Дата: 2026-10-30. Это результаты конкретной сборки, не гарантия совместимости с любым устройством.

## Выполнено успешно

- `tests/test.py`: **44 assertions**, включая реальную загрузку boot sector → freestanding C kernel → shell в QEMU на CPU **486** и **qemu64**.
- В обоих загрузочных сеансах проверены монтирование, создание каталога, запись текста, двоичное копирование, сравнение, чтение, переименование, удаление, освобождение кластеров и EXIT.
- `tests/edge_cases.py`: **32 assertions**. Фрагментированные FAT-цепочки, NoFatChain, ValidDataLength, CRC, дописывание, каталог без свободных slots, пути/кавычки, защита предков CWD, повреждённый entry checksum, переполненная строка.
- `tests/faults.py`: **48 assertions**, **16 позиций синхронного отказа sector write**, затем контрольный успешный запуск. Ошибка после установки dirty marker оставляет его установленным; повторное монтирование не позволяет включить запись.
- Host-часть тестов выполнена с **AddressSanitizer + UndefinedBehaviorSanitizer**: 44 базовых и 32 edge assertions (BIOS-сеансы не инструментированы) без обнаруженных ошибок. Эти проверки не инструментируют BIOS/ASM/ядро в эмуляторе.
- `fsck.exfat -n` независимо проверил чистоту тестовых томов после успешных операций и загрузочных сеансов.
- Проверен как образ, создаваемый `tools/mkimage.py`, так и внешний superfloppy, созданный **mkfs.exfat**.

Итого в обычной итоговой тестовой серии — **124 assertions**, не 124 разных аппаратных конфигурации. Sanitizer-прогон повторяет часть тех же сценариев и не прибавляется к этому числу.

## История: проверки переносимой версии 0.2

- `make test-hal`: контракт HAL проверен на in-memory блочном устройстве: ABI, обязательные callbacks, копирование дескриптора, передача ctx, границы LBA, запрет записи в RO, необязательные операции, запрет rebinding смонтированной ФС.
- `make check-core`: в общих модулях нет ASM, CPU port I/O, VGA-адреса или вызовов POSIX.
- Clang 15.0.7: **compile-only** для `armv7-none-eabi`, `aarch64-none-elf`, `riscv64-none-elf`, по 5 общих C-модулей на target. Итог — 15 object files, **без линковки, загрузки и BSP-драйверов**. Это не рабочие ARM/RISC-V порты.
- После перехода на HAL заново прошли файловые, загрузочные, edge, fault-injection и ASan/UBSan host-тесты, перечисленные выше.

## Инструменты BIOS-тестов

- GCC 11.5.0, GNU binutils/ld 2.41.
- QEMU 5.2.0, software TCG, модель машины `pc`, IDE primary master, 32 MiB RAM.
- exfatprogs / fsck.exfat 1.2.5.
- Хост: Linux x86-64; Python 3.13 для генерации/тестов.
- Ядро: C11, `-m32 -march=i386 -ffreestanding`, без libc, SSE, MMX, stack protector и PIE; небольшие входные/загрузочные части на GNU assembler.

Локальная системная сборка QEMU 11 предоставляла только q35 и не подходила этому IDE-прототипу. Для тестов отдельно собрана QEMU 5.2.0 с полноценной моделью PC. QEMU в архив **не включена**. На вашей системе нужна модель `pc` с legacy IDE; AHCI-only q35 пока не поддерживается.

## Ограничения BIOS-ветки / аппаратных тестов

- Физические компьютеры, UEFI/CSM конкретных производителей, USB-клавиатуры/USB-диски.
- Все модели x86/x64, native x86-64 long mode.
- Предельные размеры томов/файлов, полная совокупность допустимых вариантов exFAT.
- Полный Unicode, альтернативные таблицы UpCase, TexFAT, GPT, multiple volumes.
- Враждебные изображения ФС, систематическое fuzzing, полный аудит безопасности.
- Обрыв питания, torn sector writes, отказ контроллерного кэша, rollback и автоматическое восстановление.
- Инсталляция на реальный диск, DOS EXE/COM совместимость.

Исходники тестов и текстовые логи включены в архив. Для воспроизведения используйте раздел «Проверки проекта» в README_RU.md. Без QEMU или fsck соответствующая часть проверок пропускается; в данной поставке обе части были выполнены.

## Новые проверки 0.3-live: USB / UEFI / OSK

- QEMU 11.0.0 (q35, TCG), OVMF, xHCI, USB mass storage, USB HID keyboard: загрузка EFI/BOOT/BOOTX64.EFI с FAT32 USB-образа без другой гостевой ОС.
- Последний контрольный старт до prompt: **3.783 секунды**. Это одно измерение в эмуляторе; не универсальная скорость реальных устройств.
- Команда VER введена через OSK **только стрелками и Enter** по QMP/USB keyboard; буквенные клавиши для неё не отправлялись. Проверены текстовый вывод и выделение виртуальной Enter; скриншоты включены.
- USB-сеанс: mount exFAT image, DIR, UNLOCK, MD, WRITE, COPY, CMP, TYPE, LOCK, POWEROFF. Все команды прошли без Error.
- После завершения VM DATA.IMG извлечён с внешней FAT32: fsck.exfat clean, повторное чтение отдельной host-консолью дало Hello UEFI USB и Identical, Dirty=0.
- REBOOT вызван в отдельном UEFI-сеансе; QEMU с -no-reboot завершился ожидаемо.
- OSK unit-test: все 96 позиций печатных клавиш (48 обычных + 48 с Shift), навигация, управляющие действия, hide/toggle, отсутствие renderer.
- BIOS/host regression: прежние 44 + 32 + 48 assertions вновь прошли. HAL и статическая граница portable/platform также прошли.
- UEFI-ветка оставляет Boot Services активными и использует **драйверы прошивки**. Эти тесты не доказывают наличие native xHCI/USB/NVMe в MultiOS.
- Физические флешки/контроллеры, пользовательские прошивки, Secure Boot, нативный gamepad/touch/GPIO и реальные энергосостояния оборудования не проверялись.

OSK Hide → Up (повторное включение только кнопками) проверено в USB/UEFI-сеансе. OSK unit-test также выполнен с ASan/UBSan.


## Проверки 0.4-modular

- Общий менеджер модулей используется UEFI, BIOS и POSIX harness.
- Unit: атомарный парсер, неправильные ключи/ID/дубли/размеры, ABI/размер
  дескриптора, неверная роль, неизвестный модуль, rollback в обратном порядке
  на всех четырёх стадиях, отдельные contexts, запрет повторного запуска.
- 3000 детерминированных malformed-byte входов парсера; unit также запускается
  с AddressSanitizer и UndefinedBehaviorSanitizer. Это не полный security audit.
- Регрессия: 44 основных, 32 edge, 48 fault assertions и 16 точек отказа записи;
  HAL и OSK unit, статическая граница portable/platform.
  Host-сценарии повторены с ASan/UBSan после перехода на менеджер модулей.
- UEFI: стандартный профиль, OSK-only VER, MODULES, USB I/O, POWEROFF;
  forced-RO и минимальный вывод; шесть остановок загрузки на ошибках профиля.
- Отдельная пересборка minimal-профиля, загрузка и обратная пересборка/загрузка
  standard без make clean. Символ uefi-text отсутствует в minimal ELF.
- ARMv7/AArch64/RISC-V64: по шесть portable объектов, только компиляция,
  без готового BSP, линковки и загрузки.

Актуальные отчёты: `build/modular-regression.log`, `build/modules-sanitizer.log`,
`build/modular-uefi-report.txt`, `build/module-boot-report.txt`,
`build/profile-switch-report.txt`, `build/cross-report.txt`.
Предыдущие разделы описывают историю проверок 0.1–0.3; измерения скорости из
них не являются новой гарантией для реального оборудования.


## Проверки 0.5 / MultiPulti / vm2 studios

- Собственный BIOS entry point `multipulti_main`, имя MultiPulti32 и авторство
  видны в VER/CREDITS/MODULES и строках бинарника. GNU-EFI notices не удалены.
- Один 128 MiB образ: BIOS kernel, FAT32 ESP, общий exFAT-раздел.
- `tests/unified_test.py`: 31 проверка редактора и общего образа. BIOS 486 пишет
  файл → UEFI x64 USB читает его и пишет свой → BIOS qemu64 читает результат.
  Установленные binary bundles не меняют exFAT-данные; реальное изменение raw
  kernel mirror проверено, затем официальный kernel восстановлен.
- UEFI boot-disk adapter выбран при наличии постороннего внутреннего IDE/SATA
  образа: файл прочитан с нужного USB-тома, hash постороннего диска не изменился.
- Отдельный UEFI boot с forced-RO, независимый fsck.exfat после обмена данными.
- Редактор отклоняет symlink/hardlink, занятый POSIX-lock образ, неверные kernel/
  EFI бинарники, неизвестный ключ; неудачи оставляют исходный образ без изменений.
- `tests/bios_disk_guard.py`: BIOS загружает MultiPulti с IDE slave, а primary
  master содержит другой корректный диск. ATA module отклонён по MBR ID, reason
  виден в VGA; первичный диск побайтово не изменён. Это не поддержка USB в BIOS.
- Прежние 44 + 32 + 48 файловых assertions, HAL/OSK/module unit и core boundary
  проходят после изменения загрузчика. Compile-only ARM/RISC-V не стал BSP.

Отчёты текущего образа: `build/unified-report.txt`, `build/unified-test.log`,
`build/bios-disk-guard-report.txt`, `build/bios-disk-guard.log`,
`build/multipulti-regression.log`. Эмулируемое оборудование не равно тестированию
всех процессоров или реальных плат. Ни один физический накопитель не изменялся.


## Проверки 0.6: процессорный минимум

- 23 конфигурации CPU одного BIOS бинарника в QEMU TCG: от 486/Pentium до
  Core/EPYC, искусственный неизвестный vendor и два варианта без
  FPU/MMX/SSE/SSE2/CMOV. На каждом CPU, VER, запись и чтение файла прошли.
- Отдельная сборка с исключённым CPUID probe загружена на эмулируемом 486,
  чтение/запись выполнены; это **не** проверка реального CPU без CPUID/386.
- 10000 unit случаев CPUID signature, публикация metadata, host ID restoration,
  forced-disabled probe. Supplemental opcode audit, не формальная ISA-проверка.
- GCC и GAS ограничены i386. Опциональный CPUID opcode только в guarded BSP
  probe. Нынешний BIOS kernel не требует FPU/SIMD/CMOV/TSC.
- Early MBR CPU guard 16-bit; реальных 8086/186/286 и физического 386 нет в
  аппаратных тестах. Для 16-bit CPU рабочее ядро не реализовано.
- Один образ, UEFI CPU report (64-bit) и BIOS CPU report (32-bit), общий exFAT,
  kernel/MBR/EFI update с сохранением disk ID и таблицы разделов. 32 проверки
  редактора/общего образа, отдельный отказ при неверном BIOS primary master.
- Основные 44 + 32 + 48 assertions, HAL, OSK, modules, CPU unit снова проходят.
- ARMv7/AArch64/RISC-V64: по 7 portable objects compile-only, не готовые BSP.

Основные отчёты: `build/cpu-matrix-report.txt`, `build/cpu-unit-report.txt`,
`build/cpu-regression.log`, `build/unified-report.txt`. Все CPU модели —
эмуляция; никаких гарантий «каждый CPU/каждый PC» эти проверки не создают.

## Неопубликованный development snapshot 0.7-dev: две/три кнопки

- Общая OSK: обход всех 56 ячеек в обоих направлениях, все 95 печатных ASCII,
  Shift, Back, Enter, Tab, CtrlC, Hide/Esc и восстановление панели; обратный обход
  необязателен. Проверены отсутствие renderer, EOF, удержание, release/chord и
  отключение PREVIOUS в двухкнопочном GPIO-helper.
- Unit test кнопок прошёл AddressSanitizer/UndefinedBehaviorSanitizer.
- Реальная shell/exFAT в host harness выполнила 4069 событий NEXT/SELECT без
  буквенных/стрелочных событий: VER, OSK OFF/reopen, MD, WRITE, TYPE, LOCK, EXIT.
  Вывод проверен на ошибки и ожидаемый текст; независимый fsck.exfat: clean.
- QEMU 486 BIOS и OVMF x64 UEFI USB: отдельно 2 кнопки (F6/F7) и 3 кнопки
  (F5/F6/F7), четыре успешных загрузочных сценария. Сценарий набирает VER,
  OSK OFF, ECHO и EXIT/POWEROFF. Ввод по serial, буквами и стрелками не подавался.
  UEFI-инъекции замедлены для firmware input; serial cursor redraw не считается
  обычным CR/LF-логом. Это не тест физического GPIO/debounce или всего железа.
- Повторены 44 основных, 32 edge и 48 fault assertions; 23 CPU-конфигурации
  одного BIOS-бинарника и forced-no-CPUID build; 32 проверки единого образа,
  BIOS/UEFI shared exFAT и отказ от чужого primary-master диска.
- 7 portable-core объектов compile-only для ARMv7/AArch64/RISC-V64.
  Двух/трёхкнопочный BSP-шаблон отдельно compile-only для i386, ARMv7, AArch64,
  RISC-V64. Это **не готовые, не слинкованные и не загруженные порты**.
- Готовые PC output-драйверы требуют текстовую область 80x25. Произвольный экран
  поддерживается интерфейсом, но нуждается в собственном board_osk renderer.

Результаты лежат в build/*report.txt. Политика публикации — RELEASE_POLICY_RU.md;
эта сборка не является стабильным 1.0 и не публиковалась.


## Неопубликованный development snapshot 0.8-dev

- Три native USB сценария: PIIX3, PIIX4 (keyboard на втором root port,
  неподдерживаемая мышь на первом), default auto. Первые два используют
  отдельный USB-only профиль: PS/2 и COM1 input не читаются самим драйвером.
  Отправлены реальные QMP key events, выполнены команды записи/чтения exFAT.
  Отсоединение в auto обнаружено; повторное подключение НЕ сертифицировано.
- HID: 50000 fuzz reports; редактор: 100000 событий против reference model.
  HID/editor/PS2 и inventory проверены ASan/UBSan; PS/2 — mock I/O настоящего
  драйвера, не физическая клавиатура. Отдельно Pause не оставляет Ctrl/NumLock.
- 25 console integration assertions и 16 SOURCE edge assertions.
- Снова прошли 44 основных + 32 edge + 48 fault assertions, 4069 событий
  двухкнопочного host shell, HAL/OSK/modules/CPU unit, 23 CPU-конфигурации,
  32 unified assertions и BIOS disk guard.
- Четыре загрузочных сценария BIOS/UEFI с двумя и тремя кнопками завершились
  успешно на текущей 0.8-dev; отчёт two-button-boot-report.txt.
- По 10 portable-core объектов скомпилированы для ARMv7/AArch64/RISC-V64.
  Это compile-only, не готовые порты.

Полный перечень и границы — CHANGES_0_8_RU.md и NATIVE_USB_RU.md.
QEMU-тесты не подтверждают совместимость физического железа.

## Неопубликованный development snapshot 0.9-dev: fault boundary

Подробные изменения — CHANGES_0_9_RU.md / KERNEL_STABILITY_RU.md. В BIOS C добавлен
compiler stack canary; ранняя IDT, сохранение кадров исключений, собственная GDT,
обычный и emergency TSS для #DF. Диагностика независима от HAL и файловой системы.

12 ранних CPU fault injections на 486/qemu64 проверяют HLT=1, IF=0 и неизменность
SHA-256 диска; #DF также проверяет TR=0020 и ESP в независимом 8 KiB стеке. Для
#GP проверяется настоящий CPU error code FFF8. Canary-тест проходит через
сгенерированный GCC эпилог. Production не содержит fault-test функции.

Полная карта текущих отчётов и список не проверенных отказов — в новых документах.
Ошибку инфраструктуры (например, QEMU без machine=pc) нельзя считать ошибкой ОС
или успешным тестом; итоговые отчёты получены повторным прогоном на нужном стенде.


## 0.11-dev: неблагоприятные сценарии

BUGFIX_0_10_RU.md описывает исправления, новые preflight-лимиты и оставшиеся риски.
Добавлены повреждённые exFAT fixtures, детерминированные мутации с санитайзерами,
ошибки flush, hostile API values, FIFO/links/record locks и source snapshot race.
Физическое отключение питания и аппаратные кэши этим не симулируются.
Чистый том внешнего mkfs.exfat входит в основную регрессию и должен оставаться
доступен для поддержанных операций; extra allocated reservations не освобождаются.
