#!/Users/ijaejun/.platformio/penv/bin/python
"""손가락 서보 실시간 캘리브레이션 (ST-Link/SWD 경유 — UART/어댑터 불필요)

사용법: 보드 USB 연결 후
    ./tools/calibrate.py

키 조작 (한 글자씩, Enter 불필요):
    w / W : 각도 +5° / +1°     ← 손가락이 즉시 움직임
    s / S : 각도 -5° / -1°
    o     : 지금 위치를 "펴기(OPEN)" 각도로 저장
    g     : 지금 위치를 "쥐기(GRIP)" 각도로 저장
    1~7   : 손가락 선택 (1=F1_A 2=F1_B 3=F2_A 4=F2_B 5=F3_A 6=F3_B 7=FINGER_ROT)
    n / p : 다음 / 이전 손가락
    h     : 모든 손가락을 저장된 OPEN 위치로 (정렬 확인용)
    q     : 종료 → 저장값 출력, main.c 반영/플래시 여부 질문

주의: 캘리브레이션 중 보드의 파란 버튼은 누르지 말 것 (값이 덮어써짐).
"""
import re
import socket
import subprocess
import sys
import termios
import time
import tty
from pathlib import Path

OPENOCD = Path.home() / ".platformio/packages/tool-openocd/bin/openocd"
SCRIPTS = Path.home() / ".platformio/packages/tool-openocd/openocd/scripts"
PIO     = Path.home() / ".platformio/penv/bin/pio"
ROOT    = Path(__file__).resolve().parent.parent
MAIN_C  = ROOT / "src" / "main.c"

CCR_MIN, CCR_MAX = 500, 2500
STEP_BIG, STEP_SMALL = 5.0, 1.0

FINGERS = [
    # (이름, 핀, htim, 채널, CCR 레지스터 주소)
    ("FINGER1_A", "PA6", "&htim3", "TIM_CHANNEL_1", 0x40000434),
    ("FINGER1_B", "PA7", "&htim3", "TIM_CHANNEL_2", 0x40000438),
    ("FINGER2_A", "PB0", "&htim3", "TIM_CHANNEL_3", 0x4000043C),
    ("FINGER2_B", "PB1", "&htim3", "TIM_CHANNEL_4", 0x40000440),
    ("FINGER3_A", "PB6", "&htim4", "TIM_CHANNEL_1", 0x40000834),
    ("FINGER3_B", "PB7", "&htim4", "TIM_CHANNEL_2", 0x40000838),
    # FINGER_ROT: PA0(USER 버튼)은 GPIO 입력으로 이미 점유돼 있어 대신 사용.
    # main.c의 MOTOR_FINGER_ROT(id 10)와 동일 핀 — fingerTable(제스처 그룹)에는
    # 속하지 않는 개별 서보라서 'main.c 반영' 자동 패치는 못 찾고 수동 반영 안내만 뜸.
    ("FINGER_ROT", "PB8", "&htim4", "TIM_CHANNEL_3", 0x4000083C),
]


def angle_to_ccr(angle):
    return int(round(CCR_MIN + angle / 180.0 * (CCR_MAX - CCR_MIN)))


def ccr_to_angle(ccr):
    return (ccr - CCR_MIN) * 180.0 / (CCR_MAX - CCR_MIN)


