import sys
import os


def resource_path(filename):
    base = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, filename)


def find_ch340():
    try:
        import serial.tools.list_ports
        for port in serial.tools.list_ports.comports():
            desc = (port.description or "").upper()
            if "CH340" in desc or "CH341" in desc:
                return port.device
    except Exception:
        pass
    return None


def main():
    print("SenseCap Indicator — Firmware flashen")
    print("=" * 40)
    print()
    print("Stop de seriële logger als die open staat.")
    print()

    print("Zoeken naar apparaat (CH340)...")
    port = find_ch340()
    if not port:
        print()
        print("Apparaat niet gevonden.")
        print("Zorg dat de SenseCap Indicator via USB is aangesloten.")
        input("\nDruk Enter om af te sluiten...")
        sys.exit(1)

    print(f"Gevonden op: {port}")
    print("Flashen... (dit duurt ~30 seconden)")
    print()

    import esptool
    esptool.main([
        "--chip",  "esp32s3",
        "--port",  port,
        "--baud",  "460800",
        "write_flash",
        "0x0000",  resource_path("bootloader.bin"),
        "0x8000",  resource_path("partitions.bin"),
        "0x10000", resource_path("firmware.bin"),
    ])

    print()
    input("Flashen geslaagd! Het apparaat herstart nu. Druk Enter om af te sluiten...")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nGeannuleerd.")
        sys.exit(1)
    except SystemExit:
        raise
    except Exception as exc:
        print(f"\nFout: {exc}")
        input("\nDruk Enter om af te sluiten...")
        sys.exit(1)
