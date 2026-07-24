# Escargot JS Engine — NuttX / Cortex-M55 포팅 보고서

> 작성일: 2026-07-24
> 저장소: `~/nuttx-escargot/nuttx` (apache/nuttx, master 로컬 수정), `~/nuttx-escargot/apps` (apache/nuttx-apps, master + 신규 `interpreters/escargot`)
> 시뮬레이터: QEMU `mps3-an547` (Cortex-M55 이뮬레이션)
> 비교 대상: [`docs/FreeRTOS-Porting.md`](FreeRTOS-Porting.md) (FreeRTOS / 동일 QEMU 타겟)

> **Update:** NuttX 앱(`apps/interpreters/escargot/`)을 이 저장소로 편입하는 작업이
> 진행 중이며, 완료되면 `samples/rtos/nuttx/`에 CI로 검증되는 형태로 자리잡을
> 예정이다 (FreeRTOS가 [`samples/rtos/freertos/`](../samples/rtos/freertos) +
> [`.github/workflows/rtos-freertos.yml`](../.github/workflows/rtos-freertos.yml)로
> 편입된 것과 동일한 패턴). 이 문서는 그와 무관하게 상세 회고/포팅 보고서로
> 그대로 유지된다 — 새 RTOS를 포팅할 때 참고할 정제된 체크리스트는
> [`docs/porting/RTOS_PORTING_GUIDE.md`](porting/RTOS_PORTING_GUIDE.md)를 볼 것.

---

## 목차

