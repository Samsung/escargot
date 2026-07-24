# Escargot RTOS 포팅 보고

FreeRTOS / NuttX 두 RTOS에 Escargot을 포팅한 결과 요약. 상세 회고 보고서 및
실행 가능한 예제는 각 절 상단 링크 참고. 신규 RTOS 포팅 시 체크리스트는
[`docs/porting/RTOS_PORTING_GUIDE.md`](porting/RTOS_PORTING_GUIDE.md) 참고.

---

## 1. FreeRTOS

> 상세: [`docs/FreeRTOS-Porting.md`](FreeRTOS-Porting.md) ·
> 예제: [`samples/rtos/freertos/`](../samples/rtos/freertos) ·
> CI: `.github/workflows/rtos-freertos.yml`

### 환경

| 항목 | 값 |
|---|---|
| 대상 | QEMU `mps3-an547` 에뮬레이션 (Cortex-M55, ARMv8.1-M) — 대상 실보드: BES2800BP (400MHz, SRAM 8.3MB, PSRAM 64MB, Flash 20MB) |
| RTOS | FreeRTOS (bare-metal, ARM_CM4F 포트를 Cortex-M55에서 사용) |
| 툴체인 | `arm-none-eabi-gcc/g++ 13.2.1` |
| GC | BDWGC, NOSYS/bare-metal 모드 |
| ICU / WASM / 스레딩 | 전부 비활성 |
| JS 소스 로딩 | QEMU semihosting으로 호스트 파일시스템에서 SunSpider 1.0.2 26개 직접 로드 (바이너리 내장 아님 — NuttX와의 차이) |

### 결과

| 항목 | 결과 |
|---|---|
| 부팅 + `1 + 2` → `3` | ✅ |
| C++ / JS 예외 (15케이스) | ✅ 15/15 |
| SunSpider 1.0.2 전체 (이번 세션에 1.0→1.0.2로 재검증) | ✅ 26/26 |

### 메모리 사용량

> 아래 %는 대상 실보드(BES2800BP) 스펙 기준 — QEMU 시뮬레이터 자체의 메모리
> 리전 크기가 아님 (2절 NuttX는 반대로 QEMU 리전 기준이라 % 직접 비교는 주의).

| 항목 | 예약(실보드 기준) | 실사용 | 사용률 |
|---|---|---|---|
| Flash (`.text`+`.rodata`+예외테이블) | 20 MB | **~1.61 MB** | 8.1% |
| 스트립 후 ELF | — | **1.7 MB** | — |
| SRAM (`.data`+`.bss`+스택) | 8.3 MB | **~610 KB** | 7.3% |
| 태스크 스택(`js_stack`) — **실측 피크** | 512 KB | **~261 KB** | 51% |
| GC 힙 — **헬로월드**(엔진 초기화 직후, JS 실행 전) | 64 MB (PSRAM) | **177 KB** | 0.3% |
| GC 힙 — **SunSpider 26개 완주 후 피크** | 64 MB (PSRAM) | **~10.26 MB** | 15.6% |

- GC 힙은 4→6→10 MB로 자동 확장(5회 GC 트리거), 스택은 512 KB 중 절반
  정도 사용 — 둘 다 여유 있음.
- 총합(BES2800BP 92.3 MB 중): 헬로월드 시점 ~2.4 MB(2.6%), SunSpider 피크
  시점 ~12.5 MB(13.3%) — 실보드 기준 97% 이상 여유.

---

## 2. NuttX

> 상세: [`docs/NuttX-Porting.md`](NuttX-Porting.md) ·
> 예제: [`samples/rtos/nuttx/`](../samples/rtos/nuttx) ·
> CI: `.github/workflows/rtos-nuttx.yml`

### 환경

| 항목 | 값 |
|---|---|
| 대상 | QEMU `mps3-an547` 에뮬레이션 (Cortex-M55, ARMv8.1-M) |
| RTOS | NuttX (`mps3-an547:nsh` 보드, `CONFIG_BUILD_FLAT=y`) |
| 툴체인 | `arm-none-eabi-gcc/g++ 13.2.1` |
| GC | BDWGC, NOSYS/bare-metal 모드 |
| ICU / WASM / 스레딩 | 전부 비활성 |
| JS 소스 | SunSpider 1.0.2 26개 전체를 문자열 리터럴로 바이너리 내장 |

### 결과

| 항목 | 결과 |
|---|---|
| 부팅 + `1 + 2` → `3` | ✅ |
| C++ / JS 예외 (15케이스) | ✅ 15/15 |
| SunSpider 1.0.2 전체 | ✅ 26/26 |

### 메모리 사용량

| 항목 | 예약 | 실사용 | 사용률 |
|---|---|---|---|
| 바이너리 (`.text`+`.rodata`+예외테이블, SunSpider 26개 소스 포함) | — | **2.53 MB** | — |
| 스트립 후 ELF | — | **2.66 MB** | — |
| GC 힙 — **헬로월드**(엔진 초기화 직후, JS 실행 전) | 1 MB | **176 KB** | 17.2% |
| GC 힙 — **SunSpider 26개 완주 후 피크** | 16 MB (링커 예약) | **8.2 MB** | 50.1% |
| 태스크 스택 — **26개+예외15 전부 실행 후 피크** | 4 MB | **394.1 KB** | 9.6% |

- GC 힙은 1→8 MB까지 자동 확장(16 MB 예약분 중 절반 사용), 스택은 4 MB 중
  10%도 안 씀 — 둘 다 여유 있음.
- 스택 394 KB는 이전에 실측으로 확인된 "256 KB는 부족함"과 일관됨 — 실보드
  포팅 시 512 KB~1 MB 정도로 줄여볼 여지는 있으나, 이 수치는 이 특정
  워크로드(SunSpider 26개) 기준이라 축소 전 재실측 필요.

---

## 3. 두 포팅 비교

| 항목 | FreeRTOS | NuttX |
|---|---|---|
| GC 힙 — 헬로월드 실사용 | **177 KB** | **176 KB** |
| 태스크 스택 — 실측 피크 | ~261 KB (512 KB 중) | 394.1 KB (4 MB 중) |
| JS 소스 로딩 | semihosting (호스트 파일) | 바이너리 문자열 리터럴 내장 |
| GC 힙 최대 확장 | 4→10 MB (64 MB PSRAM 중) | 1→8 MB (16 MB 링커 예약 중) |

**핵심:** RTOS가 달라도(FreeRTOS bare-metal ↔ NuttX 스케줄러/힙/클럭 보유)
엔진이 부팅 직후 실제로 필요로 하는 메모리량(헬로월드 GC 힙 176~177 KB)은
사실상 동일하다 — 차이는 개발자가 예약 크기를 얼마나 넉넉하게 잡았는지,
그리고 두 RTOS가 애초에 제공하는 것(FreeRTOS는 아무것도 없어서 semihosting
등을 직접 구현, NuttX는 실제 스케줄러/힙/클럭 제공)뿐이다. 자세한 트레이드
오프는 각 상세 보고서의 "다른 RTOS 이식 가이드" 절 참고.
