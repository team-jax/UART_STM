#!/Users/ijaejun/.platformio/penv/bin/python
"""로봇 손 UART 명령 전송 (터미널 프로그램 대용)

사용법:
    ./tools/hand.py grip          # 손 쥐기  (AA F0 00 01 F1)
    ./tools/hand.py open          # 손 펴기  (AA F0 00 00 F0)
    ./tools/hand.py 4 90          # 개별 서보: ID4를 90.0도로
    ./tools/hand.py 0 123.5       # ID0을 123.5도로 (0.1도 단위)

포트는 /dev/cu.usb* 를 자동으로 찾음. 여러 개면 첫 번째 사용.
직접 지정: ./tools/hand.py grip --port /dev/cu.usbserial-0001
"""
import sys
import glob
import serial

BAUD = 115200


def find_port(argv):
    if "--port" in argv:
        i = argv.index("--port")
        port = argv[i + 1]
        del argv[i:i + 2]
        return port
    ports = sorted(glob.glob("/dev/cu.usb*"))
    if not ports:
        sys.exit("USB 시리얼 포트 없음. 어댑터 연결 후 `ls /dev/cu.*` 로 확인")
    if len(ports) > 1:
        print(f"포트 여러 개 발견 {ports} → {ports[0]} 사용 (--port 로 지정 가능)")
    return ports[0]


def frame(motor_id, high, low):
    checksum = (motor_id + high + low) & 0xFF
    return bytes([0xAA, motor_id, high, low, checksum])


def main():
    argv = sys.argv[1:]
    port = find_port(argv)

    if not argv:
        sys.exit(__doc__)

    cmd = argv[0].lower()
    if cmd == "grip":
        data = frame(0xF0, 0x00, 0x01)
    elif cmd == "open":
        data = frame(0xF0, 0x00, 0x00)
    else:
        motor_id = int(argv[0])
        angle = float(argv[1])
        if not (0 <= motor_id <= 10):
            sys.exit("모터 ID는 0~10")
        if not (0.0 <= angle <= 180.0):
            sys.exit("각도는 0~180")
        value = int(round(angle * 10))
        data = frame(motor_id, (value >> 8) & 0xFF, value & 0xFF)

    with serial.Serial(port, BAUD, timeout=0.5) as ser:
        ser.reset_input_buffer()
        ser.write(data)
        ser.flush()
        resp = ser.read(1)
    print(f"{port} ← {' '.join(f'{b:02X}' for b in data)}")
    if resp == b"\x55":
        print("보드 응답: ACK — 보드가 명령을 정상 수신함")
    elif resp == b"\xEE":
        print("보드 응답: NAK — 프레임이 도착했지만 checksum/ID 오류")
    elif resp:
        print(f"보드 응답: {resp.hex().upper()} (예상 밖의 값)")
    else:
        print("보드 응답 없음 — UART 경로가 연결 안 됐거나 구펌웨어(ACK 미지원)")


if __name__ == "__main__":
    main()