1. [목표 및 결과](#1-목표-및-결과)
2. [타겟 사양](#2-타겟-사양)
3. [빌드 환경](#3-빌드-환경)
4. [에뮬레이터 설정 (QEMU mps3-an547)](#4-에뮬레이터-설정-qemu-mps3-an547)
5. [메모리 맵](#5-메모리-맵)
6. [바이너리 크기](#6-바이너리-크기)
7. [발견 및 수정한 버그](#7-발견-및-수정한-버그)
8. [C++ 예외 / JS 예외 동작 검증](#8-c-예외--js-예외-동작-검증)
9. [GC 동작 분석](#9-gc-동작-분석)
10. [SunSpider 1.0.2 실행 결과](#10-sunspider-102-실행-결과)
11. [파일 구조 및 변경 목록](#11-파일-구조-및-변경-목록)
12. [실제 보드 이식 가이드](#12-실제-보드-이식-가이드)
13. [다른 RTOS 이식 가이드](#13-다른-rtos-이식-가이드)
14. [향후 과제](#14-향후-과제)

---

## 1. 목표 및 결과

| 목표 | 결과 |
|---|---|
| Escargot를 NuttX 위에서 부팅 | ✅ |
| `1 + 2` → `3` 확인 | ✅ |
| C++ throw/catch 동작 | ✅ |
| JS try/catch/finally/Error 동작 (15케이스) | ✅ 15/15 |
| SunSpider 1.0.2 전체 통과 | ✅ 26/26 |
| GC 자동 동작 확인 | ✅ (26개 테스트 전체 구간에서 힙 1→8 MB 확장 관측) |

FreeRTOS 포팅과 동일한 QEMU `mps3-an547`/Cortex-M55 타겟에서,
**RTOS를 FreeRTOS 대신 NuttX로 교체**한 것이 이번 포팅의 핵심 차이점이다.
NuttX는 실제 스케줄러, `malloc`/`sbrk`, `clock_gettime()`을 제공하는 상대적으로
"무거운" RTOS이기 때문에, FreeRTOS bare-metal 포팅에서 직접 구현해야 했던
여러 항목(§13 참고)을 그대로 재사용할 수 있었다 — 그 대신, 이번 포팅에서
실제로 문제가 된 버그는 전혀 다른 종류(§7)였다: NuttX 보드 레벨의 힙 할당
코드가 우리가 커스터마이징한 링커 스크립트와 충돌한 것.

---

## 2. 타겟 사양

### 시뮬레이터 (현재)

| 항목 | 값 |
|---|---|
| 모델 | QEMU `mps3-an547` |
| CPU | Cortex-M55 (ARMv8.1-M, MVE/Helium) |
| flash (벡터 테이블 전용) | 512 KB @ 0x00000000 |
| ddr (코드 + GC 힙, 커스텀 확장) | 32 MB @ 0x60000000 (실제 백킹은 2GB 전체, §5 참고) |
| sram1 (.data/.bss/스택) | 2 MB @ 0x01000000 |
| sram2 (미사용) | 2 MB @ 0x21000000 |
| 사용 가능한 주변장치 | UART(NSH 콘솔)만 사용, 그 외 미사용 |

### 공통 소프트웨어 스택

| 구성요소 | 내용 |
|---|---|
| **RTOS** | **NuttX** (FreeRTOS 포팅과의 핵심 차이점) — `mps3-an547:nsh` 보드 설정, `CONFIG_BUILD_FLAT=y` |
| GC | BDWGC, NOSYS/bare-metal 모드 (FreeRTOS 포팅과 동일 설정 다수 재사용) |
| ICU | 비활성 |
| WASM | 비활성 |
| 스레딩 | 비활성 (`escargot` NSH 커맨드가 단일 태스크로 실행) |
| libstdc++ | full (`CONFIG_LIBCXXTOOLCHAIN` + `CONFIG_LIBSUPCXX_TOOLCHAIN`, nano 아님) |

---

## 3. 빌드 환경

### 툴체인

```
arm-none-eabi-gcc 13.2.1
arm-none-eabi-g++ 13.2.1
arm-none-eabi-ar, arm-none-eabi-strip
```

### 아키텍처 플래그 (NuttX 보드 설정에서 그대로 추출, FreeRTOS 포팅과 대부분 동일)

```
-mthumb -mtune=cortex-m55 -march=armv8.1-m.main+fp.dp
-mfpu=fpv5-d16 -mfloat-abi=hard
```

> `+mve.fp`(MVE/Helium)는 의도적으로 뺐다 — 초기 크래시 조사 중 QEMU의 MVE
> 에뮬레이션이 원인인지 시험하기 위해 뺀 실험이 그대로 남은 것이며(§7 근본
> 원인과는 무관했음이 이번 세션에서 확인됨), 다시 넣어도 결과는 동일할 것으로
> 예상되나 재검증하지는 않았다.

### 빌드 구조: 두 단계

FreeRTOS 포팅과 달리 NuttX 포팅은 **별도의 외부 CMake 프로젝트**로
`libescargot.a`/`libgc.a`를 먼저 빌드한 뒤, NuttX 앱 빌드 시스템이 그
결과물을 링크하는 2단계 구조를 쓴다 (NuttX 앱은 보통 NuttX 자체 빌드
시스템으로 컴파일하지만, Escargot 코드베이스 전체를 NuttX Make 룰로
옮기는 대신 기존 CMake 인프라를 그대로 재사용하는 쪽을 택함).

```bash
# 1단계: libescargot.a / libgc.a (arm-none-eabi 크로스 빌드)
cd ~/escargot-nuttx-build/build
make -j$(nproc)
# → libescargot.a, libgc.a 생성

# 2단계: NuttX 앱 빌드 (위 .a 파일을 apps/staging/ 로 복사 후 최종 링크)
cd ~/nuttx-escargot/nuttx
make -j$(nproc)
```

### 주요 컴파일 옵션 (`~/escargot-nuttx-build/CMakeLists.txt`)

```cmake
# Escargot 정의
-DOS_BAREMETAL=1
-DESCARGOT_SMALL_CONFIG
-DNDEBUG
-DGC_NEW_ABORTS_ON_OOM
-DENABLE_COMPRESSIBLE_STRING
-DENABLE_RELOADABLE_STRING
-DESCARGOT_32=1                      # NaN-boxing 32비트 모드
-DSTACK_FREESPACE_FROM_LIMIT=4096
-DSTACK_USAGE_LIMIT=131072

# BDWGC 정의
-DNOSYS -DSMALL_CONFIG -DGC_NOT_DLL -DGC_NO_SIGHANDLER
-DNO_GETENV -DNO_CLOCK -DGC_DISABLE_INCREMENTAL
-DIGNORE_DYNAMIC_LOADING -DDONT_USE_MMAP -DGC_NO_DLOPEN
"-DGC_INITIAL_HEAP_SIZE=(1*1024*1024)"
-DGC_ATOMIC_UNCOLLECTABLE
-DENABLE_DISCLAIM
-D_setjmp=setjmp -D_longjmp=longjmp
```

> `gcconfig.h`는 이 포트에서 수정이 필요 없다 — 스택 하한/GC 훅은 이제
> `PlatformRef::stackBase()`/`stackTop()`/`tickMs()`를 통해 제공된다.
> 자세한 내용은 [`docs/porting/RTOS_PORTING_GUIDE.md`](porting/RTOS_PORTING_GUIDE.md) 참고.

### NuttX 커널 `.config`의 주요 옵션 (`kconfig-tweak`으로 설정)

```
CONFIG_LIBCXXTOOLCHAIN=y         # 기본 CONFIG_LIBCXXNONE엔 예외/RTTI 없음
CONFIG_CXX_EXCEPTION=y
CONFIG_CXX_RTTI=y
CONFIG_LIBC_LOCALTIME=y          # tzset()/tzname 선언 활성화 (VMInstance.cpp 요구)
CONFIG_LIBC_LOCALE=y
CONFIG_ARCH_SETJMP_H=y
CONFIG_ARCH_INTERRUPTSTACK=8192  # 폴트 핸들러 자체 스택 (초기엔 2048)
CONFIG_INTERPRETERS_ESCARGOT=y
CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE=4194304   # 4 MB (기본값 262144=256KB는 부족, §7 참고)
CONFIG_INIT_ENTRYPOINT="nsh_main"                # 최종 상태 (디버깅 중엔 escargot_main으로 임시 전환, §7.2)
```

### 앱 빌드: NuttX 자체 Kconfig/Make.defs 필요 (§12에서 상술)

```bash
cd ~/nuttx-escargot/nuttx
kconfig-tweak --enable CONFIG_INTERPRETERS_ESCARGOT
kconfig-tweak --set-val CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE 4194304
make olddefconfig
make -j$(nproc)
```

---

## 4. 에뮬레이터 설정 (QEMU mps3-an547)

### 실행 명령 (일반 실행 — 대화형 NSH)

```bash
qemu-system-arm -M mps3-an547 -cpu cortex-m55 -kernel nuttx -nographic
# → nsh> 프롬프트에서 `escargot` 입력
```

FreeRTOS 포팅과 달리 semihosting은 전혀 쓰지 않는다 — NuttX가 UART 콘솔을
표준 입출력으로 그대로 제공하므로 `-nographic` 하나로 NSH 셸까지 전부 뜬다.

### 비대화형(스크립트) 실행 — stdin 파이프

```bash
(sleep 3; echo "escargot"; sleep 60) | \
  timeout 90 qemu-system-arm -M mps3-an547 -cpu cortex-m55 -kernel nuttx -nographic
```

`nsh>` 프롬프트가 뜰 때까지 대기한 뒤 `escargot` 커맨드를 stdin으로 흘려
넣는 방식.

### GDB 배치 모드 — 근본 원인 진단에 사용 (§7.1)

```bash
# 1) QEMU를 리셋 시점에 정지한 채로 기동
qemu-system-arm -M mps3-an547 -cpu cortex-m55 -kernel nuttx -nographic -s -S &

# 2) gdb를 배치 모드로 붙여서 첫 폴트 지점의 레지스터/백트레이스를 자동 덤프
gdb-multiarch -batch -x commands.gdb
```

`commands.gdb`는 `exception_common`(NuttX의 범용 예외 진입 트램폴린)에
브레이크포인트를 걸고, 히트할 때마다 `SCB->CFSR/HFSR/BFAR/MMFAR`
(`0xE000ED28/2C/38/34`)과 `info registers`, `bt`를 자동 덤프한 뒤
`continue`하는 `commands` 블록을 사용 (8회 히트 후 자동 종료).

`CONFIG_INIT_ENTRYPOINT`를 NSH의 `nsh_main` 대신 앱의 `escargot_main`
심볼로 임시 전환하면, NSH 대화형 입력 없이 부팅 즉시 앱 코드가 실행되므로
gdb 세션 조합이 훨씬 단순해진다 (§7.2에서 상술).

### QEMU 모니터를 통한 실제 메모리 백킹 확인

```bash
# QEMU 모니터 포트(4444)로 접속해 "info mtree" 실행
qemu-system-arm ... -monitor tcp::4444,server,nowait -serial null &
echo "info mtree" | nc 127.0.0.1 4444
```

이번 포팅의 핵심 근본 원인(§7.1)을 확정하는 데 사용 — `0x60000000` 부터
2GB(`0x60000000`-`0xdfffffff`) 전체가 QEMU 모델 상 실제 RAM(`DDR`)으로
백킹되어 있음을 확인했다.

### QEMU 한계 및 주의사항

- **타이밍 부정확**: FreeRTOS 포팅과 동일하게, QEMU 명령어 실행 속도는 실제
  하드웨어와 다르므로 SunSpider 실행 시간(§10, 약 40-50초/26개)은 성능
  지표로 사용할 수 없다.
- **종료 방법**: `escargot` 커맨드 완료 후 `nsh>` 프롬프트로 정상 복귀하지만
  NSH 자체는 계속 대기하므로 QEMU가 자동 종료되지 않는다 — 스크립트에서는
  `timeout`으로 종료.

---

## 5. 메모리 맵

### 링커 스크립트 (`boards/arm/mps/mps3-an547/scripts/flash.ld`, 커스텀 수정)

```
Region  Origin      Length   용도
─────────────────────────────────────────────────────────
flash   0x00000000  512 KB   벡터 테이블만 (.vectors) — VTOR 리셋 고정 주소
ddr     0x60000000   32 MB   .text/.rodata/.ARM.exidx/.ARM.extab
                              + .gc_heap (NOLOAD, 16 MB, BDWGC 힙 전용)
sram1   0x01000000    2 MB   .data/.bss/태스크 스택 (kumm_malloc 기반 힙 원본)
sram2   0x21000000    2 MB   미사용
```

스톡 `mps3-an547` 보드의 512 KB `flash` 리전은 C++ 예외/RTTI를 켠 전체
JS 엔진이 들어가기에 너무 작다 (링커 스크립트에서 `flash`의 `LENGTH`만
키워 봐도 즉시 부팅 크래시 — QEMU 모델이 그 주소 범위를 512 KB 이상으로는
실제 백킹하지 않음). 그래서 벡터 테이블만 `flash`에 남기고 나머지 전부를
FreeRTOS 포팅과 동일한 `0x60000000` DDR 영역으로 옮기는 2-리전 분할을
도입했다.

### `REGION1`(NuttX 보드 힙 확장 영역) — 이번 포팅에서 수정한 부분

```
arch/arm/src/mps/hardware/mps_memorymap.h (CONFIG_ARCH_CHIP_MPS3_AN547):

수정 전 (스톡):
  REGION1_RAM_START = MPS_EXTMEM_START   = 0x60000000  (=ddr 리전과 동일 시작주소!)
  REGION1_RAM_SIZE  = MPS_EXTMEM_SIZE    = 0x80000000  (2GB 전체)

수정 후:
  REGION1_RAM_START = MPS_EXTMEM_START + 0x02000000    (ddr 32MB 지난 지점부터)
  REGION1_RAM_SIZE  = MPS_EXTMEM_SIZE  - 0x02000000
```

이 충돌(§7.1)이 이번 포팅에서 발견된 가장 핵심적인 버그의 근본 원인이다.

### 실제 빌드 레이아웃 (최종 바이너리, 26-테스트 포함)

```
Address       Section            Size       설명
────────────────────────────────────────────────────────────────────
0x00000000    .vectors           584 B      Cortex-M 벡터 테이블 (flash)

0x60000000    .text              2,550 KB   코드+rodata (26개 SunSpider 소스
                                              문자열 리터럴 포함, ddr)
0x60??????    .init_section      48 B       C++ 전역 생성자 포인터 배열
0x60??????    .ARM.extab         59 KB      ARM EHABI 언와인드 opcode + LSDA
0x60??????    .ARM.exidx         38 KB      ARM EHABI 예외 인덱스 테이블
0x60??????    .gc_heap (NOLOAD)  16 MB      BDWGC 힙 (sbrk 대상, 32MB ddr 중 잔여분)

0x01000000    .data (sram1)      2 KB       초기화된 전역변수
0x01000???    .bss (sram1)       50.7 KB    미초기화 전역변수
                                              (태스크 스택은 여기 포함 안 됨 —
                                               kumm_malloc으로 런타임에 할당)
```

FreeRTOS 포팅과의 핵심 차이: FreeRTOS 쪽은 `js_stack`(512 KB)을 링커가
`.bss`에 정적으로 박아 넣지만, NuttX 쪽은 태스크 스택을 `kumm_malloc()`으로
**런타임에** 힙에서 할당한다 — 그래서 정적 `.bss`는 훨씬 작지만(50.7 KB vs.
FreeRTOS의 609 KB), 그 대신 §7.1의 버그가 바로 "그 런타임 할당이 어디서
오는가"의 문제였다.

---

## 6. 바이너리 크기

### 스톡(no-Escargot) vs. Escargot 활성화 — 델타

동일 `.config`(NuttX 커널 옵션은 모두 동일, `CONFIG_INTERPRETERS_ESCARGOT`
만 on/off)로 각각 클린 빌드하여 비교:

| | 스톡 (no-Escargot) | Escargot 활성화 (26-테스트) | 델타 |
|---|---|---|---|
| `.text`+`.rodata`+예외테이블 (`.gc_heap` 제외) | 326,131 B | 2,649,571 B | **+2,323,440 B (~2.22 MB)** |
| `.data` | 1,124 B | 2,052 B | +928 B |
| `.bss` (`.gc_heap` NOLOAD 16MB 제외) | 21,060 B | 50,704 B | +29,644 B (~29 KB) |
| ELF, 스트립 후 | 339,000 B | 2,662,548 B | +2,323,548 B |

> `.gc_heap`(16 MB, NOLOAD)은 스톡/Escargot 양쪽 빌드에 동일하게 존재한다
> (보드 링커 스크립트 레벨 고정 예약이며 `CONFIG_INTERPRETERS_ESCARGOT`
> on/off와 무관) — 델타 계산에서는 양쪽에서 상쇄되므로 제외했다. 전체 이미지
> "used size"에는 당연히 포함된다(아래 §6.2).

### 절대 사용량 (최종 이미지, `arm-none-eabi-size -A`)

| 리전 | 사용 | 전체 | 사용률 |
|---|---|---|---|
| `flash` | 2,640 B | 512 KB | 0.50% |
| `ddr` | 19,426,808 B | 32 MB | 57.90% |
| `sram1` | 52,772 B | 2 MB | 2.52% |
| `sram2` | 0 B | 2 MB | 0.00% |

### ELF 파일

| 구분 | 크기 |
|---|---|
| 디버그 심볼 포함 (`.elf`) | 12.7 MB |
| 스트립 후 (`arm-none-eabi-strip -s`) | **2.66 MB** |

FreeRTOS 포팅(스트립 후 1.7 MB)과 직접 비교 시, 이번 NuttX 바이너리가 더 큰
이유의 대부분은 **26개 SunSpider 소스를 문자열 리터럴로 통째로 내장**했기
때문이다(§12.5에서 이 방식의 트레이드오프를 논의) — FreeRTOS 쪽은 애초에
semihosting으로 26개를 호스트 파일에서 로드하므로 바이너리에 JS 소스가
내장되지 않는다.

---

## 7. 발견 및 수정한 버그

이 포팅 이전에 이미 확정된 사실:
- `escargot/src/runtime/ValueInlines.h`의 `int32_t=long` 무한 재귀 버그는
  FreeRTOS 포팅에서 이미 발견·수정되어 escargot `master`에 병합됨 — 이
  포팅은 그 수정을 그대로 재사용했을 뿐, 새로 만난 문제가 아니다.

이 포팅에서 실제로 크래시를 완전히 해결한 것은 서로 독립된 **두 개의
버그**였다.

### 7.1 근본 원인 #1 — 보드 힙 코드가 Escargot 코드/GC힙 영역을 OS 힙으로 재기부 ★ 핵심 버그

**증상:** `Lockup: can't escalate 3 to HardFault`, PC는 항상
`exception_common`의 정확히 첫 명령 주소(`0x60000140`). 태스크 스택을
1MB→4MB로 늘려도, 인터럽트 스택을 2KB→8KB로 늘려도 증상이 사라지지 않고
그저 조금 늦게 재현됨.

**진단:** `qemu -s -S` + `gdb-multiarch -batch` 조합으로 `exception_common`에
브레이크포인트를 걸고 최초 진입 시점의 레지스터를 실제로 덤프.

두 번째 히트(첫 히트는 정상적인 SVCall)에서 `HFSR=0x40000000`(FORCED)과
함께 다음 백트레이스를 확보:

```
#4  arm_stack_color (stackbase=0x60000010 <mps_tcmenable+16>, nbytes=4194304)
#5  up_create_stack (tcb=..., stack_size=4194304, ...)
#6  nxtask_init (..., entry=0x60010ed9 <escargot_main(int, char**)>, ...)
#7  nxtask_spawn_create (...)
...
#12 nxtask_start ()
```

즉 **Escargot/BDWGC 코드가 실행되기도 전에**, NuttX가 `escargot_main`
태스크의 스택을 만드는 도중(`CONFIG_STACK_COLORATION`이 새로 할당한 4MB
영역을 채우는 `arm_stack_color()`)에 폴트가 난다. `stackbase` 인자값
`0x60000010`이 결정적 단서 — `mps_tcmenable` 함수(우리 코드!) 16바이트
뒤, 즉 우리 링커 스크립트가 `.text`를 배치한 `ddr` 리전의 시작 주소
(`0x60000000`) 바로 근처다.

**근본 원인:** `arch/arm/src/mps/hardware/mps_memorymap.h`의
`CONFIG_ARCH_CHIP_MPS3_AN547` 블록(손대지 않은 스톡 코드)은
`CONFIG_MM_REGIONS > 1`일 때(`.config`에서 2) 다음과 같이 정의한다:

```c
#define REGION1_RAM_START  MPS_EXTMEM_START   /* 0x60000000 */
#define REGION1_RAM_SIZE   MPS_EXTMEM_SIZE    /* 0x80000000, 2GB */
```

`arch/arm/src/mps/mps_allocateheap.c`의 `arm_addregion()`은 무조건
`kumm_addregion(REGION1_RAM_START, REGION1_RAM_SIZE)`를 호출 — 즉
**`0x60000000`부터 2GB 전체를 OS 힙에 통째로 기부**한다. 스톡 보드
레이아웃(`.text`/`.data`가 온칩 SRAM에 있는 경우)에서는 맞는 가정이지만,
우리 커스텀 `flash.ld`는 Escargot의 `.text`/`.rodata`/`.ARM.exidx`/
`.ARM.extab`와 16MB짜리 BDWGC 힙(`.gc_heap`)을 **바로 그 같은 주소**에
재배치했다.

QEMU 모니터 `info mtree`로 실제 확인: 2GB EXTMEM/DDR 구간
(`0x60000000`-`0xdfffffff`) **전체가 진짜로 RAM으로 백킹**되어 있다
(`tz-mpc-downstream: 0000000000000000-000000007fffffff (ram): DDR`).
즉 "매핑 안 된 메모리" 문제가 아니라 — **살아있는 메모리 소유권 충돌**이다:
OS 힙 할당자(dlmalloc류, 리전 저주소부터 첫 프리청크를 구성)와 우리
링커가 배치한 코드가 동시에 `0x60000000`을 자기 것이라고 생각한다.
1MB짜리 스택 요청은 `sram1`의 실제 ~2MB 힙만으로 충분히 조달됐지만, 4MB
요청은 그 프리청크(=우리 코드 영역과 겹침)까지 파고들었고,
`arm_stack_color()`의 쓰기 루프가 **살아있는 커널 코드**(`exception_common`
자신 포함, 리전 시작에서 겨우 0x130바이트 거리)를 컬러링 패턴으로 덮어썼다.
다음에 발생하는 비동기 예외(주기적 SysTick, 거의 즉시 발생)가 이미
오염된 그 고정 주소로 진입 → 쓰레기 실행 → 즉시 재폴트 → 다시
`exception_common` 진입 시도 → 여전히 오염됨 → 무한 반복.

**수정** (`arch/arm/src/mps/hardware/mps_memorymap.h`, AN547 블록만):

```c
#define REGION1_RAM_START  (MPS_EXTMEM_START + 0x02000000)  /* 우리 32M ddr 지난 지점 */
#define REGION1_RAM_SIZE   (MPS_EXTMEM_SIZE  - 0x02000000)
```

재빌드 후 재현: **락업이 완전히 사라짐.** `Globals::initialize()` →
`GC_init()` → 컨텍스트 생성 → 이후 전체 코드까지 정상 진행, `mallinfo()`도
멀쩡한 값(`arena=2066376 KB`, `~2GB-32MB`와 정확히 일치).

### 7.2 GDB 진단을 위한 인프라 — `CONFIG_INIT_ENTRYPOINT` 우회

`CONFIG_INIT_ENTRYPOINT`/`CONFIG_INIT_ENTRYNAME`(표준 NuttX Kconfig
옵션, `sched/Kconfig`)을 `nsh_main` 대신 앱의 링크 심볼
`escargot_main`(`int(int, char**)`, `apps/Application.mk`가 MODULE 빌드
시 `main`을 `<PROGNAME>_main`으로 `#define`하는 규칙에 따라 결정됨)으로
바꾸면, NSH 상호작용 없이 부팅 즉시 앱 코드가 실행된다 — §7.1의 GDB
세션을 훨씬 단순하게 만든 핵심 트릭. 디버깅이 끝난 뒤 `nsh_main`으로
되돌려 최종 검증(§8, §10)은 실제 NSH 경로로 다시 확인했다.

### 7.3 근본 원인 #2 — 스택 훅이 서로 모순되는 두 용도로 재사용됨

**증상:** §7.1을 고친 직후, 락업은 사라졌지만 **모든** JS 실행이(리터럴
`"1+2"`조차) `[PARSE_ERR] ... msg="Line 1: too many recursion in script"`
로 즉시 실패.

**진단:** 메시지의 출처는 `esprima.cpp`의 `checkRecursiveLimit()`으로,
현재 SP와 `ThreadLocal::stackLimit()`(`g_stackLimit`)를 비교한다.
`ThreadLocal::initialize()`(`OS_BAREMETAL` 경로)는 스택의 **낮은** 쪽
끝을 기준으로 `g_stackLimit`을 계산하는 반면, BDWGC의 `STACKBOTTOM`은
스택의 **높은/cold** 쪽 끝을 원한다 — 하나의 함수로 두 값을 동시에
만족시키려다 어느 한쪽이 항상 깨진다.

**수정:** `escargot_platform.cxx`에서 함수를 두 개로 분리:
- 낮은 끝을 반환하는 함수 — `ThreadLocal.cpp`의 스택 한계 계산용.
- 높은/cold 끝을 반환하는 함수 — BDWGC `STACKBOTTOM`용.

이 구분은 이제 `Escargot::PlatformRef::stackBase()`(낮은 끝)/
`stackTop()`(높은 끝)로 정식화되어 있다 — 자세한 내용은
[`docs/porting/RTOS_PORTING_GUIDE.md`](porting/RTOS_PORTING_GUIDE.md) §2 참고.

### 7.4 부수적으로 발견한 설정 회귀 — `CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE` Kconfig 기본값 함정

§6의 스톡-vs-Escargot 비교 빌드를 위해 `kconfig-tweak --disable
CONFIG_INTERPRETERS_ESCARGOT` → 재빌드 → `--enable`로 되돌리는 과정에서,
`CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE`가 유지되던 4194304(4MB) 값을
잃고 Kconfig의 `default 262144`(256KB)로 조용히 리셋됐다 — NuttX Kconfig는
`if INTERPRETERS_ESCARGOT` 블록 안의 종속 심볼 값을, 그 심볼이 `n`이 되어
"보이지 않는" 동안은 기억해 주지 않기 때문. 그 상태로 NSH를 통해
`escargot`를 실행했더니 `string-unpack-code` 테스트 하나가 `[THROW]`로
실패 — 4MB로 되돌려 재빌드하니 즉시 26/26 통과로 복귀 — **SunSpider
결과에 실제 엔진 한계는 없다**, 이건 순전히 빌드 설정 실수였다.

---

## 8. C++ 예외 / JS 예외 동작 검증

### 8.1 C++ 예외 자체 테스트

```
[cpp-exc] OK
```

```cpp
__attribute__((noinline)) static void cpp_exc_test(void)
{
    bool ok = false;
    try { throw 42; } catch (int v) { ok = (v == 42); }
    printf("[cpp-exc] %s\n", ok ? "OK" : "BROKEN");
}
```

### 8.2 JS 예외 동작 (15케이스) — 최종 NSH 경로로 재검증, 26-테스트 SunSpider 확장 이후에도 동일

| # | 테스트 | 실제 출력 |
|---|---|---|
| 1 | `throw 42` (uncaught, 의도적) | ✅ `[THROW] uncaught-throw-42` |
| 2 | `try { throw 42 } catch(e) { e }` | ✅ `42` |
| 3 | `throw new Error('hello')` → `.message` | ✅ `"hello"` |
| 4 | `null.x` (uncaught TypeError, 의도적) | ✅ `[THROW] uncaught-null-deref` |
| 5 | catch TypeError, `instanceof TypeError` | ✅ `true` |
| 6 | catch, `.name` | ✅ `"TypeError"` |
| 7 | `new Array(-1)` → `.name` | ✅ `"RangeError"` |
| 8 | `finally`만 있고 catch 없음 (throw 전파, 의도적) | ✅ `[THROW] finally-runs` |
| 9 | `catch` + `finally`, 값 누적 | ✅ `8` |
| 10 | re-throw → `.message` | ✅ `"orig"` |
| 11 | 중첩 try/catch | ✅ `"innerouter"` |
| 12 | `typeof e.stack` | ✅ `"string"` |
| 13 | `new Function('}{')` → instanceof SyntaxError | ✅ |
| 14 | forEach 콜백 내 throw 포착 | ✅ `3` |
| 15 | `Promise.reject(55)` → typeof | ✅ `"object"` |

**15 / 15 통과** (3개 항목의 `[THROW]`는 catch 없이 의도적으로 uncaught로
설계된 테스트이므로 올바른 동작이다 — 실패가 아니다).

---

## 9. GC 동작 분석

### 설정

```cmake
"-DGC_INITIAL_HEAP_SIZE=(1*1024*1024)"   # 1 MB 초기값
```

```ld
/* flash.ld: .gc_heap (NOLOAD), 16MB, ddr 리전 안 */
__gc_heap_start__ / __gc_heap_size__     /* sbrk()가 여기서 bump-allocate */
```

FreeRTOS 포팅과의 실행 방식 차이: 이 포팅의 테스트 하네스는 **각 SunSpider
테스트가 끝날 때마다 명시적으로 `GC_gcollect()`를 강제 호출**한다
(FreeRTOS 포팅은 마지막에 한 번만 수동 호출하고 그 외엔 자동/임계값 트리거
수집에 맡김). 그래서 아래 표의 `since_gc`는 "그 직전 강제 수집 이후 그
테스트 하나가 할당한 바이트 수"를 의미하며, FreeRTOS 리포트의 "누적 자동
GC 트리거" 의미와는 다르다.

### 헬로월드(엔진 초기화 직후, `1+2` 실행 시점) 메모리 스냅샷

FreeRTOS 리포트 §8의 "Hello world (`1+2`)" 행과 동일한 지점 — SunSpider를
전혀 돌리기 전, `Globals::initialize()` + 컨텍스트 생성 직후 실측값
(QEMU 콘솔 출력 그대로):

```
[mm] before-init  arena=2066376 KB used=   4109 KB free=2062267 KB
[gc] init         heap=   1024 KB since_gc=    176 KB free=    914 KB
[mm] after-init   arena=2066376 KB used=   4117 KB free=2062259 KB
[ok]  1+2                          => 3
```

| 항목 | 예약(Reserved) | 사용(Hello World) | 사용률 |
|---|---|---|---|
| GC 힙 (`.gc_heap`, ddr) | 1,024 KB (초기값) | 176 KB (`since_gc`) | 17.2% |
| NuttX `mallinfo()` `used`(kumm 힙, §7.1 수정 후 실제 수치) | 2,066,376 KB (`arena`, ≈2GB−32MB) | 4,117 KB | 0.2% |

> `mallinfo()`의 `used=4,117 KB`는 escargot 태스크 자체가 아니라 NuttX
> 커널/NSH/다른 태스크까지 포함한 전체 kumm 힙 사용량이라, "엔진만의
> 헬로월드 풋프린트"로 보려면 위 GC 힙 행(1,024 KB 예약 중 176 KB 사용,
> 17.2%)이 FreeRTOS 리포트의 "Hello world" 행과 가장 직접 비교 가능한
> 수치다 — FreeRTOS 쪽은 4,096 KB 예약 중 177 KB 사용(4.3%)이었다. 절대
> 사용 바이트 수(176 KB vs 177 KB)는 사실상 동일 — 초기 힙 예약 크기
> 자체가(1 MB vs 4 MB) 두 포팅에서 다르게 선택되었을 뿐, "엔진이 부팅
> 직후 실제로 필요로 하는 양"은 두 RTOS에서 거의 같다는 뜻이다.

### 26개 테스트 실행 중 힙 크기 변화

| 구간 | heap | 비고 |
|---|---|---|
| context 생성 직후 | 1,024 KB | 초기값 |
| `3d-raytrace` 완료 후 | **2,049 KB** | 힙 확장 1→2 MB |
| `date-format-tofte` 완료 후 | **4,097 KB** | 힙 확장 2→4 MB |
| `date-format-xparb` 완료 후 | **6,145 KB** | 힙 확장 4→6 MB |
| `regexp-dna` 완료 후 | **8,193 KB** | 힙 확장 6→8 MB |
| `string-unpack-code` 완료 후 | 8,201 KB | 미세 확장 |
| 26개 전체 + 수동 GC 후 | **8,217 KB** | 최종, since_gc=0 |

- 힙 자동 확장: 1 → 2 → 4 → 6 → 8 MB (`sbrk()`가 링커 예약 16MB
  `.gc_heap` 영역에서 순차 공급)
- 최대 힙 ~8.2 MB ≪ 예약 16 MB (여유 충분)
- FreeRTOS 포팅(최대 ~10 MB, PSRAM 64MB 중)과 규모 면에서 같은 자릿수

### 메모리 사용량 종합 요약 — 바이너리 + 스택(피크) + GC(필요량/할당량)

세 가지 축으로만 보면 이 포팅의 실제 메모리 사용량은 다음과 같다
(모두 §6/§9/§12.4의 실측치를 그대로 가져온 것, 중복 계산 없음):

| 항목 | 예약(Reserved) | 실사용(Used) | 사용률 |
|---|---|---|---|
| **바이너리** (`.text`+`.rodata`+예외테이블, `.gc_heap` 제외) | — (Flash/ddr 512KB+32MB) | 2,649,571 B (~2.53 MB) | — |
| **스택** (escargot 태스크, 26개 SunSpider+예외15 실행 후 피크) | 4,194,256 B (4 MB) | 403,596 B (394.1 KB) | **9.6%** |
| **GC 힙** — 헬로월드(엔진 초기화 직후) | 1,048,576 B (1 MB, 초기값) | 180,224 B (176 KB) | 17.2% |
| **GC 힙** — 피크(26개 SunSpider 전체 실행 후) | 16,777,216 B (16 MB, 링커 예약) | 8,414,208 B (8,217 KB) | 50.1% |

> 바이너리는 "예약"이라는 개념이 없다(코드는 정적으로 링크되는 만큼만
> 차지) — Flash 512KB/ddr 32MB라는 전체 리전 대비 사용률은 §6의 절대
> 사용량 표(`ddr` 57.90%, `.gc_heap` 16MB 포함 기준)를 참고할 것. 이
> 표는 그 대신 "엔진이 실제로 필요로 하는 3가지 자원의 크기"를 한눈에
> 보기 위한 것이라 예약 대비 사용률은 스택/GC 힙에만 의미가 있다.

FreeRTOS 포팅과 비교하면: 스택 실사용량(394 KB)은 FreeRTOS의 실측치
(~261 KB)보다 크지만 같은 자릿수이고, GC 힙 헬로월드 사용량(176 KB)은
FreeRTOS(177 KB)와 사실상 동일 — §9 헬로월드 스냅샷 절에서 이미 다룬
바와 같이, 이 세 항목의 절대 필요량은 두 RTOS 사이에서 거의 변하지
않는다(예약 크기를 얼마나 넉넉하게 잡았는지만 다름).

---

## 10. SunSpider 1.0.2 실행 결과

QEMU `mps3-an547`에서 26개 전부 완료 (실제 NSH `escargot` 커맨드 경로,
3회 반복 실행 모두 동일 결과 — 결정적). 총 소요 시간(엔진 내부 측정,
`clock_gettime` 기준) 약 43-50초 — FreeRTOS 리포트와 마찬가지로 QEMU
타이밍은 성능 지표로 무의미하다.

| 테스트 | 결과 | GC since (KB) |
|---|---|---|
| 3d-cube | ✅ | 824 |
| 3d-morph | ✅ | 750 |
| 3d-raytrace | ✅ | 211 |
| access-binary-trees | ✅ | 183 |
| access-fannkuch | ✅ | 13 |
| access-nbody | ✅ | 31 |
| access-nsieve | ✅ | 551 |
| bitops-3bit-bits-in-byte | ✅ | 2 |
| bitops-bits-in-byte | ✅ | 2 |
| bitops-bitwise-and | ✅ | 2 |
| bitops-nsieve-bits | ✅ | 179 |
| controlflow-recursive | ✅ | 2 |
| crypto-aes | ✅ | 1,080 |
| crypto-md5 | ✅ | 138 |
| crypto-sha1 | ✅ | 149 |
| date-format-tofte | ✅ | 2,086 |
| date-format-xparb | ✅ | 3,043 |
| math-cordic | ✅ | 16 |
| math-partial-sums | ✅ | 5 |
| math-spectral-norm | ✅ | 21 |
| regexp-dna | ✅ | 879 |
| string-base64 | ✅ | 1,627 |
| string-fasta | ✅ | 373 |
| string-tagcloud | ✅ | 5,493 |
| string-unpack-code | ✅ | 3,787 |
| string-validate-input | ✅ | 1,932 |

**26 / 26 통과**

> §7.4에서 기록했듯, 이 26개 결과를 얻기 전 한 번 `string-unpack-code`가
> 실패한 적이 있었다 — 그러나 그 원인은 엔진/스택 한계가 아니라 빌드 설정
> 실수(태스크 스택 4MB→256KB로 잘못 리셋)였고, 수정 후 3회 반복 모두
> 26/26으로 결정적으로 재현됨을 확인했다. 정직하게 기록해 두는 이유는,
> 향후 실보드에서 태스크 스택 크기를 줄이려는 시도를 할 때 "256KB는 실제로
> 부족했었다"는 실측 데이터로 참고할 수 있기 때문이다(§12.4).

---

## 11. 파일 구조 및 변경 목록

### 디렉토리 구조

```
nuttx-escargot/
├── PROGRESS.md                 작업 로그 (시간순)
├── nuttx/                       NuttX 커널 (git, 로컬 수정 2개 파일)
│   ├── arch/arm/src/mps/hardware/mps_memorymap.h   REGION1 수정 (§7.1)
│   └── boards/arm/mps/mps3-an547/scripts/flash.ld  ddr 리전 분할 (§5)
├── apps/                        NuttX 앱 (git, 신규 디렉토리 1개)
│   └── interpreters/escargot/
│       ├── Kconfig              CONFIG_INTERPRETERS_ESCARGOT(_STACKSIZE/_PRIORITY)
│       ├── Make.defs             CONFIGURED_APPS 등록 (없으면 조용히 빌드 안 됨)
│       ├── Makefile               MAINSRC/CXXSRCS + 프리빌트 .a 스테이징 복사
│       ├── escargot_main.cxx     NSH 커맨드, 예외 15케이스, SunSpider 26개
│       ├── escargot_platform.cxx PlatformRef 구현, sbrk()
│       └── embedded_tests.h      SunSpider 1.0.2 26개, 자동 생성 (아래 참고)
└── docs/
    └── porting-report.md         상세 회고 (본 문서는 그 사본/정리본)

escargot-nuttx-build/             libescargot.a/libgc.a 외부 CMake 빌드
└── CMakeLists.txt

work/escargot/                    Escargot 엔진 본체 (이 저장소)
```

### `embedded_tests.h` 생성 방법

손으로 26개 파일을 옮겨 적지 않고, `test/vendortest/SunSpider/tests/
sunspider-1.0.2/*.js` 26개를 raw string literal(`R"JSSRC(...)JSSRC"`)로
그대로 감싸는 bash 스크립트로 생성 (구분자가 26개 소스 어디에도 등장하지
않음을 사전에 grep으로 확인).

### `mps_memorymap.h` 변경 내용 (§7.1)

```c
// arch/arm/src/mps/hardware/mps_memorymap.h, CONFIG_ARCH_CHIP_MPS3_AN547 블록
- #define REGION1_RAM_START  MPS_EXTMEM_START
- #define REGION1_RAM_SIZE   MPS_EXTMEM_SIZE
+ #define REGION1_RAM_START  (MPS_EXTMEM_START + 0x02000000)
+ #define REGION1_RAM_SIZE   (MPS_EXTMEM_SIZE  - 0x02000000)
```

---

## 12. 실제 보드 이식 가이드

NuttX가 이미 지원하는 유사 Cortex-M55/M-class 보드, 혹은 신규 보드 포팅
시 체크리스트.

### 12.1 NuttX 앱으로 등록하기

```
apps/interpreters/<name>/Kconfig      # CONFIG_INTERPRETERS_<NAME> 정의
apps/interpreters/Kconfig             # source 라인 추가 (auto-regen 되지만 최초 1회 수동)
apps/interpreters/<name>/Make.defs    # ★ 필수, 없으면 "설정은 됐는데 조용히 안 빌드됨"
apps/interpreters/<name>/Makefile     # MAINSRC/CXXSRCS + 필요 시 프리빌트 .a 스테이징
```

`Make.defs` 누락은 가장 흔한 함정 — Kconfig로 옵션을 켜도 `CONFIGURED_APPS`
목록에 추가되는 코드가 이 파일에 있어서, 없으면 빌드 결과물 크기가
전혀 안 변하는데도 아무 에러도 나지 않는다.

### 12.2 커널 `.config` 필수 옵션

```bash
kconfig-tweak --enable CONFIG_LIBCXXTOOLCHAIN
kconfig-tweak --enable CONFIG_CXX_EXCEPTION
kconfig-tweak --enable CONFIG_CXX_RTTI
kconfig-tweak --enable CONFIG_LIBC_LOCALTIME   # tzset()/tzname 선언용
kconfig-tweak --set-val CONFIG_ARCH_INTERRUPTSTACK 8192
kconfig-tweak --enable CONFIG_INTERPRETERS_ESCARGOT
kconfig-tweak --set-val CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE 4194304
make olddefconfig
```

> ★ 함정 (§7.4): `CONFIG_INTERPRETERS_ESCARGOT`를 disable→enable로 왔다
> 갔다 하면 `CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE`가 Kconfig 기본값
> (256KB)으로 조용히 리셋된다. 재빌드 전에 항상 `grep
> CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE .config`로 재확인할 것.

### 12.3 링커 스크립트: 보드 힙 코드와의 충돌 확인 (§7.1, 이번 포팅의 핵심 교훈)

Escargot(혹은 다른 큰 정적 라이브러리)를 위해 보드 스톡 링커 스크립트의
메모리 배치를 바꿀 때는, **그 보드의 `arch/*/src/<chip>/`
`*_allocateheap.c`가 어느 주소 범위를 "자유 메모리"로 가정하고 OS 힙에
기부하는지 반드시 확인**해야 한다. 이 리전이 새로 배치한 `.text`/`.data`
와 겹치면, 큰 태스크 스택(혹은 다른 큰 힙 할당)이 살아있는 코드를 덮어쓰는
방식으로 크래시가 나며 — 그 증상은 스택 오버플로우/GC 버그처럼 보이지만
실제로는 완전히 다른 문제다. 체크리스트:

1. `up_allocate_heap()`/`arm_addregion()`(또는 해당 아키텍처의 대응 함수)이
   참조하는 매크로/심볼을 찾는다.
2. 그 값들이 새 링커 스크립트가 실제로 코드/데이터를 배치한 주소 범위와
   겹치는지 확인한다.
3. 겹친다면, 힙 기부 시작 주소를 새 링커 스크립트가 사용하는 영역
   "다음"으로 옮긴다(이번 포팅에서 한 것처럼).
4. QEMU라면 모니터의 `info mtree`로 그 영역이 실제로 얼마나 백킹되어
   있는지 먼저 확인 — 마법의 숫자(2GB 등)가 실제 물리 RAM 크기와 다를 수
   있다.

### 12.4 스택 크기 확인 — 실측 워터마크 확보됨

26개 SunSpider 1.0.2 전체 + 15개 예외 테스트를 모두 실행한 뒤,
`escargot_main.cxx` 반환 직전에 `up_check_tcbstack()`(NSH `ps`/procfs가
쓰는 것과 동일한 메커니즘)로 실측:

```
[stack] high-water: 403596 / 4194256 bytes (394.1 KB / 4096.0 KB)
```

**실제 피크 사용량은 4 MB 예약분 중 394.1 KB(9.6%)** — §7.4에서 확인된
"256 KB는 부족함"과 일관된 결과이며(394 KB > 256 KB), 4 MB는 상당히
여유 있게 잡은 값임을 보여준다. 실보드 이식 시 512 KB~1 MB 정도로
줄여도 될 가능성이 높지만, 안전 마진(이 실측치는 26개 SunSpider + 15개
예외 케이스라는 특정 워크로드 기준이라 더 깊은 재귀/더 큰 JS를 돌리면
늘어날 수 있음)을 감안해 실제 축소 전에는 대상 워크로드로 다시 재보고
시작할 것.

### 12.5 JS 소스 파일 로딩 방식 재검토

이번 포팅은 26개 SunSpider 소스를 전부 C 문자열 리터럴로 바이너리에
내장했다(§11) — 코드 크기가 ~2.2MB 늘어나는 대가로 파일시스템/시리얼
로딩 없이 자체 완결적으로 동작한다는 장점이 있다. 실보드에서 여러 개의
큰 JS 번들을 실행해야 한다면, FreeRTOS 리포트와 마찬가지로 Flash에 JS
번들 전용 섹션을 두거나 NuttX의 실제 파일시스템(SPIFFS/LittleFS 등)에
파일로 저장해 두는 방식이 바이너리 크기 면에서 더 유리하다.

### 12.6 클럭 / `sbrk` / `Globals::finalize()`

NuttX는 실제 `clock_gettime(CLOCK_MONOTONIC, ...)`을 제공하므로
FreeRTOS 포팅처럼 틱 카운트로 대체할 필요가 없었다(§13.1). `sbrk()`는
NuttX 플랫 빌드에 기본 제공되지 않으므로(`umm_sbrk.c`가
`CONFIG_BUILD_KERNEL` 전용) 링커 예약 영역 위에 직접 구현했다 — FreeRTOS
포팅의 `_sbrk()`와 동일한 방식. `Globals::finalize()`는 FreeRTOS 포팅과
마찬가지로 호출하지 않는다(호출할 필요가 없는 임베디드 단발성 실행 모델).

---

## 13. 다른 RTOS 이식 가이드

### 13.1 공통 필수 조건 (FreeRTOS 리포트 §13.1과 동일)

| 조건 | 내용 |
|---|---|
| **스택 크기** | JS 태스크 최소 ~1MB 권장(§12.4 참고, 256KB는 실측으로 부족) |
| **힙 공급** | `sbrk(int incr)` 또는 동등 기능 필요 (BDWGC가 호출) |
| **예외 링크** | full libstdc++ (nano 계열은 exidx 테이블 누락 위험, FreeRTOS 리포트 참고) |
| **`GC_ATOMIC_UNCOLLECTABLE`** | BDWGC 빌드 시 define 필요 |
| **`Globals::finalize()` 제거** | `munmap` 없는 환경에서 hang |
| **보드 힙 코드 감사** | ★ NuttX 포팅에서 새로 드러난 항목(§7.1, §12.3) — 코드/GC힙을 표준 배치가 아닌 곳으로 옮겼다면, RTOS의 힙 초기화 코드가 그 영역을 "자유 메모리"로 오인하고 있지 않은지 반드시 확인 |

### 13.2 NuttX가 "공짜로" 제공한 것 vs. FreeRTOS bare-metal에서 직접 구현해야 했던 것

| 항목 | FreeRTOS bare-metal 포팅 | NuttX 포팅 |
|---|---|---|
| 시간 | 실제 RTC 없음 → FreeRTOS 틱 카운트로 `Date.now()` 근사 | `clock_gettime(CLOCK_MONOTONIC,...)` 그대로 사용 |
| 힙/`malloc` | `heap_3.c`(malloc/free 위임) 직접 설정, `_sbrk`도 링커 심볼 기반 직접 구현 | 실제 `malloc`/`kumm_malloc` 존재 — 다만 GC 힙 자체는 두 포팅 다 링커 예약 영역에 직접 `sbrk` 구현(성능/독립성 이유로 동일 선택) |
| 태스크/스케줄러 | `xTaskCreateStatic` 등 FreeRTOS API 직접 사용 | NuttX 표준 `task_spawn`(NSH가 대신 호출) — 앱 코드는 스케줄러 API를 거의 모름 |
| 콘솔 I/O | UART 없어서 QEMU semihosting(`bkpt #0xAB`)으로 host 파일 I/O까지 구현 | 표준 POSIX `printf`/stdin이 NSH 콘솔에 그대로 연결 — 별도 구현 불필요 |
| C++ 예외/RTTI | 커스텀 링커 스크립트에 `KEEP(.ARM.exidx*/.ARM.extab*)` 직접 추가 | 동일하게 필요 (RTOS가 대신 해주지 않음 — 두 포팅 다 직접 처리) |
| **보드 힙 초기화 버그(§7.1)** | 해당 없음(bare-metal은 애초에 "OS가 자체적으로 메모리 영역을 재기부"하는 레이어가 없음) | **NuttX 특유의 위험** — RTOS가 자체적인 보드 힙 초기화 코드를 갖고 있고, 그 코드가 커스텀 링커 배치를 모르면 정확히 이런 충돌이 난다 |

**요약:** NuttX처럼 이미 스케줄러/힙/클럭을 갖춘 "무거운" RTOS로 옮기면
FreeRTOS bare-metal 포팅에서 손으로 구현해야 했던 여러 항목이 사라지는
대신, 그 RTOS 자신의 보드 지원 코드(이번 경우 `mps_allocateheap.c`)가
가진 가정과 우리의 커스텀 메모리 레이아웃이 충돌할 수 있다는 새로운
리스크가 생긴다 — 이번 포팅에서 실제로 겪은 것이 정확히 그 케이스였다.

### 13.3 다른 RTOS로 이식 시 참고 (FreeRTOS 리포트 §13.2-13.6 그대로 유효)

Zephyr/ThreadX/Mbed OS/bare scheduler 케이스는 FreeRTOS 리포트 §13.2-13.6을
그대로 참고 — RTOS별 스택 생성 API 차이는 그대로이며, 이번 NuttX 포팅이
추가로 알려주는 것은 "그 RTOS가 자체 힙 초기화/메모리 맵 가정을 갖고
있다면 §7.1/§12.3 체크리스트를 반드시 거칠 것" 한 가지다.

새 RTOS를 포팅할 때는 이 두 리포트보다 [`docs/porting/RTOS_PORTING_GUIDE.md`](porting/RTOS_PORTING_GUIDE.md)를
먼저 볼 것 — 두 포팅이 수렴한 최종 체크리스트/`PlatformRef` 계약이 정리되어 있다.

---

## 14. 향후 과제

| 항목 | 우선순위 | 설명 |
|---|---|---|
| 실 하드웨어 포팅 | 높음 | §12 가이드 기반, 실제 Cortex-M55 보드 링커 스크립트/보드 힙 코드 감사 |
| 태스크 스택 워터마크 실측 | 중간 | 현재 4MB, `nxsched_get_stackhighwater`류 API로 실사용량 확인 후 축소 |
| JS 소스 파일 로딩 방식 | 중간 | 현재 문자열 리터럴 내장(§12.5) → 실보드에서는 파일시스템/Flash 번들 검토 |
| SunSpider 실행 시간 측정 | 낮음 | QEMU 타이밍 무의미, 실물에서만 의미 있음 |
| GC 파라미터 튜닝 | 낮음 | 초기 힙 1MB, 26개 테스트에서 최대 ~8.2MB까지 확장 확인 — 실사용 패턴 기반 재조정 여지 |
| `mps_allocateheap.c`류 감사 자동화 | 낮음 | §12.3 체크리스트를 다른 보드/칩으로 확장 포팅할 때 반복하지 않도록 스크립트화 검토 |
