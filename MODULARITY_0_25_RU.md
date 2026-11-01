# Модульный шаблон MultiPulti / MultiOS

vm2 studios | t.me/v2vproj. 0.25-dev-easymod, UNPUBLISHED.

1. Новичок меняет три настройки и сохраняет новый образ без компилятора.
2. Профиль key=value выбирает УЖЕ собранные провайдеры при старте.
3. Новый C-драйвер: контракт MultiDriver, реестр, список компиляции, пересборка.
4. Новая плата/архитектура: startup/linker/исключения/память и BSP, затем драйверы.
   Пара настроек не переносит x86 на ARM и не создаёт неизвестный контроллер.

## Карта проекта

| Файлы/каталоги | Назначение |
|---|---|
| include/multios/modules.h | ABI1 четырёх типов: output/input/block/power |
| core/modules.c | Валидация ABI, atomic profile parse, rollback open, MODULES |
| core/boot_profile.c | CRC/версия/длина BIOS-профиля, без дискового I/O |
| core/osk.c | Общая OSK и NEXT/SELECT/PREVIOUS |
| platform/pc-bios/registry.h | Реестр собранных BIOS-драйверов и fallback |
| platform/uefi/registry.h | Реестр UEFI-драйверов и fallback |
| drivers/pc-bios, drivers/uefi | Реальные платформенные реализации |
| drivers/templates/output_example.c.txt | Помеченный НЕРАБОЧИЙ пример нового модуля |
| lib/ | VFS, FS-адаптеры, undo/транзакции, shell |
| ports/ntfs3g | Порт NTFS-3G, BIOS allocator/runtime |
| vendor, third_party | Включённые библиотеки/архивы и лицензии |
| tools/customize.py | GUI/CLI, Python standard library, новая копия .img |
| tests/ | Контракты, faults/crash и загрузочные проверки |

open не должен писать на диск; при неуспехе освобождает свои ресурсы. Если
следующий модуль не открылся, менеджер вызывает close в обратном порядке.
Реестры/дескрипторы/ctx живут до конца оболочки. Блочный RW-модуль требует
512-byte read/write и настоящего flush. RO не объявляет writable.
ABI1 — внутренний C-контракт, НЕ переносимый формат исполняемых .drv.
FS-backend выбирается VFS/профилем сборки; это не сменная DLL.
MODULES показывает текущие options и доступные собранные провайдеры.

## Сборка Linux

Python3, make, GCC/binutils с -m32 freestanding, mtools. Для тестов нужны QEMU,
OVMF/SeaBIOS, FS-утилиты. Точные зависимости/команды см. Makefile и tools.

```sh
make FS_PROFILE=profiles/filesystems-ntfs3g.mk uefi build/multios-host
python3 tools/build_bios_ntfs.py
python3 tools/build_bios_rw_image.py ORIGINAL.img NEW.img --efi build/BOOTX64.EFI
python3 tools/customize.py NEW.img --preset safe --output SAFE.img
```

ORIGINAL — чистый prepared image из поставки, НЕ произвольный диск.
Сборочный installer обновляет BIOS/EFI payload и соответствующие исходники,
сохраняя DATA. Старый compact make all — другой ограниченный профиль,
не заменяющий расширенный BIOS NTFS RW. Все лицензии библиотек сохраняются.

## BIOS config

LBA16: MPCFG25\0, LE32 length на+8, version1 на+12, до492 ASCII-байт на+16,
нулевой хвост, CRC32 первых508 байт на+508. Header LBA17 содержит MC25 на+24.
Stage2 помещается в LBA1..15; linker запрещает BSS пересекать0x9e00. MBR
читает16 секторов, последний — конфиг. Нулевой слот = compiled fallback;
ненулевой повреждённый = fail-stop до открытия модулей. CRC не аутентификация.
Видимые BOOT.CFG/BIOS.CFG/SETTINGS.INI синхронизируются tools/customize.py.
