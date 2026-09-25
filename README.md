# Camioneta (Nombre pendiente)

Dispositivo para pruebas de seguridad WiFi y Bluetooth, basado en ESP32-C6. 

Tiene pantalla OLED, 4 botones, buzzer, NeoPixels, GPS y lector de SD. 

No se necesita ninguna PC o conexion a internet

## Hardware

- MCU: ESP32-C6-WROOM-1U-N8/16
- Pantalla: OLED SH1106 128x64, I2C
- GPS: ATGM336H-6N-74, 115200 baudios
- NeoPixel: 9 LEDs
- Botonera: SELECT, UP, DOWN, BACK
- Buzzer y lector de tarjeta SD
- Queda un header libre sin usar: 3V3, TX, RX, GND, GPIO10, GND para *add-ons* 

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
│   ├── GPS Position     (NO probado a fondo)
│   ├── Wardriving       (aun no implementado)
│   ├── Sniffer
│   ├── PC-Mode
│   ├── YModem
│   └── BLE Scanner
├── AJUSTES
│   ├── Configuracion
│   ├── SD Storage
│   ├── NEOPIXEL
│   ├── Terminal
│   └── Con. Remota (FTP)
├── CREDITOS
└── AYUDA
```

## Apps

**Wifi Captive Portal**: Crea una red WiFi con un portal cautivo, para pruebas de ingenieria social. 
El portal puede cargarse desde la SD (para HTML y CSS custom) o usar uno que ya trae por defecto.

**AP Flood**: Llena el entorno de redes WiFi falsas, con nombres que se pueden configurar o se puede lanzar un AP FLood parametrico con la CLI 

**Bluetooth Spam**: Manda anuncios Bluetooth que hacen aparecer los popups de emparejamiento de Apple, Microsoft y Samsung, como hacen herramientas como ESP32Marauder. 
Cada anuncio sale con una direccion distinta para que el celular lo vea como un dispositivo nuevo cada vez. 

**GPS Position**: Muestra la posicion en vivo que da el modulo GPS. Todavia no se ha probado bien.

**Sniffer**: Captura paquetes WiFi del ambiente 

**PC-Mode**: Se conecta la camioneta a una PC por cable y se maneja todo desde una terminal, con comandos para ver el estado del dispositivo, cambiar el color del NeoPixel, tocar el buzzer, ver archivos de la SD, armar la lista de redes falsas, y revisar el registro del portal cautivo.

**YModem**: Transfiere archivos entre la SD y una PC usando el protocolo YMODEM real. 
Se navega la SD con un browser en pantalla para elegir que enviar, o se elige recibir un archivo nuevo desde la PC.

**BLE Scanner**: Busca y muestra los dispositivos Bluetooth que hay cerca (la MAC)

## Ajustes

**Configuracion** —> Ajustes generales del dispositivo.

**SD Storage** —> Para navegar los archivos de la SD.

**NEOPIXEL** —> Para cambiar el color de los NeoPixels a mano.

**Terminal** —> Otra terminal por cable, separada de la de PC-Mode, pensada para controlar el AP Flood, el Sniffer y el Captive Portal.

**Conexion Remota y Servidor FTP** —> Levanta un servidor FTP, se puede utilizar curl, ftp, filezilla o un gestor en LabVIEW para usarlo como interfaz.

**Conexion Remota** —> Inicia un AP y levanta una terminal remota por TCP (puerto 2323), para manejarla sin cable como si fuera la terminal serial  

## Problemas conocidos

- GPS Position: no se ha probado a fondo. Lo unico que sabemos es que el modulo prende y da señal, pero no se ha confirmado que la pantalla muestre bien la posicion.
- Wardriving: esta reservado en el menu pero todavia no se le programo nada por los mismos problemas de GPS 

