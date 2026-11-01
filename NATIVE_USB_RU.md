> **USB 0.26-dev-usb12:** актуальная поддержка UHCI/OHCI/EHCI, границы и сборка описаны в [USB_0_26_RU.md](USB_0_26_RU.md). Ниже сохранены исторические инструкции. Не Release 1.0.

# MultiOS 0.8-dev: USB-клавиатура в BIOS-ветке

**MultiPulti · vm2 studios · t.me/v2vproj**

**Не опубликовано.** Это development snapshot, не стабильный Release 1.0.
Запускайте сначала в VM и на резервных копиях. USB-драйвер управляет DMA и
контроллером напрямую; защиты памяти/IOMMU здесь нет. Реальные платы не испытаны.

## Что именно добавлено

Собственный драйвер **Intel UHCI** в 32-битном BIOS-ядре. Это не использование
UEFI и не вызовы BIOS INT 16h. Драйвер отключает Intel USB Legacy emulation,
берёт контроллер под своё управление, строит DMA frame list/QH/TD, перечисляет
подключённое устройство и принимает HID Boot Keyboard reports.

Поддерживаемый начальный набор:

- PCI configuration mechanism #1, class `0c/03/00`, vendor `8086`, I/O BAR4;
- до четырёх выбранных UHCI-контроллеров, два root-порта на каждом;
- одна поддерживаемая клавиатура на контроллер, **напрямую в root-порту**;
- low/full-speed, HID subclass 1 protocol 1, interrupt IN endpoint на 8 байт;
- первая USB configuration, descriptor не более 500 байт;
- SET_ADDRESS, GET_DESCRIPTOR, SET_CONFIGURATION, SET_PROTOCOL(boot), SET_IDLE;
- US ASCII, Shift, Ctrl, Caps Lock, Num Lock, цифровой блок, Enter, Backspace;
- стрелки, Home/End/Delete, F2, F5/F6/F7 и F8/F9;
- bounded очередь событий, подавление повторов одинакового held-report,
  обработка 6KRO/error reports и ошибок transfer;
- обнаружение отключения клавиатуры и очистка её очереди;
- halt DMA при остановке ОС и откате запуска модулей.

Задержки старта/сброса используют PIT channel 2, не требуют TSC/CPUID/FPU.
Frame polling имеет программные пределы ожидания. Это не RTOS и не гарантия
максимального времени на произвольном неисправном железе. Прерывания и scheduler
для USB не включены: пока используется polling с высокой загрузкой CPU.

**UHCI — USB 1.x host interface. Это не универсальная поддержка USB 2/3/4.**
USB-клавиатура может подключаться к порту USB 2, но для этой BIOS-ветки её путь
должен обслуживаться поддержанным UHCI, а не только EHCI/xHCI. Форма разъёма и
надпись «USB» на клавиатуре не определяют тип контроллера компьютера.

## Чего нет

- OHCI, EHCI, xHCI и не-Intel UHCI;
- внешних USB-хабов, hub-пути внутри монитора/док-станции;
- USB-накопителей и загрузочного блочного драйвера USB для собственного ядра;
- Bluetooth, произвольного HID report descriptor/NKRO, gaming/vendor protocols;
- обновления LED Caps/Num/Scroll и USB auto-repeat при удержании клавиши;
- гарантии работы всех composite-устройств или всех допустимых USB descriptors;
- принятого теста повторного подключения во время работы: probe-код есть,
  но сценарий QMP device_add после удаления завис в использованном стенде.
  Причина не установлена; reconnect не объявляется проверенной возможностью.

Клавиатуру **подключайте до загрузки**. Отсоединение распознаётся; возможность
подключить обратно без перезапуска пока не сертифицирована. Удержание F6/F7
намеренно не должно превращаться в поток нажатий: для экранной клавиатуры
используйте отдельные нажатия. GPIO debounce остаётся задачей BSP.

## Профили ввода

Модуль `pc-input-auto` заменяет прежнее имя `pc-ps2-com1` в BIOS-профиле:

