# UART_STM32 — STM32F407 로봇 손(휴머노이드 암) 서보 컨트롤러

Black F407VE 보드(STM32F407VET6, Cortex-M4F)용 PlatformIO 펌웨어.
Jetson Nano(또는 USB-UART 터미널)가 USART1로 보내는 명령을 받아
서보 11개를 50 Hz PWM(500~2500 µs)으로 구동한다.

- 프레임워크: STM32CubeF4 HAL + FreeRTOS(CMSIS-RTOS2)
- 구조: UART 수신 인터럽트 → 프레임 검증 → FreeRTOS 큐 → `PWM_Control_Task`가 TIM2/3/4 CCR 갱신

---

## 핀맵 (모터 ↔ 핀 ↔ 타이머 채널)

### 서보 PWM 출력

| 모터 ID | 담당 부위 | 핀 | 타이머 채널 | 서보 |
|:---:|---|:---:|:---:|:---:|
| 0 | 어깨 회전 (SHOULDER_ROT) | **PA0** | TIM2_CH1 | MG996R |
| 1 | 손목 1 (WRIST_1) | **PA1** | TIM2_CH2 | MG996R |
| 2 | 손목 2 (WRIST_2) | **PA2** | TIM2_CH3 | MG996R |
| 3 | 손목 3 (WRIST_3) | **PA3** | TIM2_CH4 | MG996R |
| 4 | 손가락1 A (FINGER1_A) | **PA6** | TIM3_CH1 | MG90S |
| 5 | 손가락1 B (FINGER1_B) | **PA7** | TIM3_CH2 | MG90S |
| 6 | 손가락2 A (FINGER2_A) | **PB0** | TIM3_CH3 | MG90S |
| 7 | 손가락2 B (FINGER2_B) | **PB1** | TIM3_CH4 | MG90S |
| 8 | 손가락3 A (FINGER3_A) | **PB6** | TIM4_CH1 | MG90S |
| 9 | 손가락3 B (FINGER3_B) | **PB7** | TIM4_CH2 | MG90S |
| 10 | 손가락 회전 (FINGER_ROT) | **PB8** | TIM4_CH3 | MG90S |

- 큰 토크가 필요한 어깨/손목(모터 0~3)은 MG996R + TIM2, 손가락(4~10)은 MG90S + TIM3/TIM4.
- 각 손가락은 A/B 서보 한 쌍으로 움직인다 (제스처 명령이 6개를 한꺼번에 구동).

### 통신 (UART)

| 기능 | 핀 | 설정 |
|---|:---:|---|
| USART1 TX (보드 → 외부) | **PA9** | 115200 baud, 8-N-1 |
| USART1 RX (외부 → 보드) | **PA10** | USB-UART 어댑터의 TX를 여기에 연결 |

> ⚠ 배선 시 교차 연결: 어댑터 **RX → PA9**, 어댑터 **TX → PA10**, 그리고 **GND 공통** 필수.
> 서보 전원은 외부 5~6 V 사용 (보드 5 V 핀으로 서보 11개 구동 금지 — 스톨 시 수 A).

---

## UART 명령 프로토콜

모든 명령은 5바이트 프레임:

```
[0xAA 헤더] [모터 ID] [angle_H] [angle_L] [checksum]
```

- 각도값 = 실제 각도 × 10 (예: 90.0° → 900 = `0x0384` → H=`03`, L=`84`)
- checksum = (모터 ID + angle_H + angle_L)의 하위 8비트
- 헤더가 아니거나 checksum이 틀리면 그 프레임은 조용히 버려짐 (0xAA에서 재동기화)

### 개별 서보 제어 (모터 ID 0~10)

| 예시 | 의미 |
|---|---|
| `AA 04 03 84 8B` | 모터 4(FINGER1_A)를 90.0°로 |
| `AA 00 03 84 87` | 모터 0(어깨 회전)을 90.0°로 |

### 제스처 명령 (모터 ID 0xF0) — 손가락 6개 일괄 구동

angle_L 자리에 제스처 코드가 들어간다:

| 보낼 바이트 (HEX) | 제스처 | 동작 |
|---|---|---|
| `AA F0 00 01 F1` | **GRIP (손 쥐기)** | 손가락 서보 6개(모터 4~9) → `FINGER_GRIP_ANGLE`(기본 30°) |
| `AA F0 00 00 F0` | **OPEN (손 펴기)** | 손가락 서보 6개 → `FINGER_OPEN_ANGLE`(기본 180°) |

- 어깨/손목/손가락 회전(모터 0~3, 10)은 제스처의 영향을 받지 않는다.
- 각도 기본값은 `src/main.c` 상단 `FINGER_OPEN_ANGLE` / `FINGER_GRIP_ANGLE`에서 수정
  (실물 캘리브레이션 절차는 `CLAUDE.md` STEP 6 참조).

---

## 빌드 / 플래시

PlatformIO 프로젝트 (Makefile/CMake 없음). `pio`는 `~/.platformio/penv/bin/pio`에 있다.

```bash
pio run                # 빌드 (env: black_f407ve)
pio run -t upload      # ST-Link로 플래시
pio run -t clean       # 클린
pio device monitor     # 시리얼 모니터 (115200)
```

터미널 테스트는 RealTerm/Hercules 등에서 **HEX 송신 모드**로 위 프레임을 그대로 전송.

---

