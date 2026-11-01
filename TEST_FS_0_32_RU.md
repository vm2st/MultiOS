# Проверки MultiOS 0.32-dev-exactnames

Проверки относятся к конкретным сценариям и не означают стабильный Release 1.0, полную совместимость NTFS или сертификацию оборудования. Окончательные результаты находятся в `reports/` поставки.

## Реальный fixture NTFS

`tests/ntfs_case_fixture.py` создаёт через системную libntfs-3g существующие POSIX имена `File.txt`/`file.txt`, каталоги `CaseDir`/`casedir` с разными данными и DATA-потоки `Notes`/`notes`. Это независимый fixture, не production-драйвер и не установка Windows directory case flags.

На прежнем коде 0.31 воспроизведены неправильные чтения file.txt, casedir и notes. Дополнительный тест записи показал, что одного исправления VFS недостаточно: внутренний ignore-case lookup NTFS также мог изменять другой inode. Поэтому исправлены оба слоя.

`tests/ntfs_case.py` проверяет ordinary и ASan/UBSan сборки:

- Точные файлы, каталоги и потоки выбираются раздельно, размеры и все байты совпадают.
- Единственное совпадение без учёта регистра принимается; неоднозначное отклоняется без выдачи данных.
- Replace, write-at через границу сектора, замена ADS, атомарная копия, hardlink, rename и delete сохраняют соседний вариант имени.
- Неоднозначные source/destination/parent для мутаций VFS отклоняются до первой записи. Неоднозначные ADS изменения проходят через undo-отказ и не изменяют DATA-раздел.
- Независимый ntfscat проверяет точные DATA/ADS байты, ntfsfix -n — базовую структуру. Это не Windows chkdsk.

## Аварийная матрица

`ntfs_case_actions.h` содержит восемь операций над case-distinct fixture: replace, write-at, ADS replace, copy, hardlink, rename, delete и resize. `ntfs_case_fault.py` запускает общий real-FS persistence/undo model обычной и sanitizer-сборкой. На write/flush границах моделируются crash/I/O, torn/partial/reordered persistence, прерывание восстановления и конечный undo-бюджет. После восстановления проверяется точное старое либо завершённое новое состояние секторов вне журнала.

Для сравнения NTFS timestamps фиксируются только в тестовом процессе через `MULTIOS_NTFS_TEST_TIME=1`. Время production ОС этим не фиксируется. Число сценариев берётся из журналов; ordinary/sanitizer не удваивают уникальные случаи.

## Регрессии и firmware

Повторяются файловые операции 0.31 на FAT32/exFAT/NTFS, общие NTFS transaction matrices (обычные и compressed), FAT32/exFAT persistence model, почти заполненные тома, active FAT и unit-контракты. Отдельно загружаются текущие BIOS i386 и UEFI x64 с case-collision fixture; содержимое после остановки сверяется независимым ntfscat. Фабричные образы проверяются через UEFI и нативный BIOS/EHCI USB-only boot с записью и повторной загрузкой.

UEFI использует firmware Block I/O и сохраняет Boot Services. Проверка эмулированного USB контроллера firmware не означает наличия собственного xHCI. Новая полная UHCI/OHCI hardware-матрица, физическое отключение питания и Windows chkdsk не заявляются.

## Воспроизведение

Из FULL-SOURCES рядом с images/:

```sh
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/ntfs-case-probe build/ntfs-case-probe-sanitize build/ntfs-case-tx-test build/ntfs-case-tx-sanitize
python3 tests/ntfs_case.py ../images/MultiOS-0.32-usb-ntfs.img
python3 tests/ntfs_case_fault.py
make FS_PROFILE=profiles/filesystems-ntfs3g.mk build/fs-writeops-probe build/fs-writeops-probe-sanitize build/storage-tx-test build/storage-tx-sanitize build/fs-full-probe build/fs-full-probe-sanitize build/fat32-active-probe build/fat32-active-probe-sanitize build/fs-read-probe build/multios-host
python3 tests/fs_writeops.py ../images
python3 tests/storage_tx.py
python3 tests/upstream_ntfs_fault.py
MULTIOS_TEST_COMPRESSED=1 python3 tests/upstream_ntfs_fault.py
python3 tests/fs_full.py ../images
python3 tests/fat32_active.py ../images/MultiOS-0.32-usb-fat32.img
make test-fatfs-bridge test-unlock-contract
make bios-rw
make FS_PROFILE=profiles/filesystems-ntfs3g.mk uefi
python3 tests/ntfs_case_boot.py ../images/MultiOS-0.32-usb-ntfs.img
python3 tests/fs_compat_boot.py ../images
```

Длительный NTFS fault-runner допускает MULTIOS_TEST_TIMEOUT (секунды на один бинарник, по умолчанию 1800). При параллельной нагрузке старый лимит 600 секунд оказался недостаточен; тайм-аут стенда не считается успешным прогоном.

Для firmware нужны QEMU/SeaBIOS/OVMF. В зависимости от установки задайте QEMU_BIOS, QEMU_ROM_DIR, BIOS_ROM и VGA_ROM. Скрипт case_boot устанавливает текущие бинарники в новую расходную копию fixture. Все тесты работают с копиями; SHA256 источников контролируется.
