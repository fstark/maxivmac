# Phase 4 — Device Interface & Machine Object (Detailed Plan)

This is the architectural pivot that enables everything downstream: runtime configuration, multi-model binaries, testing, and the debugger/MCP API. We introduce a `Device` interface, wrap each emulated hardware component in a class, and create a `Machine` object that owns all emulator state.

---

## Context: What We're Starting With

After Phase 3, the source tree is clean and navigable:

```
src/core/machine.cpp    — GLOBGLUE: ATT, MMDV_Access, Wires, ICT scheduler, memory mapping
src/core/main.cpp       — PROGMAIN: main loop, tick dispatch, ICT_DoTask
src/cpu/m68k.cpp        — MINEM68K: CPU emulation, regstruct, 55× #if Use68020, 3× #if EmFPU
src/devices/via.cpp     — VIA1 with static VIA1_Ty VIA1_D
src/devices/via2.cpp    — VIA2 with static VIA2_Ty VIA2_D
src/devices/scc.cpp     — SCC with static SCC_Ty SCC
src/devices/rtc.cpp     — RTC with static RTC_Ty RTC
src/devices/iwm.cpp     — IWM with static IWM_Ty IWM
... (16 device files total)
```

### Key architectural problems to solve

1. **All state is in file-scope statics** — `static VIA1_Ty VIA1_D`, `static SCC_Ty SCC`, `static struct regstruct regs`, etc. Cannot instantiate two machines or test devices in isolation.

2. **Device dispatch is a compile-time switch** — `MMDV_Access()` is a `switch(p->MMDV)` where each case is guarded by `#if EmVIA1`, `#if EmASC`, etc. The ATT entries store a `kMMDV_xxx` enum index.

3. **Inter-device communication is through `Wires[]` + `#define` aliases** — e.g., `VIA1_iB0` expands to `Wires[Wire_VIA1_iB0_RTCdataLine]`. Wire change notifications are compile-time `#define` aliases like `#define VIA1_iB0_ChangeNtfy RTCdataLine_ChangeNtfy`.

4. **173 `CurEmMd` compile-time checks** and **55 `Use68020` checks** make the binary single-model, single-CPU-variant only.

5. **The ICT scheduler is global** — `ICTactive`, `ICTwhen[]`, `NextiCount` are globals in machine.cpp, tightly coupled to the CPU's `CyclesRemaining`.

6. **Memory layout (`SetUp_address()`) is duplicated** per model family behind `#if CurEmMd` blocks.

### What stays the same

