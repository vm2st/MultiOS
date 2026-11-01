# Отчёт по изменениям MultiOS 0.8-dev

**MultiPulti · vm2 studios · t.me/v2vproj**

**Неопубликованная development-сборка. Первая публичная версия — только
стабильный Release 1.0.** Ничего не выгружалось в публичные репозитории/каналы.

Запрос «добавить всё» разбит на работающие подсистемы. Ниже нет обещания, что
все ранее обсуждавшиеся возможности уже реализованы. Крупность проекта — это
разделение кода, контракты, тесты и документация, не искусственное раздувание.

| № | Изменение | Где | Проверка / границы |
| --- | --- | --- | --- |
| 01 | PCI mechanism #1 inventory | drivers/pc-bios/pci.c, core/hardware.c | PCI devices найдены в QEMU; 128 записей; BAR не перепрограммируются сканером, устройства без нужды не включаются |
| 02 | PIT channel 2 bounded delay | drivers/pc-bios/pci.c | Использован в native UHCI reset; без TSC/FPU/IRQ. Не универсальный monotonic clock |
| 03 | Native Intel UHCI host | drivers/pc-bios/uhci.c | Собственные frame list/QH/TD, control/interrupt IN, DMA; PIIX3/PIIX4 в QEMU, не physical certification |
| 04 | USB HID Boot enumeration | drivers/pc-bios/uhci.c | Descriptor/address/configuration/protocol/idle; direct root ports, один keyboard на controller, до четырёх controller; не hubs/USB disks |
| 05 | Переносимый HID decoder | core/hid.c, include/multios/hid.h | US/modifiers/caps/num/keypad/6KRO, held/duplicate/error reports, очередь; 50000 fuzz reports; нет generic NKRO/LED/USB autorepeat |
| 06 | Мультиплексор ввода и USB-only proof | drivers/pc-bios/keys.c, profiles/pc-bios-usb-only.mk | auto / usb-only / ps2-only; USB-only не читает PS2/COM1 input. Vendor/class ограничения явные |
| 07 | Диагностика и отключение USB | core/hardware.c, lib/shell.c | DEVICES/PCI/USB; счётчики native reports/errors/disconnects. Отсоединение проверено через RAM; reconnect НЕ принят |
| 08 | USB lifecycle cleanup | drivers/pc-bios/keys.c, power.c | Halt controller при EXIT и откате запуска модуля; DMA buffers static, не выгружаются во время работы |
| 09 | Расширенный PS/2 | drivers/pc-bios/keys.c | L/R Shift, Right Ctrl, Caps/Num guard, keypad, Home/End/Delete, F8/F9, пропуск Pause. Mock I/O настоящего драйвера и BIOS regression |
| 10 | Переносимый редактор строки | core/editor.c, include/multios/editor.h | Вставка, удаление, курсор, Home/End, Ctrl-аналоги; 100000 событий против reference model, overflow/cancel; scroll edge ограничен |
| 11 | История восьми команд | lib/shell.c | F8/F9, Ctrl+P/N, черновик, HISTORY и подавление повторов; host integration и native USB F8 |
| 12 | Постраничный MORE | lib/shell.c | F6 next/F7 quit без обхода OSK; host integration; выбран порог для PC text output |
| 13 | Сценарии SOURCE | lib/shell.c | ASCII/CRLF/comments, 4 KiB, recursion cap 4, stop-on-error с номером строки; host integration; не DOS BAT |
| 14 | Единый образ и документация | tools/mkunified.py, *.md | В образе исходники, инструкции и один общий exFAT; BIOS/UEFI bundles проверяются отдельно; редактирование выключенной копии |
| 15 | Регрессионный и sanitizer набор | tests/* | HID/editor/PS2, console integration, BIOS/UEFI, CPU matrix, fsck, fault injection, portability; точные журналы — build/*report.txt |

## Что остаётся НЕ реализовано

- BIOS INT13/INT10/INT16 bridge для работы ОС через real-mode firmware;
- собственные OHCI/EHCI/xHCI, hubs и USB storage, AHCI/NVMe;
- стабильный reconnect USB в ходе работы; никаких обещаний по всем USB-портам;
- native x64 после ExitBootServices, UEFI IA32, 16-bit 8086/286 branch;
- готовые ROM/BSP для произвольных «голых» плат, ARM/RISC-V bootable ports;
- dynamic ELF/.sys driver loader, PCI resource allocator, IOMMU/memory isolation;
- scheduler/processes/syscalls, EXE/COM/ELF app loader, DOS binary compatibility;
- Unicode/русская раскладка, GPT/несколько томов/64-bit file offsets;
- production-ready exFAT recovery/тесты реального обрыва питания;
- native ACPI poweroff BIOS-ветки, сеть/звук/графическая среда/сенсорный ввод.

**USB-клавиатура в BIOS уже не только план, но это конкретная ограниченная
UHCI-реализация, а не поддержка всех современных PC.** Работы из оставшегося
списка — следующие отдельные этапы до решения о Release 1.0.

## Как читать подтверждения

- native-usb-report.txt: отдельный USB-only kernel для двух контроллеров и
  default auto profile; запись/чтение exFAT отправлены USB-клавиатурой.
- dev08-keyboard-unit-report.txt / dev08-sanitizer-report.txt: детерминированные
  unit/fuzz tests, не аппаратные испытания.
- console-features-report.txt: host harness shell/exFAT, не загрузка ОС.
- cpu-matrix-report.txt: QEMU CPU models, не физический 386 или все CPU.
- unified-report.txt / two-button-boot-report.txt: QEMU BIOS/UEFI и input workflows.
- dev08-cross-report.txt: только компиляция общего C, не линковка/загрузка портов.

Физическое оборудование не проверялось. QEMU outcomes не подменяют стабильность,
сертификацию USB-IF, реальную совместимость платы и сохранность пользовательских
данных. exFAT-записи всё ещё экспериментальные; по умолчанию доступ read-only.
