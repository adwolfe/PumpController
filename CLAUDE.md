# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

QtPumpController is a Qt-based desktop application for controlling syringe pumps and monitoring conductivity during neuronal glycolysis experiments. The application manages multi-phase pump protocols with real-time conductivity monitoring via serial communication.

## Build System

This project uses qmake as the build system.

**Building the application:**
```bash
qmake PumpControllerQt.pro
make
```

**Building from Qt Creator:**
- Open `PumpControllerQt.pro` in Qt Creator
- Configure the project for your kit (Desktop arm darwin generic mach_o 64bit)
- Build using Cmd+B (macOS) or the Build menu

**Run the application:**
```bash
./PumpControllerQt  # From build directory
```

## Architecture

### Hardware Interface Layer

The application communicates with two types of hardware via serial ports:

**PumpInterface** (`pumpinterface.h/cpp`):
- Manages communication with multiple syringe pumps (New Era NE-1000 or compatible)
- Uses a command queue system via `PumpCommandWorker` to serialize commands
- Supports multi-phase protocols with rate, linear ramp, and stop functions
- Each pump is addressed individually but shares a single serial connection
- Commands are sent using a specialized protocol defined in `pumpcommands.h`

**CondInterface** (`condinterface.h/cpp`):
- Communicates with Thermo Orion Lab Star EC112 conductivity meter
- Uses `CondWorker` for command queuing similar to pump interface
- Only supports GETMEAS command due to limited USB connectivity of the meter
- Returns `CondReading` structs containing value, units, and timestamp

Both interfaces use the worker/thread pattern:
- Main interface objects handle serial port I/O
- Worker objects (`PumpCommandWorker`, `CondWorker`) run on separate threads
- Workers queue commands and wait for responses before sending next command
- This prevents command collision and ensures proper response handling

### Protocol and Data Management

**Protocol** (`protocol.h/cpp`):
- Represents an experimental protocol as time/concentration segments
- Generates smooth x/y data for visualization and pump control
- Segments are defined as [time_minutes, start_conc, end_conc]

**TableModel** (`tablemodel.h/cpp`):
- QAbstractTableModel for displaying/editing protocol segments in the UI
- Emits `segmentsChanged()` signal when user modifies the table
- Provides `getSegments()` to retrieve segment data for protocol generation

**PumpPhase** (in `pumpcommands.h`):
- Represents a single phase sent to pumps
- Phases can be: RAT (constant rate), LIN (linear ramp), or STP (stop)
- Each pump gets its own set of phases calculated from the protocol

### Main Controller

**PumpController** (`pumpcontroller.h/cpp`):
- Main window and application coordinator
- Converts protocol segments into pump phases using `generatePumpPhases()`
- Calculates individual pump flow rates based on concentration using `calculateFlowRates()`
- Uses two pumps (A and B) to create concentration gradients by mixing
- Concentration formula: `conc = (PaC * rateA + PbC * rateB) / totalRate`
- Manages timers for protocol execution and conductivity polling
- Stores experimental runs in `savedRuns` map indexed by start time

### Visualization

**PlotWidget** (`plotwidget.h/cpp`):
- Wrapper around QCustomPlot library for real-time data plotting
- Used for both protocol preview and conductivity monitoring
- Supports vertical line indicator showing current position in protocol

### UI Components

**ComsDialog** (`comsdialog.h/cpp`):
- Dialog for selecting serial ports for pump and conductivity meter
- User must configure COM ports before hardware interfaces can connect

## Key Workflows

### Starting an Experiment

1. User opens COM ports dialog and selects ports for pump and cond meter
2. User confirms pump settings (flow rate, concentrations PaC and PbC)
3. User builds protocol by adding time/concentration segments to table
4. User clicks "Send Protocol" which:
   - Converts segments to pump phases via `generatePumpPhases()`
   - Sends phases to each pump via `PumpInterface::setPhases()`
5. User clicks "Start Protocol" which:
   - Starts interval timer for conductivity measurements
   - Starts pumps with phase 0
   - Begins tracking elapsed time

### Pump Phase Generation

The `generatePumpPhases()` method in pumpcontroller.cpp:
- Takes protocol segments and starting phase number
- For each segment, calculates flow rates for pump A and B
- Creates paired phases (n, n+1) for linear ramps between concentrations
- Returns vector of phase vectors, one per pump
- Critical for converting user-friendly protocol into pump commands

### Real-time Monitoring

- `intervalTimer` ticks every `dt` seconds (default from Protocol)
- Each tick triggers `timerTick()` which requests conductivity reading
- Conductivity data flows: CondInterface → signal → PumpController → PlotWidget
- Data is stored in `condReadings` vector for later export
- `condPreReadings` buffer stores last 60 seconds before protocol start

## Dependencies

**Qt Modules:**
- QtCore
- QtGui
- QtWidgets
- QtSerialPort
- QtPrintSupport

**External Libraries:**
- QCustomPlot (included in `libs/qcustomplot/`)

## Serial Communication Notes

- Pumps use 19200 baud by default
- Conductivity meter uses 9600 baud
- Both use command-response pattern requiring queue management
- Serial buffers in interfaces handle partial message assembly
- Timeouts implemented in worker threads to handle non-responsive devices

## Version Information

Version is defined in the .pro file:
- VERSION_MAJOR = 1
- VERSION_MINOR = 0
- VERSION_BUILD = 2

Access via VERSION macro in code.