- The fundamental instruction decode loop in m68k.cpp.
- The ATT linked-list lookup with move-to-front optimization.
- The device access function signature: `uint32_t XXX_Access(uint32_t Data, bool WriteMem, uint32_t addr)`.
- The Wire-based inter-device signaling model (it's actually a good design — we just make it runtime-configurable instead of compile-time).

---

## Architecture Overview

### Target class hierarchy

```
Machine
├── MachineConfig          (model, CPU type, RAM, screen, device enables)
├── CPU                    (registers, decode tables, instruction execution)
├── MemoryController       (ATT, RAM/ROM/VRAM buffers, address decoding)
├── ICTScheduler           (cycle-based task scheduling)
├── WireBus                (inter-device signal routing, replaces Wires[])
└── devices: vector<unique_ptr<Device>>
    ├── VIA1Device         (VIA1_Ty state + VIA1_Access)
    ├── VIA2Device         (VIA2_Ty state + VIA2_Access)
    ├── SCCDevice          (SCC_Ty state + SCC_Access)
    ├── SCSIDevice
    ├── IWMDevice
    ├── RTCDevice          (wire-driven, no memory-mapped access)
    ├── ASCDevice
    ├── ScreenDevice
    ├── VideoDevice
    ├── SonyDevice
    ├── ADBDevice / KeyboardDevice
    ├── MouseDevice
    └── PMUDevice
```

### Key design decisions

| Decision | Rationale |
|----------|-----------|
| **`Device` is an interface (abstract class)** | Uniform dispatch from ATT. Virtual call overhead is negligible vs. emulation work per access. |
| **`Machine` owns everything via `std::unique_ptr`** | Clear ownership. No dangling pointers. Can instantiate multiple machines for testing. |
| **Wires become a `WireBus` with runtime-registered callbacks** | Replaces `#define VIA1_iB0_ChangeNtfy RTCdataLine_ChangeNtfy` with `wireBus.onChange(Wire::RTCdataLine, [&](uint8_t v){ rtc->onDataLineChange(v); })`. Model-specific wiring becomes a factory function. |
| **ATT stores `Device*` instead of `kMMDV_xxx` enum** | Eliminates the `switch` in `MMDV_Access()`. Each ATT entry points directly to the device that handles that address range. |
| **`CurEmMd` comparisons become `config.model` runtime checks** | `if (config.model == kEmMd_II)` instead of `#if CurEmMd == kEmMd_II`. The compiler won't constant-fold them, but they're in cold paths (setup, address decoding) not hot paths (instruction fetch). The hot path is the ATT lookup, which is already runtime. |
| **`Use68020`/`EmFPU` become runtime bools on CPU** | The 55 `#if Use68020` blocks become `if (use68020)`. These are in the instruction decoder, but branch prediction makes this essentially free since the value never changes during a session. |
| **Incremental migration** — each step wraps existing code in classes without rewriting logic | We don't rewrite the VIA timer algorithm or the SCC state machine. We move the existing `static` state into class members and the existing functions into methods. |

---

## Steps

### Step 4.1 — Define `MachineConfig` struct

Create `src/core/machine_config.h` with a struct that captures all compile-time configuration knobs as runtime values.

**New file: `src/core/machine_config.h`**
```cpp
#pragma once
#include <cstdint>
#include <string>

// Model IDs — same values as the existing #defines in machine.h
enum class MacModel : int {
    Twig43   = 0,
    Twiggy   = 1,
    Mac128K  = 2,
    Mac512Ke = 3,
    Kanji    = 4,
    Plus     = 5,
    SE       = 6,
    SEFDHD   = 7,
    Classic  = 8,
    PB100    = 9,
    II       = 10,
    IIx      = 11,
};

struct MachineConfig {
    MacModel model       = MacModel::II;
    bool     use68020    = true;
    bool     emFPU       = true;
    bool     emMMU       = false;

    uint32_t ramASize    = 0x00400000;  // 4 MB
    uint32_t ramBSize    = 0x00400000;  // 4 MB
    uint32_t vidMemSize  = 0x00080000;  // 512 KB
    uint32_t vidROMSize  = 0x000800;

    // Derived feature flags (set by model factory)
    bool emVIA1         = true;
    bool emVIA2         = true;
    bool emADB          = true;
    bool emClassicKbrd  = false;
    bool emRTC          = true;
    bool emPMU          = false;
    bool emASC          = true;
    bool emClassicSnd   = false;
    bool emVidCard      = true;
    bool includeVidMem  = true;

    int  clockMult      = 2;
    int  autoSlowSubTicks = 16384;
    int  autoSlowTime   = 60;

    int  maxATTListN    = 20;

    // Helper predicates
    bool isIIFamily() const {
        return model == MacModel::II || model == MacModel::IIx;
    }
    bool isCompactMac() const {
        return static_cast<int>(model) <= static_cast<int>(MacModel::Plus);
    }
    uint32_t ramSize() const { return ramASize + ramBSize; }
};

// Factory: populate derived flags from model
MachineConfig MachineConfigForModel(MacModel model);
```

**Changes to existing code:**
- Add the new header. No other files change yet.
- Implement `MachineConfigForModel()` in a new `src/core/machine_config.cpp` — this encodes the model→feature mapping that currently lives scattered across `CNFUDPIC.h` and the `setup_t` tool.

**Validation:**
```bash
cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Build succeeds (new files compiled but not yet used). Boot System 7 — unchanged behavior.

**Commit:** `Add MachineConfig struct with model-to-feature factory`

---

### Step 4.2 — Define `Device` interface

Create the abstract `Device` interface that all hardware devices will implement.

**New file: `src/devices/device.h`**
```cpp
#pragma once
#include <cstdint>
#include <string>

class Machine;  // forward declaration

class Device {
public:
    virtual ~Device() = default;

    // Memory-mapped I/O access (for devices on the bus).
    // Not all devices are memory-mapped (e.g., RTC is wire-driven).
    // Default implementation reports abnormal access.
    virtual uint32_t access(uint32_t data, bool writeMem, uint32_t addr);

    // Lifecycle
    virtual void zap()   {}  // Cold start (power-on)
    virtual void reset() {}  // Warm reset

    // Identity
    virtual const char* name() const = 0;

protected:
    Machine* machine_ = nullptr;  // set by Machine when device is registered
    friend class Machine;
};
```

**Design notes:**
- `access()` has the same signature as the existing `XXX_Access()` functions (minus `ByteSize` — that's handled by the Memory Controller before dispatch, as it is today in `MMDV_Access()`).
- `machine_` pointer gives devices access to the Machine (for Wires, ICT scheduler, other devices). This replaces the current pattern of devices reaching globals.
- Not every device overrides `access()` — the RTC is wire-driven via VIA port B bits, not memory-mapped. ADB, Mouse, Keyboard, and PMU are also driven by wire change notifications or ICT callbacks, not direct memory access.

**Validation:** Compiles, boots unchanged.

**Commit:** `Add Device abstract interface`

---

### Step 4.3 — Define `WireBus` class

Replace the `Wires[]` array and `#define` change notification aliases with a runtime wire bus.

**New file: `src/core/wire_bus.h`**
```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <array>

// Wire IDs — exactly mirror the existing enum in CNFUDPIC.h, but
// as a runtime enum class. Max ~40 wires per model.
static constexpr int kMaxWires = 64;

class WireBus {
public:
    using ChangeCallback = std::function<void()>;

    void init(int numWires);

    uint8_t get(int wireId) const        { return wires_[wireId]; }
    void    set(int wireId, uint8_t val);  // sets value + fires callback if changed

    // Register a callback for when a wire changes value.
    // Multiple callbacks per wire are supported (appended).
    void onChange(int wireId, ChangeCallback cb);

    // Pulse notification (called once, not on value change)
    void onPulse(int wireId, ChangeCallback cb);
    void pulse(int wireId);

    int numWires() const { return numWires_; }

private:
    int numWires_ = 0;
    std::array<uint8_t, kMaxWires> wires_{};
    std::array<std::vector<ChangeCallback>, kMaxWires> changeCallbacks_{};
    std::array<std::vector<ChangeCallback>, kMaxWires> pulseCallbacks_{};
};
```

**Implementation (`src/core/wire_bus.cpp`):**
- `init()`: sets all wires to 1 (matching current `AddrSpac_Init` behavior).
- `set()`: `if (wires_[id] != val) { wires_[id] = val; for (auto& cb : changeCallbacks_[id]) cb(); }`.
- `pulse()`: fires all pulse callbacks without changing the wire value.

**Important — backward compatibility layer:**
In this step, the `WireBus` class exists but is NOT yet wired up. The existing `Wires[]` global and `#define` macros remain functional. We'll migrate devices to use `WireBus` in steps 4.6–4.9.

**Validation:** Compiles, boots unchanged.

**Commit:** `Add WireBus class for runtime inter-device signal routing`

---

### Step 4.4 — Define `ICTScheduler` class

Extract the ICT (Interrupt/Cycle Timer) scheduler from globals into a class.

**New file: `src/core/ict_scheduler.h`**
```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <array>

static constexpr int kMaxICTasks = 16;

using iCountt = uint32_t;

class ICTScheduler {
public:
    using TaskHandler = std::function<void()>;

    void zap();
    void add(int taskId, uint32_t cyclesFromNow);

    iCountt getCurrent() const;
    void    doCurrentTasks();
    int32_t doGetNext() const;

    // Register a handler for a task ID
    void registerTask(int taskId, TaskHandler handler);

    // CPU cycle coupling
    void setCycleAccessors(
        std::function<int32_t()> getCyclesRemaining,
        std::function<void(int32_t)> setCyclesRemaining
    );

    uint32_t active    = 0;
    std::array<iCountt, kMaxICTasks> when{};
    iCountt  nextCount = 0;

private:
    std::array<TaskHandler, kMaxICTasks> handlers_{};
    std::function<int32_t()>      getCyclesRemaining_;
    std::function<void(int32_t)>  setCyclesRemaining_;
};
```

**Design notes:**
- The existing ICT logic is simple (bitmask + array), ~80 lines. We move it into the class wholesale.
- `registerTask()` replaces the `switch` in `ICT_DoTask()` ([main.cpp](src/core/main.cpp) L257) — each device registers its own handler at initialization instead of a monolithic switch.
- CPU cycle coupling via function objects (set during Machine initialization) replaces the direct `GetCyclesRemaining()`/`SetCyclesRemaining()` globals.

**Backward compatibility:** In this step, the class is defined but the existing global `ICTactive`/`ICTwhen`/`NextiCount` remain. Migration happens in Step 4.8.

**Validation:** Compiles, boots unchanged.

**Commit:** `Add ICTScheduler class for cycle-based task scheduling`

---

### Step 4.5 — Create `Machine` class shell

Create the `Machine` class that will own all emulator state. Start with just a shell that holds the config and the new classes from Steps 4.1–4.4.

**New file: `src/core/machine_obj.h`** (separate from existing `machine.h` to avoid conflicts during migration)
```cpp
#pragma once
#include <memory>
#include <vector>
#include "core/machine_config.h"
#include "core/wire_bus.h"
#include "core/ict_scheduler.h"

class Device;

class Machine {
public:
    explicit Machine(MachineConfig config);
    ~Machine();

    // Lifecycle
    bool init();        // allocate buffers, create devices, build ATT
    void reset();       // warm reset all devices
    void zap();         // cold start (power-on reset)

    // Accessors
    const MachineConfig& config() const { return config_; }
    WireBus&       wireBus()       { return wireBus_; }
    ICTScheduler&  ict()           { return ict_; }

    // Memory buffers
    uint8_t* ram()    const { return ram_.get(); }
    uint8_t* rom()    const { return rom_; }    // owned by platform layer for now
    uint8_t* vidMem() const { return vidMem_.get(); }
    uint8_t* vidROM() const { return vidROM_.get(); }

    uint32_t ramSize() const { return config_.ramSize(); }

    // Device registry
    void addDevice(std::unique_ptr<Device> dev);

    template<typename T>
    T* findDevice() const;  // find device by type (for cross-device access)

    // Interrupt priority logic
    void recalcIPL();
    uint8_t curIPL() const { return curIPL_; }

private:
    MachineConfig config_;
    WireBus       wireBus_;
    ICTScheduler  ict_;

    // Memory buffers
    std::unique_ptr<uint8_t[]> ram_;
    uint8_t*                   rom_ = nullptr;  // set externally
    std::unique_ptr<uint8_t[]> vidMem_;
    std::unique_ptr<uint8_t[]> vidROM_;

    // Devices
    std::vector<std::unique_ptr<Device>> devices_;

    // Interrupt state
    uint8_t curIPL_ = 0;
    bool    interruptButton_ = false;
};
```

**Implementation (`src/core/machine_obj.cpp`):**
- Constructor: stores config, init is deferred to `init()`.
- `init()`: allocates RAM (config.ramSize() + RAMSafetyMarginFudge), vidMem, vidROM based on config. Initializes WireBus with the wire count for the model. Does NOT yet create devices or build ATT (that comes in steps 4.6+).
- `addDevice()`: pushes to `devices_`, sets `dev->machine_ = this`.
- `findDevice<T>()`: `dynamic_cast` scan over `devices_`. Used rarely (init-time wiring, not hot path).
- `recalcIPL()`: the existing `VIAorSCCinterruptChngNtfy()` logic from machine.cpp L1648, but reading from `wireBus_` instead of the global `Wires[]`.

**Backward compatibility:** The Machine object exists but nothing uses it yet. The old globals continue to drive the emulation.

**Validation:** Compiles, boots unchanged.

**Commit:** `Add Machine class shell (config, WireBus, ICTScheduler, memory buffers)`

---

### Step 4.6 — Wrap VIA1 as a Device class

This is the first real device migration. Convert `via.cpp`'s static state and free functions into a `VIA1Device` class implementing `Device`.

**Strategy: wrapper approach**

Rather than rewriting the VIA1 logic, we:
1. Move `static VIA1_Ty VIA1_D` from file scope into `VIA1Device` as a member.
2. Convert all `static` functions in via.cpp into `VIA1Device` methods.
3. Keep the original function signatures — just add `this->` where they accessed `VIA1_D`.
4. Keep the old global `VIA1_Access()` as a thin forwarding function (for backward compatibility with `MMDV_Access()`) that calls through a global `VIA1Device*` pointer.

**Changes to `src/devices/via.h`:**
```cpp
#pragma once
#include "devices/device.h"

// Forward compat: global singleton pointer (removed in Step 4.10)
class VIA1Device;
extern VIA1Device* g_via1;

class VIA1Device : public Device {
public:
    // Device interface
    uint32_t access(uint32_t data, bool writeMem, uint32_t addr) override;
    void zap() override;
    void reset() override;
    const char* name() const override { return "VIA1"; }

    // Init
    bool init();

    // Timer ICT callbacks
    void doTimer1Check();
    void doTimer2Check();

    // Port I/O — called by wire system
    // (signatures match the existing Get/Put pattern)
    uint8_t getORA(uint8_t viaPortBitsInput);
    void    putORA(uint8_t viaPortBitsOutput);
    uint8_t getORB(uint8_t viaPortBitsInput);
    void    putORB(uint8_t viaPortBitsOutput);

    // Shift register / CB2
    void shiftInData(uint8_t v);
    uint8_t shiftOutData();

private:
    struct VIA1_Ty { /* ... existing fields ... */ };
    VIA1_Ty d_{};

    // Internal methods (moved from static functions)
    void checkInterruptFlag();
    // ... other internal methods
};
```

**Changes to `src/devices/via.cpp`:**
- Move all `static VIA1_xxx` functions to `VIA1Device::xxx` methods.
- Replace `VIA1_D.field` with `d_.field` throughout.
- Replace reads of `Wires[Wire_VIA1_iA0_xxx]` with `machine_->wireBus().get(Wire_VIA1_iA0_xxx)` and writes with `machine_->wireBus().set(...)`.
- Replace `ICT_add(kICT_VIA1_Timer1Check, n)` with `machine_->ict().add(kICT_VIA1_Timer1Check, n)`.
- Replace `VIA1_InterruptRequest` wire writes with `machine_->wireBus().set(Wire_VIA1_InterruptRequest, v)`.
- Add a global `VIA1Device* g_via1 = nullptr;` set during Machine init.
- Add forwarding functions for backward compat:
  ```cpp
  uint32_t VIA1_Access(uint32_t Data, bool WriteMem, uint32_t addr) {
      return g_via1->access(Data, WriteMem, addr);
  }
  ```

**Wire registration (in Machine init):**
```cpp
wireBus_.onChange(Wire_VIA1_iA4_MemOverlay, [this]{ memOverlayChanged(); });
// etc.
```

**Caution — VIA port I/O and wire notifications:**
The VIA's `Put_ORA()`/`Put_ORB()` methods call a `ChangeNtfy` function for each bit that changed. Currently these are `#define` aliases (e.g., `#define VIA1_iB0_ChangeNtfy RTCdataLine_ChangeNtfy`). In the new design:
- Instead of calling `RTCdataLine_ChangeNtfy()` directly, the VIA writes to `wireBus_.set(Wire_VIA1_iB0_RTCdataLine, bitValue)`.
- The RTC device registers a callback on that wire during its init.
- This decouples VIA from RTC at the source level while preserving identical behavior.

**Validation:**
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Boot System 7. Test: keyboard works (VIA1 handles keyboard via shift register on Mac II), mouse works, disk access works (VIA1 port A controls IWM select), timing is correct (VIA1 timers drive main tick).

**Commit:** `Wrap VIA1 in VIA1Device class implementing Device interface`

---

### Step 4.7 — Wrap VIA2 and SCC as Device classes

Same pattern as Step 4.6 for VIA2 and SCC.

**VIA2Device (`src/devices/via2.h`, `src/devices/via2.cpp`):**
- Move `static VIA2_Ty VIA2_D` → `VIA2Device::d_`.
- VIA2 is only present on Mac II/IIx (`#if EmVIA2`). The Machine creates it only if `config.emVIA2`.
- VIA2 handles NuBus interrupts, power-off, and 32-bit addressing mode on Mac II.
- Wire change: `VIA2_iB2_ChangeNtfy → PowerOff_ChangeNtfy`, `VIA2_iB3_ChangeNtfy → Addr32_ChangeNtfy`.

**SCCDevice (`src/devices/scc.h`, `src/devices/scc.cpp`):**
- Move `static SCC_Ty SCC` → `SCCDevice::d_`.
- SCC has two channels (`Channel_Ty a[2]`), so the internal state struct is larger.
- Interrupt: `SCCInterruptRequest` wire → `VIAorSCCinterruptChngNtfy`.

**Backward compat:** Same forwarding-function pattern. `VIA2_Access()` and `SCC_Access()` global functions call through `g_via2->access()` and `g_scc->access()`.

**Validation:** Clean build + boot. Test: serial port operations (SCC), power-off behavior (VIA2 on Mac II).

**Commit:** `Wrap VIA2 and SCC in Device classes`

---

### Step 4.8 — Wrap remaining memory-mapped devices

Convert SCSI, IWM, ASC, Video, Screen, and Sony devices to Device classes. These follow the same pattern as VIA but are simpler (less wire interaction).

**In order of complexity (simplest first):**

1. **SCSIDevice** — `static SCSI_Ty SCSI_D` → `SCSIDevice::d_`. Simple memory-mapped access. No wires. No ICT tasks.

2. **IWMDevice** — `static IWM_Ty IWM` → `IWMDevice::d_`. Accesses `IWMvSel` wire (from VIA1 port A bit 5). No ICT tasks.

3. **ASCDevice** — `static ASC_Ty ASC` → `ASCDevice::d_`. Only on Mac II/IIx (and PB100). Has `SubTickNotify()` called from ICT subtick.

4. **VideoDevice** — `static Video_Ty` → `VideoDevice::d_`. Mac II only (`#if EmVidCard`). VBL interrupt via wire.

5. **ScreenDevice** — `static Screen_Ty` → `ScreenDevice::d_`. Manages screen buffer updates.

6. **SonyDevice** — Sony floppy driver. Uses the extension mechanism (`Extn_Access`). References ROM patching.

**For each device:**
- Move static state → class member.
- Move static functions → methods.
- Replace global Wires/ICT access → `machine_->wireBus()`/`machine_->ict()`.
- Add forwarding global function for backward compat.

**Validation after each device:** Build + boot. Test the specific device functionality.

**Commit per device or batched:**
- `Wrap SCSI and IWM in Device classes`
- `Wrap ASC, Video, and Screen in Device classes`
- `Wrap Sony in Device class`

---

### Step 4.9 — Wrap wire-driven and ICT-driven devices

These devices are NOT memory-mapped (no entry in the ATT / MMDV dispatch). They're driven by wire change notifications or ICT callbacks.

1. **RTCDevice** — Driven by VIA1 port B bits (RTCdataLine, RTCclock, RTCunEnabled). Registers wire callbacks:
   ```cpp
   machine_->wireBus().onChange(Wire_VIA1_iB0_RTCdataLine,
       [this]{ onDataLineChange(); });
   machine_->wireBus().onChange(Wire_VIA1_iB1_RTCclock,
       [this]{ onClockChange(); });
   machine_->wireBus().onChange(Wire_VIA1_iB2_RTCunEnabled,
       [this]{ onEnableChange(); });
   ```
   Also has `RTC_Interrupt()` called from `SixtiethSecondNotify()`.

2. **ADBDevice** — Driven by VIA1 port B bits (ADB_st0, ADB_st1, ADB_Int) and VIA1 CB2 (ADB_Data). Has ICT task `kICT_ADB_NewState`.

3. **KeyboardDevice** — Classic keyboard (`#if EmClassicKbrd`). Not used on Mac II (uses ADB instead). Has ICT tasks `kICT_Kybd_ReceiveCommand`, `kICT_Kybd_ReceiveEndCommand`.

4. **MouseDevice** — Provides mouse deltas. Called from `SixtiethSecondNotify()`.

5. **PMUDevice** — PowerBook only (`#if EmPMU`). Has ICT task `kICT_PMU_Task`.

6. **SoundDevice** — Sound output (`#if EmClassicSnd`). Not used on Mac II (uses ASC).

**For each device:**
- Move static state → class member.
- Override `zap()`/`reset()` instead of global `XXX_Zap()`/`XXX_Reset()`.
- Register wire callbacks and ICT handlers during `init()`.

**Validation:** Build + boot. Test: clock keeps time (RTC), ADB mouse/keyboard input works (ADB), sound plays (ASC/Sound).

**Commit:** `Wrap RTC, ADB, Keyboard, Mouse, PMU, and Sound in Device classes`

---

### Step 4.10 — Migrate ATT to use Device* pointers

Replace the `kMMDV_xxx` enum + `MMDV_Access()` switch with direct `Device*` dispatch.

**Changes to `ATTer` struct (in `machine.h`):**
```cpp
struct ATTer {
    ATTer*    Next;
    uint32_t  cmpmask;
    uint32_t  cmpvalu;
    uint32_t  Access;    // kATTA_ flags
    uint32_t  usemask;
    uint8_t*  usebase;
    Device*   device;    // replaces uint8_t MMDV
    uint8_t   Ntfy;
    // padding as needed
};
```

**Changes to `MMDV_Access()` → `DeviceAccess()`:**
```cpp
uint32_t DeviceAccess(ATTep p, uint32_t Data,
    bool WriteMem, bool ByteSize, uint32_t addr)
{
    // Byte-size validation and address masking still happens here
    // (model-specific quirks), then:
    return p->device->access(Data, WriteMem, maskedAddr);
}
```

**Migration strategy for the address validation/masking logic:**

The current `MMDV_Access()` does per-device, per-model address validation before calling `XXX_Access()`. This logic (~250 lines) needs to move somewhere. Options:

- **Option A: Into each Device::access()** — each device validates its own addresses. Pro: encapsulation. Con: duplicates model-specific masking across devices.
- **Option B: Into a per-device "address translator" function stored on ATTer** — each ATT entry gets a small lambda/function pointer that masks/validates the address before calling `device->access()`. Pro: keeps device code clean. Con: extra indirection.
- **Option C: Keep it in the dispatch function but use a device type tag** — essentially what we have now. Not a real improvement.

**Recommended: Option A.** Move the address validation into each `Device::access()` method. The validation logic is device-specific anyway (VIA uses `(addr >> 9) & kVIA1_Mask`, SCC uses `(addr >> 1) & kSCC_Mask`, IWM uses `(addr >> 9) & kIWM_Mask`, etc.). The model-specific quirks (the `#if CurEmMd` checks for byte/word access, odd/even address) become `if (machine_->config().isIIFamily())` runtime checks.

**Changes to `SetUp_address()`:**
Instead of `r.MMDV = kMMDV_VIA1`, use `r.device = g_via1` (or `machine->findDevice<VIA1Device>()`).

**Remove the `kMMDV_xxx` enum** — no longer needed.

**Remove the forwarding functions** (`VIA1_Access()`, `SCC_Access()` globals, etc.) — no longer needed since ATT dispatches directly.

**Remove `g_via1`, `g_scc`, etc. global pointers** — only `Machine*` is needed globally (and eventually not even that).

**Validation:** This is a high-risk step. Test thoroughly:
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Boot System 7. Exercise all device paths:
- Disk access (IWM + Sony)
- Serial (SCC)
- SCSI (hard disk images)
- Sound (ASC on Mac II)
- Video (Mac II video card)
- Keyboard + mouse (ADB → VIA1)
- Clock (RTC → VIA1)
- Multi-drive operations

**Commit:** `Replace MMDV_Access switch with direct Device* dispatch via ATT`

---

### Step 4.11 — Migrate ICT to `ICTScheduler` on Machine

Replace the global `ICTactive`/`ICTwhen`/`NextiCount` with the `ICTScheduler` class on Machine.

**Changes:**
1. Each device registers its ICT handlers during `init()`:
   ```cpp
   machine_->ict().registerTask(kICT_VIA1_Timer1Check,
       [this]{ doTimer1Check(); });
   ```
2. Remove the `ICT_DoTask()` switch in main.cpp — the scheduler calls registered handlers directly.
3. Replace `ICT_add(taskid, n)` calls in device code with `machine_->ict().add(taskid, n)`.
4. The CPU cycle coupling: in `Machine::init()`:
   ```cpp
   ict_.setCycleAccessors(
       [this]{ return cpu_->getCyclesRemaining(); },
       [this](int32_t n){ cpu_->setCyclesRemaining(n); }
   );
   ```
5. Remove the global `ICTactive`, `ICTwhen`, `NextiCount` variables.

**Validation:** Build + boot. Timing-sensitive test: ensure VIA timers fire at correct intervals (startup chime, cursor blink, key repeat).

**Commit:** `Migrate ICT scheduler to ICTScheduler class on Machine`

---

### Step 4.12 — Migrate Wires to `WireBus` on Machine

Replace the global `Wires[]` array and `#define` macros with `WireBus`.

**Changes:**
1. In each device's `init()`, register wire callbacks (replacing `#define VIA1_iB0_ChangeNtfy RTCdataLine_ChangeNtfy` etc.):
   ```cpp
   // In RTCDevice::init():
   machine_->wireBus().onChange(Wire_VIA1_iB0_RTCdataLine,
       [this]{ onDataLineChange(); });
   ```
2. In each device, replace `Wires[Wire_xxx]` reads with `machine_->wireBus().get(Wire_xxx)`.
3. In each device, replace `Wires[Wire_xxx] = val` writes with `machine_->wireBus().set(Wire_xxx, val)`.
4. The `#define` aliases in CNFUDPIC.h (like `#define MemOverlay (Wires[Wire_VIA1_iA4_MemOverlay])`) are removed. Each usage site uses the wire bus directly.
5. In `Machine::init()`, register the interrupt recalculation callback:
   ```cpp
   wireBus_.onChange(Wire_VIA1_InterruptRequest, [this]{ recalcIPL(); });
   wireBus_.onChange(Wire_VIA2_InterruptRequest, [this]{ recalcIPL(); });
   wireBus_.onChange(Wire_SCCInterruptRequest,   [this]{ recalcIPL(); });
   ```
6. Remove the global `Wires[]` array and `kNumWires` enum from CNFUDPIC.h.

**Wire ID management:**
Create a `src/core/wire_ids.h` that defines wire IDs as `constexpr int` values, derived from the model. For now, keep the Mac II wire set as a static enum. Making wire IDs model-specific (dynamically allocated) is a Phase 5 task when we add the model factory.

**Validation:** Build + boot. Test all inter-device interactions:
- RTC: VIA1 port B → RTCdataLine/RTCclock/RTCunEnabled → RTC device
- ADB: VIA1 port B → ADB_st0/ADB_st1/ADB_Int/ADB_Data → ADB device
- Memory overlay: VIA1 port A bit 4 → MemOverlay → address map rebuild
- 32-bit mode: VIA2 port B bit 3 → Addr32 → address map rebuild
- Interrupts: VIA1/VIA2/SCC interrupt wires → IPL recalculation → CPU

**Commit:** `Migrate Wires[] to WireBus on Machine; remove global wire macros`

---

### Step 4.13 — Extract CPU into a `CPU` class

Convert the CPU from static globals (`static struct regstruct regs`) to a class owned by Machine.

**New file: `src/cpu/cpu.h`**
```cpp
#pragma once
#include <cstdint>

class Machine;

class CPU {
public:
    explicit CPU(Machine& machine, bool use68020, bool emFPU, bool emMMU);

    // Execution
    void go_nCycles(int32_t n);

    // State
    int32_t getCyclesRemaining() const;
    void    setCyclesRemaining(int32_t n);

    // Interrupts
    void iplChangeNotify();
    void setIPLPointer(uint8_t* iplPtr);

    // Initialization
    void init();
    void reset();

    // Register access (for debugger, future Phase 8)
    uint32_t getDReg(int n) const;
    uint32_t getAReg(int n) const;
    uint32_t getPC() const;
    uint16_t getSR() const;
    void setDReg(int n, uint32_t val);
    void setAReg(int n, uint32_t val);
    void setPC(uint32_t val);

    // ATT interface
    struct ATTer* headATTel() const;
    void setHeadATTel(struct ATTer* p);

    // CPU variant flags (const for lifetime of CPU instance)
    const bool use68020;
    const bool emFPU;
    const bool emMMU;

private:
    Machine&  machine_;
    // The regstruct moves here as a member
    struct regstruct regs_;
};
```

**Strategy for the 55 `#if Use68020` blocks:**

The `Use68020` macro is currently `#define Use68020 1` in CNFUDPIC.h. To make it a runtime bool:

1. In m68k.cpp, replace `#if Use68020` with `if (cpu->use68020)` — but ONLY in the instruction decode tables and cold paths.
2. For the **hot instruction decode loop** (the main `switch` on opcode), performance matters. The decoded instruction dispatch table (`DecOpR disp_table[256*256]`) is built at init time and already points to the correct handler functions. The `#if Use68020` blocks in the **table building** code (`SetDisp()`, `DeCode4xx()`, etc. in `m68k_tables.cpp`) can become runtime `if` checks because they only run once.
3. Within individual instruction handlers, the `#if Use68020` blocks gate things like additional addressing modes (scale, index) and extended register access. These become `if (use68020)` — the branch predictor handles this efficiently since the value never changes.
4. The `#if EmFPU` blocks (3 total) gate inclusion of `fpu_emdev.h` code. Make the FPU opcode handler check `emFPU` at runtime and trap with "unimplemented instruction" if disabled.

**Changes to m68k.cpp:**
- Remove `static struct regstruct regs;` — it's now `CPU::regs_`.
- The macro `V_regs` (currently `regs`) becomes a macro that resolves to the CPU instance's regs.
- This is a large file (~9000 lines). The safest approach: keep the file as-is but route all register access through a `CPU*` pointer stored in a file-scope variable (set before each call to `go_nCycles`). This avoids rewriting every line:
  ```cpp
  static CPU* currentCPU = nullptr;
  #define V_regs (currentCPU->regs_)
  ```
  This is an intermediate step. Full encapsulation (passing CPU explicitly) can happen incrementally later.

**Validation:** Build + boot. CPU-intensive test: boot System 7, launch an application, exercise 68020 instructions.

**Commit:** `Extract CPU into CPU class; Use68020/EmFPU become runtime bools`

---

### Step 4.14 — Wire Machine into the main loop

Replace the scattered globals in main.cpp with Machine method calls.

**Changes to `src/core/main.cpp`:**
1. `EmulationReserveAlloc()` → `Machine::init()` (allocates RAM, creates devices).
2. `EmulatedHardwareZap()` → `Machine::zap()` (cold-starts all devices).
3. `customreset()` → `Machine::reset()`.
4. `DoEmulateOneTick()`:
   - `SixtiethSecondNotify()` calls device methods via the Machine instead of free functions.
   - `m68k_go_nCycles_1()` uses the Machine's CPU and ICTScheduler.
5. `ProgramMain()` creates a `Machine` instance:
   ```cpp
   void ProgramMain() {
       MachineConfig config = MachineConfigForModel(MacModel::II);
       Machine machine(config);
       machine.init();
       machine.zap();
       // ... platform init, main event loop ...
   }
   ```

**Global Machine pointer:**
During the transition, a global `Machine* g_machine` pointer provides backward compatibility for any remaining global-accessing code. Long-term (Phase 5+), this is removed.

**Validation:** Build + boot. Full functionality test.

**Commit:** `Wire Machine object into main loop; replace EmulationReserveAlloc/Zap/Reset`

---

### Step 4.15 — Convert `CurEmMd` compile-time checks to runtime

Replace the ~173 `#if CurEmMd` / `#elif CurEmMd` blocks with `if (config.model)` runtime checks.

**Strategy:** Process file-by-file:

1. **machine.cpp** (~70 refs): `SetUp_address()` has the largest concentration — the address map setup is different per model family. Convert the `#if CurEmMd <= kEmMd_Plus` / `#elif CurEmMd == kEmMd_II` / etc. blocks into `if (config.isCompactMac())` / `else if (config.isIIFamily())` / etc.

2. **rtc.cpp** (~25 refs): `HaveXPRAM` size, command decoding differences. Convert to `config.model >= MacModel::Plus` etc.

3. **rom.cpp** (~15 refs): ROM checksum validation, screen hacks per model. Convert to runtime model checks.

4. **sony.cpp** (~15 refs): Disk format, speed zones per model. Convert.

5. **scc.cpp, iwm.cpp, via.cpp** (~6 each): Minor per-model behavior variants.

6. **m68k.cpp**: The CPU file has ~10 `CurEmMd` refs, mostly for choosing between "compact Mac" vs "Mac II" interrupt priority schemes. These are in `VIAorSCCinterruptChngNtfy()` (which has already moved to `Machine::recalcIPL()` in Step 4.12) and in the ATT setup (already in Machine's memory controller).

**Per-file approach:**
- Replace `#if CurEmMd == kEmMd_II` with `if (machine_->config().model == MacModel::II)`.
- For the common pattern `#if (CurEmMd == kEmMd_II) || (CurEmMd == kEmMd_IIx)`, use `machine_->config().isIIFamily()`.
- For `#if CurEmMd <= kEmMd_Plus`, use `machine_->config().isCompactMac()`.
- For `#if CurEmMd >= kEmMd_SE`, use `machine_->config().model >= MacModel::SE` (works since enum is ordered).
- Leave the `#define CurEmMd kEmMd_II` in CNFUDPIC.h for now (it's still used by files not yet migrated). Remove it in Step 4.17.

**Compile after each file.** This is a large mechanical change but low risk — the runtime values match the compile-time constants for the Mac II model.

**Validation:** Build + boot. The behavior is identical because the `MachineConfig` defaults match the existing compile-time Mac II configuration.

**Commit (can be split per file if preferred):**
- `Convert CurEmMd checks to runtime in machine.cpp`
- `Convert CurEmMd checks to runtime in device files`

---

### Step 4.16 — Convert `EmXxx` compile-time device enables to runtime

Replace the `#if EmVIA1`, `#if EmVIA2`, `#if EmASC`, `#if EmADB`, `#if EmClassicKbrd`, `#if EmPMU`, `#if EmRTC`, `#if EmClassicSnd` guards.

**Strategy:**
- In the Machine's device factory, only create devices that are enabled in the config:
  ```cpp
  if (config.emVIA1)  addDevice(std::make_unique<VIA1Device>());
  if (config.emVIA2)  addDevice(std::make_unique<VIA2Device>());
  if (config.emASC)   addDevice(std::make_unique<ASCDevice>());
  // etc.
  ```
- In code that references a device conditionally (e.g., `#if EmVIA2 ... VIA2_Access() ...`), replace with a null check: `if (auto* via2 = machine->findDevice<VIA2Device>()) { ... }`.
- For performance-critical paths, cache the pointer: `VIA2Device* via2_ = machine->findDevice<VIA2Device>()` at init time.
- The `Em*` macros in CNFUDPIC.h remain temporarily (they're still used by the build system and some edge cases). They'll be removed together with `CurEmMd` in Step 4.17.

**Validation:** Build + boot.

**Commit:** `Convert EmXxx device-enable guards to runtime config checks`

---

### Step 4.17 — Remove compile-time model configuration

Clean up: remove the `#define CurEmMd`, `#define EmVIA1`, `#define Use68020`, `#define EmFPU`, etc. from CNFUDPIC.h. These are now fully runtime.

**Changes to `src/config/CNFUDPIC.h`:**
- Remove model-specific `#define`s (CurEmMd, Use68020, EmFPU, EmMMU, EmVIA1, EmVIA2, EmADB, EmClassicKbrd, EmRTC, EmPMU, EmASC, EmClassicSnd).
- Remove the Wire `enum` and `#define` aliases (now in `wire_ids.h`).
- Remove VIA port configuration `#define`s (now in `MachineConfig` or per-device init).
- Keep device-independent settings (Sony options, PRAM defaults, etc.) — or move them to `MachineConfig`.
- The file may become nearly empty and can be folded into `machine_config.h`.

**Changes to `cmake/models/MacII_CNFUDPIC.h`:**
- Same cleanup. This file was the build-system model config; it's now superseded by `MachineConfigForModel()`.

**Validation:** Full clean build from scratch:
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Boot System 7. Verify all functionality.

**Commit:** `Remove compile-time model/device configuration from CNFUDPIC.h`

---

### Step 4.18 — Model factory and second model validation

Implement `MachineConfigForModel()` for at least Mac II and Mac Plus, and validate that both work.

**Changes to `src/core/machine_config.cpp`:**
```cpp
MachineConfig MachineConfigForModel(MacModel model) {
    MachineConfig c;
    c.model = model;

    switch (model) {
        case MacModel::Plus:
            c.use68020   = false;
            c.emFPU      = false;
            c.ramASize   = 0x00400000;
            c.ramBSize   = 0;
            c.emVIA1     = true;
            c.emVIA2     = false;
            c.emADB      = false;
            c.emClassicKbrd = true;
            c.emASC      = false;
            c.emClassicSnd  = true;
            c.emVidCard  = false;
            c.includeVidMem = false;
            c.maxATTListN = 16;
            break;

        case MacModel::II:
            // defaults are already Mac II
            break;

        // ... other models
    }

    return c;
}
```

**Wire topology per model:**
Create `src/core/wire_ids.h` with per-model wire ID sets:
```cpp
namespace MacIIWires {
    enum {
        SoundDisable, SoundVolb0, /* ... */
        VIA1_InterruptRequest, VIA2_InterruptRequest,
        SCCInterruptRequest,
        /* ... */
        kNumWires
    };
}

namespace MacPlusWires {
    enum {
        SoundDisable, SoundVolb0, /* ... */
        VIA1_InterruptRequest,
        SCCInterruptRequest,
        /* ... (no VIA2 wires) */
        kNumWires
    };
}
```

**Validation — the acid test:**
```bash
# Mac II (default)
./minivmac.app/Contents/MacOS/minivmac MacII.ROM 608.hfs
# → boots System 7

# Mac Plus (once we add a Plus ROM)
./minivmac.app/Contents/MacOS/minivmac --model=Plus MacPlus.ROM system6.img
# → boots System 6
```

For now (before Phase 5 runtime config), the model can be selected via a CMake variable or a simple command-line parsing hack. The important thing is that both model configs produce correct emulation.

**Validation checklist for Mac Plus:**
- [ ] 68000 CPU (no 68020 instructions)
- [ ] Classic keyboard (not ADB)
- [ ] No VIA2
- [ ] Classic sound (not ASC)
- [ ] 24-bit addressing only
- [ ] 512×342 1-bit screen

**Commit:** `Implement model factory; validate Mac II and Mac Plus configurations`

---

### Step 4.19 — Final cleanup and documentation

1. Remove all backward-compatibility forwarding functions (global `VIA1_Access()`, `SCC_Access()`, etc.) — they should no longer be called by any code path.
2. Remove global device pointers (`g_via1`, `g_scc`, etc.) if still present.
3. Remove the global `g_machine` pointer if possible (may need to stay until Phase 5).
4. Audit for remaining global state that should be on Machine:
   - `InterruptButton` → `Machine::interruptButton_`
   - `my_disk_icon_addr` → `Machine::diskIconAddr_`
   - `GotOneAbnormal` → remove or make method-local
5. Update `docs/INSIGHTS.md` with the new architecture.
6. Run clean build + full boot test on macOS Cocoa.

**Validation:**
```bash
# Full clean build
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa

# Verify no old device globals remain
grep -rn 'g_via1\|g_scc\|g_scsi\|g_iwm\|g_asc' src/ --include='*.cpp' --include='*.h'
# Should return zero matches (or only in comments)

# Verify no CurEmMd compile-time checks remain in active code
grep -rn '#if.*CurEmMd' src/core/ src/cpu/ src/devices/ src/platform/cocoa.mm
# Should return zero matches

# Boot test
./bld/macos-cocoa/minivmac.app/Contents/MacOS/minivmac MacII.ROM extras/disks/608.hfs
```

**Commit:** `Phase 4 complete: Device interface and Machine object`

---

## File Changes Summary

### New Files

| File | Purpose |
|------|---------|
| `src/core/machine_config.h` | `MachineConfig` struct + `MacModel` enum |
| `src/core/machine_config.cpp` | `MachineConfigForModel()` factory |
| `src/core/machine_obj.h` | `Machine` class declaration |
| `src/core/machine_obj.cpp` | `Machine` class implementation |
| `src/core/wire_bus.h` | `WireBus` class declaration |
| `src/core/wire_bus.cpp` | `WireBus` implementation |
| `src/core/ict_scheduler.h` | `ICTScheduler` class declaration |
| `src/core/ict_scheduler.cpp` | `ICTScheduler` implementation |
| `src/core/wire_ids.h` | Per-model wire ID enums |
| `src/devices/device.h` | `Device` abstract interface |

### Heavily Modified Files

| File | Nature of change |
|------|-----------------|
| `src/devices/via.cpp` / `.h` | Static state → VIA1Device class |
| `src/devices/via2.cpp` / `.h` | Static state → VIA2Device class |
| `src/devices/scc.cpp` / `.h` | Static state → SCCDevice class |
| `src/devices/scsi.cpp` / `.h` | Static state → SCSIDevice class |
| `src/devices/iwm.cpp` / `.h` | Static state → IWMDevice class |
| `src/devices/asc.cpp` / `.h` | Static state → ASCDevice class |
| `src/devices/rtc.cpp` / `.h` | Static state → RTCDevice class |
| `src/devices/video.cpp` / `.h` | Static state → VideoDevice class |
| `src/devices/screen.cpp` / `.h` | Static state → ScreenDevice class |
| `src/devices/sony.cpp` / `.h` | Static state → SonyDevice class |
| `src/devices/adb.cpp` / `.h` | Static state → ADBDevice class |
| `src/devices/keyboard.cpp` / `.h` | Static state → KeyboardDevice class |
| `src/devices/mouse.cpp` / `.h` | Static state → MouseDevice class |
| `src/devices/pmu.cpp` / `.h` | Static state → PMUDevice class |
| `src/devices/sound.cpp` / `.h` | Static state → SoundDevice class |
| `src/core/machine.cpp` | ATT stores Device*, remove MMDV switch, remove CurEmMd #ifs |
| `src/core/machine.h` | Updated ATTer struct, remove global externs |
| `src/core/main.cpp` | Use Machine object, remove ICT_DoTask switch |
| `src/cpu/m68k.cpp` | regstruct → CPU class member, Use68020 → runtime bool |
| `src/cpu/m68k.h` | CPU class declaration |
| `src/cpu/m68k_tables.cpp` | Use68020 → runtime bool in table generation |
| `src/config/CNFUDPIC.h` | Remove CurEmMd, EmXxx, Wire enum, VIA config |
| `CMakeLists.txt` | Add new source files |

### Deleted Files

None (CNFUDPIC.h is simplified, not deleted).

---

## Commit Sequence

| # | Commit Message | Steps | Risk |
|---|---------------|-------|------|
| 1 | `Add MachineConfig struct with model-to-feature factory` | 4.1 | None — new files only |
| 2 | `Add Device abstract interface` | 4.2 | None — new file only |
| 3 | `Add WireBus class for runtime inter-device signal routing` | 4.3 | None — new files only |
| 4 | `Add ICTScheduler class for cycle-based task scheduling` | 4.4 | None — new files only |
| 5 | `Add Machine class shell` | 4.5 | None — new files only |
| 6 | `Wrap VIA1 in VIA1Device class implementing Device interface` | 4.6 | **High** — first device migration, most complex wiring |
| 7 | `Wrap VIA2 and SCC in Device classes` | 4.7 | Medium — follows VIA1 pattern |
| 8 | `Wrap SCSI and IWM in Device classes` | 4.8a | Low — simple devices |
| 9 | `Wrap ASC, Video, and Screen in Device classes` | 4.8b | Medium |
| 10 | `Wrap Sony in Device class` | 4.8c | Medium — extension mechanism |
| 11 | `Wrap RTC, ADB, Keyboard, Mouse, PMU, Sound in Device classes` | 4.9 | Medium — wire-driven devices |
| 12 | `Replace MMDV_Access switch with direct Device* dispatch via ATT` | 4.10 | **High** — changes hot path |
| 13 | `Migrate ICT scheduler to ICTScheduler class on Machine` | 4.11 | Medium — timing-sensitive |
| 14 | `Migrate Wires[] to WireBus on Machine; remove global wire macros` | 4.12 | **High** — touches all devices |
| 15 | `Extract CPU into CPU class; Use68020/EmFPU become runtime bools` | 4.13 | **High** — 9000-line file |
| 16 | `Wire Machine object into main loop` | 4.14 | Medium |
| 17 | `Convert CurEmMd checks to runtime in machine.cpp` | 4.15a | Medium |
| 18 | `Convert CurEmMd checks to runtime in device files` | 4.15b | Medium |
| 19 | `Convert EmXxx device-enable guards to runtime config checks` | 4.16 | Low |
| 20 | `Remove compile-time model/device configuration from CNFUDPIC.h` | 4.17 | Medium |
| 21 | `Implement model factory; validate Mac II and Mac Plus configurations` | 4.18 | **High** — first multi-model test |
| 22 | `Phase 4 complete: Device interface and Machine object` | 4.19 | Low — cleanup |

---

## Validation Checklist

After each commit, the following must pass:

### Build validation
```bash
rm -rf bld/macos-cocoa && cmake --preset macos-cocoa && cmake --build --preset macos-cocoa
```
Zero errors. Warning count should not increase.

### Boot validation (Mac II)
- [ ] System 7 boots to desktop (MacII.ROM + 608.hfs)
- [ ] Startup chime plays (ASC sound)
- [ ] Mouse tracks correctly
- [ ] Keyboard input works (type in a text field)
- [ ] Clock shows correct time (RTC)
- [ ] Floppy disk eject/insert works (Sony+IWM)
- [ ] Control mode overlay works (Ctrl+click)

### Timing validation
- [ ] Boot-to-desktop time is within 10% of pre-Phase-4 time
- [ ] Cursor blinks at normal rate (VIA timer 1)
- [ ] Key repeat works at correct speed (VIA timer)

### Final validation (Step 4.18, multi-model)
- [ ] Mac II boots System 7 (68020, ADB, VIA2, ASC, 32-bit)
- [ ] Mac Plus boots System 6 (68000, classic keyboard, no VIA2, classic sound, 24-bit)

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| **VIA1 migration breaks keyboard input** — VIA1 shift register handles Mac II ADB keyboard via CB2 | Silent keyboard failure | Test keyboard input after Step 4.6. Compare VIA register dumps before/after. |
| **Wire callback ordering** — notifications fire in registration order, which may differ from the old compile-time `#define` resolution order | Subtle timing differences | Register callbacks in the same order as the old `#define` aliases. Add assertions that wire values match expected state after each notification chain. |
| **ICT timing regression** — moving ICT from globals to a class adds one pointer dereference per cycle check | Performance regression in tight loops | Profile. The ICT check is ~5 instructions. One extra dereference is noise. Use `__builtin_expect` on the "no task pending" fast path. |
| **`std::function` overhead in WireBus callbacks** — each wire change calls through `std::function`, which may involve heap allocation | Performance regression on device interaction | Use a small fixed-size callback list. For the hottest wires (interrupt lines), consider direct function pointers instead of `std::function`. Profile to verify. |
| **ATT Device* pointer invalidation** — if devices are reconstructed after ATT is built | Dangling pointer crash | Rebuild ATT whenever device set changes. Use `unique_ptr` consistently. Document lifetime constraints. |
| **68020 → runtime `if` performance hit** — 55 branches in instruction handlers | CPU emulation slowdown | Branch predictor handles static-value branches perfectly. Benchmark: run a tight loop in emulated code, compare cycles-per-host-second before and after. Expected impact: <1%. |
| **CurEmMd migration breaks a model-specific code path** — a `#if` was converted incorrectly | Wrong behavior for specific Mac model | Validate with both Mac II and Mac Plus ROMs. Use `grep` to find any remaining `#if CurEmMd` after Step 4.17 and eliminate them. |
| **Multiple Machine instances share hidden global state** — some state we missed in the audit | Crash or corruption when testing | Search for remaining `static` variables in device files after migration. Any `static` that holds emulation state must move to the Device/Machine. |

---

## Execution Dependencies

- **Phase 3 must be complete** — files in final locations, code-as-headers converted.
- **Steps 4.1–4.5 are additive** (new files only) — can be done in parallel or any order.
- **Steps 4.6–4.9 must follow 4.1–4.5** — devices need the interfaces to implement.
- **Step 4.6 (VIA1) must come first among devices** — it's the most complex and sets the pattern for all others.
- **Step 4.10 (ATT migration) depends on 4.6–4.9** — all memory-mapped devices must be wrapped before we can change the dispatch.
- **Steps 4.11 (ICT) and 4.12 (Wires) can be done in either order** but both depend on 4.6–4.9.
- **Step 4.13 (CPU) depends on 4.11** (ICT scheduler coupling).
- **Step 4.14 (main loop) depends on 4.10–4.13** (all components migrated).
- **Steps 4.15–4.17 (CurEmMd removal) depend on 4.14** (Machine wired in).
- **Step 4.18 (multi-model) depends on 4.17** (all compile-time config removed).
- **Step 4.19 (cleanup) is the final step.**

```
4.1 ──┐
4.2 ──┤
4.3 ──┼── 4.6 ── 4.7 ── 4.8 ── 4.9 ──┬── 4.10 ──┬── 4.14 ── 4.15 ── 4.16 ── 4.17 ── 4.18 ── 4.19
4.4 ──┤                                ├── 4.11 ──┤
4.5 ──┘                                └── 4.12 ──┘
                                                   └── 4.13 ──┘
```

---

## Estimated Effort

| Steps | Description | Effort |
|-------|-------------|--------|
| 4.1–4.5 | New classes (MachineConfig, Device, WireBus, ICTScheduler, Machine shell) | ~4 hours |
| 4.6 | VIA1 Device class (most complex, sets pattern) | ~4–6 hours |
| 4.7 | VIA2 + SCC (following VIA1 pattern) | ~3–4 hours |
| 4.8 | SCSI, IWM, ASC, Video, Screen, Sony | ~4–6 hours |
| 4.9 | RTC, ADB, Keyboard, Mouse, PMU, Sound | ~3–4 hours |
| 4.10 | ATT → Device* dispatch | ~3–4 hours |
| 4.11–4.12 | ICT + Wires migration | ~4–6 hours |
| 4.13 | CPU class extraction | ~6–8 hours |
| 4.14 | Main loop integration | ~2–3 hours |
| 4.15–4.17 | CurEmMd/EmXxx → runtime | ~6–8 hours |
| 4.18 | Multi-model factory + validation | ~4–6 hours |
| 4.19 | Cleanup + docs | ~2 hours |
| **Total** | | **~45–60 hours** |

---

## Post-Phase 4 State

After this phase:

- A **`Machine` object** encapsulates all emulator state: config, CPU, devices, wires, ICT scheduler, and memory buffers.
- All **16 devices** implement the `Device` interface with their state as class members (not file-scope statics).
- **Inter-device communication** goes through `WireBus` with runtime-registered callbacks.
- **ATT dispatches directly** to `Device*` pointers — no enum switch.
- **CPU variant (68000/68020/FPU)** is a runtime flag, not a compile-time `#if`.
- **Mac model** is a runtime parameter — one binary can (in principle) emulate any supported model.
- **Zero `#if CurEmMd`** or `#if Use68020` remains in active code.
- The emulator boots System 7 (Mac II) with identical behavior to pre-Phase-4.
- Foundation is set for **Phase 5** (runtime configuration: TOML/command-line, any model from one binary).
