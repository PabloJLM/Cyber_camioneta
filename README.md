# Camioneta

Dispositivo de bolsillo para pruebas de seguridad WiFi y Bluetooth, basado en ESP32-C6. Tiene pantalla OLED, botonera de 4 botones, buzzer, NeoPixels, GPS y lector de SD. Todo se maneja desde el menu, sin necesidad de una PC.

## Hardware

- MCU: ESP32-C6-WROOM-1U-N8
- Pantalla: OLED SH1106 128x64, I2C
- GPS: ATGM336H-6N-74, 115200 baudios
- NeoPixel: 9 LEDs
- Botonera: SELECT, UP, DOWN, BACK
- Buzzer y lector de tarjeta SD
- Queda un header libre sin usar: 3V3, TX, RX, GND, GPIO10, GND (es el UART del modulo principal, no el que usa el GPS)

### Pines

| Funcion    | Pin |
|------------|-----|
| SDA        | 6   |
| SCL        | 7   |
| SELECT     | 1   |
| UP         | 15  |
| DOWN       | 23  |
| BACK       | 22  |
| NEOPIXEL   | 11  |
| BUZZER     | 2   |
| LED1       | 3   |
| GPS ON     | 8   |
| CD (SD)    | 40  |

## Estructura del menu

```
MENU
├── APPS
│   ├── Wifi Captive Portal
│   ├── AP Flood
│   ├── Bluetooth Spam
│   ├── GPS Position
│   ├── Wardriving       (todavia no hace nada)
│   ├── Sniffer
│   ├── PC-Mode
│   └── BLE Scanner
├── AJUSTES
│   ├── Configuracion
│   ├── SD Storage
│   ├── NEOPIXEL
│   └── Terminal
├── CREDITOS
└── AYUDA
```

## Apps

**Wifi Captive Portal** — Crea una red WiFi con un portal cautivo, para pruebas de ingenieria social. El portal puede cargarse desde la SD o usar uno que ya trae por defecto.

**AP Flood** — Llena el entorno de redes WiFi falsas, con nombres que se pueden configurar.

**Bluetooth Spam** — Manda anuncios Bluetooth que hacen aparecer los popups de emparejamiento de Apple, Microsoft y Samsung, como hacen herramientas como ESP32Marauder. Cada anuncio sale con una direccion distinta para que el celular lo vea como un dispositivo nuevo cada vez. Faltan dos fabricantes mas (Google y Flipper Zero) por un bug que sigue en revision, ver abajo. Ojo: no incluye el truco de hacerse pasar por un AirTag, eso se dejo fuera a proposito porque no es una simple molestia, es aprovecharse de una alerta de seguridad real que le avisa a la gente que la estan rastreando.

**GPS Position** — Muestra la posicion en vivo que da el modulo GPS. Todavia no se ha probado bien, ver abajo.

**Sniffer** — Captura paquetes WiFi que andan por el aire.

**PC-Mode** — Se conecta la camioneta a una PC por cable y se maneja todo desde una terminal, con comandos para ver el estado del dispositivo, cambiar el color del NeoPixel, tocar el buzzer, ver archivos de la SD, armar la lista de redes falsas, y revisar el registro del portal cautivo.

**BLE Scanner** — Busca y muestra los dispositivos Bluetooth que hay cerca.

## Ajustes

**Configuracion** — Ajustes generales del dispositivo.

**SD Storage** — Para navegar los archivos de la SD.

**NEOPIXEL** — Para cambiar el color de los NeoPixels a mano.

**Terminal** — Otra terminal por cable, separada de la de PC-Mode, pensada para controlar el AP Flood, el Sniffer y el Captive Portal.

## Problemas conocidos

- Bluetooth Spam: los tipos Google Fast Pair y Flipper Zero hacen que el dispositivo se reinicie solo en hardware real. El codigo ya esta escrito pero se dejo apagado hasta encontrar la causa real del problema.
- GPS Position: no se ha probado a fondo. Lo unico que sabemos es que el modulo prende y da señal, pero no se ha confirmado que la pantalla muestre bien la posicion.
- Wardriving: esta reservado en el menu pero todavia no se le programo nada.

## Estructura de archivos

Los archivos `.cpp` van al mismo nivel que `camioneta.ino`, porque asi es como el Arduino IDE los reconoce y los compila. Los `.h` de cada uno se guardan en carpetas separadas segun la categoria: `Apps/`, `Ajustes/`, `Ayuda/`, `Drivers/`, `Estaticos/`.
