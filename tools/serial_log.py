"""Leest een seriele poort uit en schrijft alles naar een logbestand.

Gebruik: python serial_log.py COM6 115200 C:\pad\naar\log.txt
Blijft doorlopen bij tijdelijke disconnects (USB re-enumeratie).

Let op: DTR/RTS worden bewust NIET geactiveerd. pyserial zet die op Windows
standaard aan bij het openen van de poort, wat op ESP32-boards een reset
(of zelfs bootloader-modus) triggert — elke herverbinding van deze tool
zou het board dan opnieuw laten opstarten.
"""
import sys
import time

import serial

port_name, baud, log_path = sys.argv[1], int(sys.argv[2]), sys.argv[3]

with open(log_path, "a", encoding="utf-8", errors="replace") as log:
    while True:
        port = serial.Serial()
        port.port = port_name
        port.baudrate = baud
        port.timeout = 1
        port.dtr = False   # instellen vóór open(): geen reset-puls
        port.rts = False
        try:
            port.open()
            log.write(f"--- verbonden met {port_name} ---\n")
            log.flush()
            while True:
                data = port.read(port.in_waiting or 1)
                if data:
                    log.write(data.decode("utf-8", errors="replace"))
                    log.flush()
        except (serial.SerialException, OSError) as exc:
            log.write(f"--- verbinding weg ({exc}), opnieuw proberen ---\n")
            log.flush()
            time.sleep(2)
        finally:
            try:
                port.close()
            except Exception:
                pass
