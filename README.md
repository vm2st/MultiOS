> **Текущая 0.28-dev-usb-hardening:** [USB_STORAGE_0_28_RU.md](USB_STORAGE_0_28_RU.md), [TEST_USB_0_28_RU.md](TEST_USB_0_28_RU.md). UHCI/OHCI/EHCI, BOT STALL/short packet hardening, ограниченный RO-кеш NTFS, защита от потери отложенной ошибки. Неопубликованная разработка, не стабильный Release 1.0. Ниже — история.

> **Актуальная 0.27-dev-usb-bot:** [USB_STORAGE_0_27_RU.md](USB_STORAGE_0_27_RU.md), [TEST_USB_0_27_RU.md](TEST_USB_0_27_RU.md). Нативные USB BOT диски в BIOS, FAT32/exFAT/NTFS experimental RW, реальная BIOS USB-загрузка. Не полная USB-совместимость и не стабильный Release 1.0. Все следующие version notes исторические.

> **USB 0.26-dev-usb12:** актуальная поддержка UHCI/OHCI/EHCI, границы и сборка описаны в [USB_0_26_RU.md](USB_0_26_RU.md). Ниже сохранены исторические инструкции. Не Release 1.0.

> **Актуальная 0.25-dev-easymod:** начните с EASY_START_RU.md и MODULARITY_0_25_RU.md. Ниже исторические инструкции. UNPUBLISHED, НЕ Release1.0.

> **Current / текущая сборка: 0.21-dev-ntfsstreams.** ADS, hardlinks, проверяемый COPYADS, ABI ФС 5. См. FILESYSTEMS_0_21_RU.md. Не Release 1.0; старые разделы ниже — история.

> **Current build 0.20-dev-fileio:** random write/resize, filesystem ABI 4, exFAT ValidDataLength safety fix and large-file host/UEFI audit. See FILESYSTEMS_0_20_RU.md. Older sections below are historical; NOT Release 1.0.

> **0.19 UEFI baseline fix:** firmware and NTFS library explicitly target base x86-64, not distribution defaults such as x86-64-v2. GNU-EFI is built from the included source archive by default; see FILESYSTEMS_0_19_RU.md. The development build remains unpublished and is not Release 1.0.

# MultiOS 0.19-dev-stagedfs / MultiPulti

vm2 studios | t.me/v2vproj — UNPUBLISHED DEVELOPMENT, NOT Release 1.0.

Current change: staged/resumable, verified data copy beyond one undo transaction. Read **FILESYSTEMS_0_19_RU.md** and **reports/stagedfs-0.19/README_RU.md**. Prior 0.18 notes below are historical.

# MultiOS / MultiPulti — vm2 studios | t.me/v2vproj

**0.18-dev-unifiedfs · UNPUBLISHED DEVELOPMENT · NOT stable Release 1.0.**

Начните с **FILESYSTEMS_0_18_RU.md**: актуальные изменения, сборка, запуск и границы поддержки.

- FAT32/exFAT: единые FatFs read/write/directory/free-space callbacks.
- NTFS x64 UEFI: единые NTFS-3G read/write/mount/directory/free-space callbacks.
- Проверены NTFS compressed RW, sparse >5 ГиБ, append за 4 ГиБ.
- Новые образы: undo на 4096 секторов, старые 128-секторные остаются совместимыми.
- BIOS NTFS всё ещё RO; BIOS runtime не проверен в этой среде.
- UEFI сохраняет Boot Services. Нет гарантии поддержки всех ПК/процессоров.
- Объединённый NTFS-enabled EFI — GPL-2.0-or-later, соответствующие исходники включены.
- Только копии образов; это не полная стабильная поддержка всех свойств NTFS.

`make upstream-image UPSTREAM_DATA_FS=fat32 UPSTREAM_OUTPUT=build/new.img`

Документы предыдущих версий и reports/upstream-fs — история. Текущие отчёты: reports/unifiedfs-0.18.
