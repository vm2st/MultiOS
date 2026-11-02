# MultiOS 0.9-dev: отчёт по этапу стабилизации

**MultiPulti · vm2 studios | t.me/v2vproj · UNPUBLISHED**

Это не стабильный Release 1.0. Проект не публиковался. Цель данного этапа —
обнаруживать аварии собственного BIOS i386-ядра и получать диагностируемый
fail-stop вместо молчаливого зависания/сброса. Новые аппаратные драйверы в этом
этапе не добавлялись. Подробные ограничения — KERNEL_STABILITY_RU.md.

| № | Изменение | Реализация / подтверждение |
|---|---|---|
| 01 | Ранняя установка обработчиков | start.S вызывает fault_init сразу после BSS, до модулей |
| 02 | IDT на 256 векторов | exceptions.c; layout и 256 assembly stubs проверены аудитом |
| 03 | Кадр исключения | exceptions.S/.h; CPU error code, регистры, сегменты, static assertions offsets |
| 04 | Независимый panic | VGA + COM1, без HAL, heap, filesystem или ввода |
| 05 | Диагностика | vector/error/EIP/CS/EFLAGS/registers; предусмотрен CR2 для #PF, но #PF отдельно не испытывался |
| 06 | Emergency double fault | Собственная GDT, два TSS, task gate и 8 KiB stack; QMP проверяет TR и фактический ESP |
| 07 | Fail-stop | CLI/HLT, IF=0 и HLT=1 подтверждены QMP; автоматического recovery/reboot нет |
| 08 | Compiler stack canary | BIOS C: stack-protector-strong/global; panic исключён из instrumentation; guard фиксированный, не security secret |
| 09 | Настоящие fault injections | #UD/#DE/#GP/#DF/INT81/canary, по шесть сценариев на 486 и qemu64; изолированные TEST kernels |
| 10 | Диск при ранней аварии | SHA-256 тестового диска до/после совпадает; это не гарантия атомарности аварии посреди записи |
| 11 | Структурный аудит | Размеры IDT/TSS/GDT/stack, отсутствие unresolved symbols и TEST-функции в production, canary в shell/exFAT, i386 ISA |
| 12 | Повторная регрессия | Файловые, CPU, shared BIOS/UEFI image, USB и кнопочные сценарии; отдельные отчёты |
| 13 | Документация и единый образ | Встроен DOCS/GUARD.MD; исходники и описания текущего этапа в SOURCES/PORTING.ZIP |
| 14 | Политика 1.0 | Зафиксированы полнота заявленных возможностей, feature freeze, RC, physical tests и последующее bugfix/security сопровождение |
| 15 | Зависимости host-сборки | Исправлено отсутствие header dependencies у host/harness; изолированный тест меняет только brand.h и проверяет пересборку и обратную воспроизводимость |
| 16 | Независимая пересборка | boot.bin/kernel.bin/kernel.elf/BOOTX64.EFI/multios-host совпадают побайтно с чистой сборкой тем же toolchain; весь образ случайный disk ID не воспроизводит |

## Точные группы проверок

- dev09-reproducibility-report.txt и dev09-build-dependency-report.txt: независимая пересборка и header dependency regression.
- kernel-fault-report.txt: 12 ранних аварийных инъекций на machine=pc. Также
  первоначально пройден такой же набор на q35; итоговый отчёт содержит pc.
- kernel-guard-audit-report.txt: supplemental audit, не формальная верификация.
- dev09-regression-report.txt: 44 основных + 32 edge + 48 injected-write-fault
  assertions, HAL/OSK/modules/CPU/HID/editor/PS2 и host two-button shell.
- dev09-console-report.txt и dev09-source-edges-report.txt: 25 и 16 assertions.
- dev09-sanitizer-report.txt: HID/editor/PS2/hardware inventory, ASan + UBSan.
- cpu-matrix-report.txt: 23 конфигурации одного kernel + forced-no-CPUID build.
- unified-report.txt: 32 проверки редактирования и shared BIOS/UEFI exFAT.
- bios-disk-guard-report.txt: отказ от чужого IDE primary master.
- native-usb-report.txt: три native UHCI сценария, включая USB-only профиль.
- two-button-boot-report.txt: BIOS/UEFI, две и три кнопки, четыре сценария.
- dev09-cross-report.txt: по 10 portable-core объектов для ARMv7/AArch64/RISC-V64,
  только компиляция. Это не готовые порты и не перенос x86 exception layer.

## Что данный этап не решает

Нет memory isolation/paging/guard pages, recovery после произвольной порчи
памяти, остановки всего DMA при panic, production-ready exFAT, BIOS INT13 bridge,
AHCI/NVMe, OHCI/EHCI/xHCI, USB storage/hubs или подтверждённого USB reconnect.
UEFI — приложение с Boot Services; native x64 пока нет. Физическое железо не
проверялось. Нельзя делать вывод «все CPU, все PC, данные всегда сохранны».

Production kernel не содержит TEST-функции и сценариев запуска аварий. Отдельные
fault .img в архив не включены; получить их можно только сборкой тестовых целей.