## 프로젝트 구조

```
├── platformio.ini        # 환경 설정 (FPU 플래그, FreeRTOS lib_deps — 수정 주의)
├── extra_script.py       # FPU 플래그를 링크 단계에 재적용 (없으면 링크 실패)
├── CLAUDE.md             # 하드웨어 브링업 작업 지시서 (테스트 순서/캘리브레이션)
├── src/
│   ├── main.c            # 애플리케이션 전부: UART ISR, PWM 태스크, 제스처 로직
│   ├── stm32f4xx_it.c    # 인터럽트 핸들러 (FreeRTOS 통합 수정 포함)
│   └── stm32f4xx_hal_msp.c  # 핀 AF 매핑 (위 핀맵의 실제 GPIO 설정)
├── include/
│   ├── FreeRTOSConfig.h  # RTOS 설정 (일부 값은 CMSIS-RTOS2 하드 제약)
│   └── stm32f4xx_hal_conf.h # HAL 모듈 선택
├── lib/Middlewares/      # FreeRTOS (vendored, heap_4 + GCC ARM_CM4F 포트)
├── Drivers/              # STM32CubeF4 HAL/CMSIS (서드파티 — 편집 금지)
├── docs/FREERTOS_GUIDE.md # 초기 설계 노트 (프로토콜 부분은 구버전 — 이 README가 최신)
└── test/                 # PlatformIO Unity 테스트 스캐폴드 (비어 있음)
```

각 디렉토리의 상세 문서는 해당 위치의 `AGENTS.md` 참조.

### 동작 흐름

```
Jetson Nano / 터미널
      │ UART 115200 (5바이트 프레임)
      ▼
USART1 RX 인터럽트 (1바이트 상태머신: 0xAA 동기화 + checksum 검증)
      │ osMessageQueuePut (ISR에서 직접)
      ▼
FreeRTOS 큐 (16개)
      │
      ▼
PWM_Control_Task ── Angle_to_CCR(0~180° → 500~2500µs) ──► TIM2/3/4 CCR 갱신
```

- 타이머 공통 설정: Prescaler=15, Period=19999 → 16 MHz(HSI)/16 = 1 MHz 틱, 20 ms 주기(50 Hz 서보 표준). **서보 스펙에 맞춘 값이므로 변경 금지.**
- 부팅 직후에는 모든 채널 CCR=0(펄스 없음) — 첫 명령 전까지 서보는 현재 위치 유지.

---

## 현재 상태

- ✅ 빌드 클린 (`pio run` 경고/에러 0)
- ✅ 제스처(GRIP/OPEN) 펌웨어 구현 완료
- ⬜ 실기 하드웨어 검증 진행 중 — 테스트 순서는 `CLAUDE.md` 작업 지시서(STEP 2~7) 참조
- ⬜ 각도 캘리브레이션 (OPEN=180°/GRIP=30°는 임시 초안값)

### 직동 방법
1. 전원 확인 (보드 켜기 전, 중요)
- 서보 전원은 외부 5~6V로 공급 (보드 5V 핀에서 뽑으면 안 됨 — 서보 11개 스톨 시 수 A 흐름)
- 외부 전원 GND ↔ STM32 보드 GND 서로 연결 (공통 GND 없으면 PWM 인식 안 됨)
- 손가락 사이에 물체 없는 상태로 시작

2. 펌웨어 굽기 — ST-Link를 보드에 연결한 뒤 터미널에서:
~/.platformio/penv/bin/pio run -t upload
(ST-Link 연결하고 알려주시면 제가 대신 실행해도 됩니다)

3. UART 연결 — USB-UART 어댑터를:
- 어댑터 RX → PA9 (보드 TX)
- 어댑터 TX → PA10 (보드 RX)
- GND 공통

4. 명령 보내기 — 별도 터미널 프로그램 필요 없음. 프로젝트에 포함된 스크립트 사용:

```bash
./tools/hand.py grip     # 손 쥐기  (AA F0 00 01 F1 전송)
./tools/hand.py open     # 손 펴기  (AA F0 00 00 F0 전송)
./tools/hand.py 4 90     # 개별 서보: ID4를 90.0도로 (AA 04 03 84 8B)
```

포트(/dev/cu.usb*)는 자동 탐지. 여러 개 잡히면 `--port /dev/cu.usbserial-xxxx`로 지정.

스크립트 없이 zsh만으로 보내려면 (포트를 열어둔 채 설정해야 macOS에서 baud가 유지됨):

```bash
ls /dev/cu.*                                  # 포트 이름 확인 (예: /dev/cu.usbserial-0001)
exec 3<>/dev/cu.usbserial-0001                # 포트 열어두기
stty -f /dev/cu.usbserial-0001 115200 cs8 -cstopb -parenb raw
printf '\xAA\xF0\x00\x01\xF1' >&3             # GRIP
printf '\xAA\xF0\x00\x00\xF0' >&3             # OPEN
exec 3>&-                                     # 다 쓰면 포트 닫기
```

보낼 HEX (참고):

┌────────────────┬────────────────┐
│      동작      │    보낼 HEX    │
├────────────────┼────────────────┤
│ 손 쥐기 (GRIP) │ AA F0 00 01 F1 │
├────────────────┼────────────────┤
│ 손 펴기 (OPEN) │ AA F0 00 00 F0 │
└────────────────┴────────────────┘