| input.options | Поведение |
| --- | --- |
| пусто или `auto` | Native USB + PS/2 + COM1 input |
| `usb-only` | Только native USB; PS/2 и COM1 input вообще не читаются |
| `ps2-only` | Без инициализации native USB; PS/2 + COM1 |

Готовый compatibility-профиль: `make all PC_PROFILE=profiles/pc-bios-ps2-only.mk`.
Для возврата к автоматическому вводу — `make all` без PC_PROFILE.

Выход в COM1 остаётся доступен во всех трёх режимах. BIOS-профиль **скомпилирован**
в kernel; BOOT.CFG в едином образе по-прежнему относится к UEFI-ветке.

Включать USB Legacy вручную для native UHCI не требуется: драйвер забирает
поддержанный контроллер у прошивки. Если нужен старый firmware-совместимый путь,
используйте отдельный `ps2-only` профиль, не обещая, что прошивка всех плат
эмулирует PS/2 одинаково.

Нативный ввод не меняет ограничений диска: BIOS-файлы всё ещё читаются через
ATA primary master с проверкой MBR disk ID. **USB-клавиатура — не USB storage.**

## Проверка

`tests/native_usb.py` отправляет клавиши через QEMU USB keyboard, а не serial.
Два сценария используют специальный `usb-only` kernel, где функции чтения
PS/2/COM1 не вызываются; это отдельно проверено mock-I/O тестом настоящего
`drivers/pc-bios/keys.c`.

- PIIX3 UHCI: команды, Shift, редактирование Left/Delete, история F8,
  native F5/F6/F7, PCI inventory, запись/чтение exFAT, штатный EXIT.
- PIIX4 UHCI: клавиатура во втором root-порту, мышь в первом, тот же файловый
  workflow; мышь отвергается как неподдерживаемое устройство, не поддерживается.
- Default auto profile: native USB workflow; отключение подтверждено по живому
  native inventory в RAM. Затем QMP завершает VM после SYNC: это не тест EXIT.

Это **эмуляционные**, а не физические испытания. Нельзя выдавать наличие USB
Legacy у SeaBIOS за результат native-драйвера; поэтому и нужен usb-only профиль.

Воспроизведение (Linux/WSL, зависимости как в README_RU.md):

```bash
make all PC_PROFILE=profiles/pc-bios-usb-only.mk
cp build/multios.img build/usb-only-proof.img
make all uefi multipulti
QEMU=/path/to/qemu-system-x86_64 python3 tests/native_usb.py
```

Нужен QEMU с legacy machine `pc` и UHCI devices; использованный BIOS-стенд —
QEMU 5.2 TCG. Для обычного запуска native USB:

```bash
qemu-system-x86_64 -machine pc -m 32M \
  -drive file=build/MultiOS-MultiPulti.img,format=raw,if=ide,index=0 \
  -device piix3-usb-uhci,id=uhci \
  -device usb-kbd,bus=uhci.0,port=1 -serial stdio
```

Команды в ОС:

```text
USB
DEVICES
PCI
CPU
MODULES
```

UEFI-ветка сохраняет firmware SimpleTextInput, включая доступные прошивке USB
клавиатуры. Она **не запускает native UHCI поверх UEFI USB stack**. `USB` в UEFI
честно сообщает об отсутствии native UHCI, а не об отсутствии firmware input.

## Спецификации и авторство

Реализация — собственный код vm2 studios. Использованы описания протоколов,
не скопирован код чужих ОС:

- USB-IF HID 1.11, Appendix B и SET_PROTOCOL:
  https://www.usb.org/sites/default/files/hid1_11.pdf
- Обзор UHCI register/TD/QH layout (вторичный источник):
  https://wiki.osdev.org/Universal_Host_Controller_Interface

Лицензии GNU-EFI сохранены отдельно в THIRD_PARTY_NOTICES.txt. Наличие этих
спецификаций не означает сертификацию USB-IF или аппаратную совместимость всех
контроллеров.