class OpenOCD:
    """openocd를 백그라운드로 띄우고 telnet(4444)으로 명령 전달"""

    def __init__(self):
        self.proc = subprocess.Popen(
            [str(OPENOCD), "-s", str(SCRIPTS),
             "-f", "interface/stlink.cfg", "-f", "target/stm32f4x.cfg"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self.sock = None
        for _ in range(40):
            try:
                self.sock = socket.create_connection(("127.0.0.1", 4444), timeout=2)
                break
            except OSError:
                if self.proc.poll() is not None:
                    sys.exit("openocd 실행 실패 — 보드 USB 연결과 다른 디버거 세션 종료를 확인")
                time.sleep(0.25)
        if self.sock is None:
            self.close()
            sys.exit("openocd telnet(4444) 연결 실패")
        self.sock.settimeout(3)
        self._read_prompt()

    def _read_prompt(self):
        buf = b""
        while not buf.rstrip().endswith(b">"):
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("openocd 연결 끊김")
            buf += chunk
        return buf.decode("ascii", "ignore")

    def cmd(self, line):
        self.sock.sendall(line.encode() + b"\n")
        return self._read_prompt()

    def write32(self, addr, value):
        self.cmd(f"mww 0x{addr:08X} 0x{value:08X}")

    def read32(self, addr):
        out = self.cmd(f"mdw 0x{addr:08X}")
        m = re.search(rf"0x{addr:08x}:?\s+([0-9a-fA-F]+)", out, re.IGNORECASE)
        if not m:
            raise RuntimeError(f"레지스터 읽기 실패: {out!r}")
        return int(m.group(1), 16)

    def close(self):
        try:
            if self.sock:
                self.sock.sendall(b"exit\n")
                self.sock.close()
        except OSError:
            pass
        self.proc.terminate()
        try:
            self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()


def getch():
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        return sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def status_line(idx, angle, cal):
    name, pin = FINGERS[idx][0], FINGERS[idx][1]
    o = f"{cal[idx]['open']:.0f}°" if cal[idx]["open"] is not None else "미저장"
    g = f"{cal[idx]['grip']:.0f}°" if cal[idx]["grip"] is not None else "미저장"
    return f"[{idx + 1}/{len(FINGERS)} {name}/{pin}] 현재 {angle:6.1f}°   OPEN={o}  GRIP={g}"


def patch_main_c(cal):
    src = MAIN_C.read_text()
    for i, (name, pin, htim, ch, _addr) in enumerate(FINGERS):
        o, g = cal[i]["open"], cal[i]["grip"]
        if o is None or g is None:
            continue
        new_line = (f"  {{ {htim}, {ch}, {o:.1f}f, {g:.1f}f }},"
                    f"   /* {name} / {pin} */")
        pattern = re.compile(rf"^\s*\{{\s*{re.escape(htim)},\s*{ch},[^}}]*\}},\s*/\*\s*{name}.*$",
                             re.MULTILINE)
        if not pattern.search(src):
            print(f"⚠ {name}: main.c에서 해당 줄을 못 찾음 — 수동 반영 필요")
            continue
        src = pattern.sub(new_line, src)
    MAIN_C.write_text(src)
    print(f"✔ {MAIN_C} 의 fingerTable 갱신 완료")


def main():
    if "--help" in sys.argv:
        sys.exit(__doc__)

    print("openocd 연결 중...")
    ocd = OpenOCD()
    print("연결됨. (캘리브레이션 중 파란 버튼 누르지 말 것!)\n")

    # 현재 CCR 값 읽어서 시작 각도로 사용 (0 = 펄스 없음 → 기본 180°)
    angles = []
    for *_rest, addr in FINGERS:
        ccr = ocd.read32(addr)
        angles.append(ccr_to_angle(ccr) if CCR_MIN <= ccr <= CCR_MAX else 180.0)
    cal = [{"open": None, "grip": None} for _ in FINGERS]

    if "--test" in sys.argv:   # 연결 파이프라인 자체 점검용 (서보 안 움직임)
        addr = FINGERS[0][4]
        before = ocd.read32(addr)
        ocd.write32(addr, before)
        after = ocd.read32(addr)
        ocd.close()
        print(f"SELFTEST ccr_before={before} ccr_after={after} angle={ccr_to_angle(before):.1f}")
        return

    idx = 0
    ocd.write32(FINGERS[idx][4], angle_to_ccr(angles[idx]))
    print(__doc__.split("사용법", 1)[1])

    try:
        while True:
            print("\r\033[K" + status_line(idx, angles[idx], cal), end="", flush=True)
            k = getch()
            if k in ("w", "W", "s", "S"):
                step = STEP_BIG if k in ("w", "s") else STEP_SMALL
                if k in ("s", "S"):
                    step = -step
                angles[idx] = max(0.0, min(180.0, angles[idx] + step))
                ocd.write32(FINGERS[idx][4], angle_to_ccr(angles[idx]))
            elif k == "o":
                cal[idx]["open"] = angles[idx]
                g = cal[idx]["grip"]
                if g is not None and abs(g - angles[idx]) < 5.0:
                    print("\n⚠ OPEN과 GRIP이 거의 같음 — 쥔 모양으로 움직인 뒤 g를 다시 누르세요")
            elif k == "g":
                cal[idx]["grip"] = angles[idx]
                o = cal[idx]["open"]
                if o is not None and abs(o - angles[idx]) < 5.0:
                    print("\n⚠ OPEN과 GRIP이 거의 같음 — 두 자세는 서로 달라야 손이 움직입니다")
            elif k in "1234567":
                idx = int(k) - 1
            elif k == "n":
                idx = (idx + 1) % len(FINGERS)
            elif k == "p":
                idx = (idx - 1) % len(FINGERS)
            elif k == "h":
                for i, f in enumerate(FINGERS):
                    a = cal[i]["open"] if cal[i]["open"] is not None else angles[i]
                    ocd.write32(f[4], angle_to_ccr(a))
                print("\n→ 전체 OPEN 정렬 (저장 안 된 손가락은 현재 각도 유지)")
            elif k == "q" or k == "\x03":
                break
    finally:
        print()
        ocd.close()

    print("\n=== 캘리브레이션 결과 ===")
    done = 0
    for i, (name, *_r) in enumerate(FINGERS):
        o, g = cal[i]["open"], cal[i]["grip"]
        if o is None or g is None:
            mark = "✘ 미완성"
        elif abs(o - g) < 5.0:
            mark = "⚠ OPEN=GRIP (버튼 눌러도 안 움직임!)"
        else:
            mark = "✔"
        print(f"  {name}: OPEN={o if o is not None else '-'}  GRIP={g if g is not None else '-'}  {mark}")
        if o is not None and g is not None:
            done += 1

    if done == 0:
        print("저장된 손가락 없음 — 아무것도 반영 안 함")
        return

    if input("\nmain.c fingerTable에 반영할까요? [y/N] ").strip().lower() == "y":
        patch_main_c(cal)
        if input("바로 빌드+플래시할까요? [y/N] ").strip().lower() == "y":
            subprocess.run([str(PIO), "run", "-t", "upload"], cwd=ROOT, check=False)
            print("완료. 보드가 리셋되며 새 값으로 홈잉합니다.")


if __name__ == "__main__":
    main()
