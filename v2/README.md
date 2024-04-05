# Reflow v2

## TODO Features
- [ ] implement learn max-ramp
- [ ] implement console for overriding profile temperature
- [ ] implement console for performing real-time profile
- [ ] implement persist time-series
- [ ] implement resolution-reduction on frontend as time-series gets larger
- [ ] implement job export
- [ ] implement y axis shrink on bound exceeding
- [ ] implement maintain learn
- [ ] implement remote-mode or desktop-mode
- [ ] implement color theme
- [ ] implement straight-forward firmware serial API
- [ ] implement straight-forward server API (with websocket API)

## Notes
### Separation of Concerns
#### Controller (firmware)
* firmware controller should be concerned with the most basic concepts of operation.
* the controller should be minimally concerned with business logic of how it is used.
* basic concerns of controller should be:
 * **reporting temperature as a function of time**
  - doesn't zero out time: just reports time in seconds (no notion of "jobs")
 * **setting temperature to a set point or according to a linear function of time**
  - we can provide a scalar fixed point
  - or we can provide a slope
  - controller knows nothing of "profiles"
 * **directly driving heating elements**
  - allows daemon (client) to drive elements with a duty cycle [0, 100]
 * **safety**
  - turns off heat elements if max temperature is reached
 * **tuning (learning) pid parameters**
  - in order to control the heat accurately, the controller must be able to self tune the pid parameters
  - the "max slope" estimation is not part of this, just a heuristic for the daemon and in fact the controller conceptually knows nothing about this.
 * **setting (arbitrary) key-values in NVM**
  - this is required for the oven to remember pid parameters (though, pid parameters are in the "hash-map" implementation and cannot be overwritten by the client)
  - but can also be used to store additional data that a particular client can use (e.g. max-slope, machine name, etc.)
 * **reading all key-values from NVM**
  - this allows us to know what is in the hash map
  - we can also read the pid parameters
#### Daemon (backend)
* the daemon should be concerned with higher level concepts of operation
* fundamental concerns of daemon should be:
 * **loading/storing temperature profiles**
  - a temperature profile being some function of temperature over time
  - these can be multi-phase
  - a profile can also be "real-time": changing as we go
 * **managing (starting/stopping/pausing) "jobs"**
  - a job is a "run" of a profile
  - useful for zeroing time with respect to the beginning
  - useful for grouping data into a segmented time-series
  - useful for storing data about individual time-series
 * **sending/receiving information from controller**
 * **recording/storing temperature data**
  - converts time from controller to timestamp
  - stores data in memory for last n (2?) jobs (unless too big)
  - stores data to a db if requested
  - can downsample data
 * **reads/reports stored temperature data of a run**
  - a client (frontend) to display past jobs.
 * **managing learning cycle**
  - oversees learning and storing max slope of the oven (analyze a "driven" job)
  - oversees initiating pid tuning of oven
 * **manages renaming machine and storing other metadata about machine**
 * **manages being run in server-mode or desktop-mode**
  - can be run as a standalone desktop application (run in browser), has a .desktop file, automatically shutsdown if browser tab is quit
  - can be run as a server which connects to a remote (not localhost) client (via webbrowser), will not die if tab shutsdown
 * **manages higher level safety**
  - if no clients are connected, disables heating elements (enforces that you must be observing it. can't "set and forget")
 * **manages which controller we are connected to**
  - searches for controllers (usb connected devices)
#### Client (frontend)
 * **display real-time temperature (DRO)**
 * **display controller status (connected/disconnected)**
 * **display temperature profile graph**
  - adjustable over time and max temp
 * **display current run information**
 * **display past jobs**
 * **real-time programming console**
 * **profile creator**
 * **controls for starting/stopping/pausing job**
 * **controls for learning**
  - these should be not a top level control, but accessible after clicking something else.
 * **controls for connecting to a particular oven USB device**
 * **controls for saving a job**
 * **downsample real-time data (optimization)**
 * **controls for driving (pulsing) the heating elements with a certain duty cycle**
### Architecture
#### Controller
* controller states:
 * **disabled** - elements cannot be turned on. they are off.
 * **tuning** - controller is learning PID parameters.
 * **idle** - controller is not controlling temperature.
 * **running** - controller is actively controlling temperature.
* reports temperature in every state.
* responds to commands in every state.
##### API
###### Commands (from daemon)
* 0x00 - get status
* 0x01 - get info (includes k-v values)
* 0x10 - disable
* 0x11 - enable
* 0x12 - become idle (stops tuning as well)
* 0x20 <MSB> <LSB> - set scalar target temperature (big endian)
* 0x21 <MSB> <LSB> - set target temperature slope (big endian)
* 0x22 <DUT> - drive heating elements at duty cycle
* 0x30 - tune all
* 0x31 - tune base (maintain) power parameter
* 0x32 - tune +velocity (ramp) power parameter
* 0x33 - tune inertia (insulation) power parameter
* 0x40 <ADDR> <N_BYTES> <DATA MSB> ... <DATA LSB> - set data (?)
###### Responses (from controller)
* 0x00 <DATA> - status
 * DATA[0] - 1:enabled, 0:disabled
 * DATA[1] - 1:running, 0:idle
 * DATA[2] - 1:tuning, 0:not-tuning
 * DATA[3:4] - 0:tuning base, 1:tuning velocity, 2: tuning inertia
 * DATA[5:7] - don't care
* 0x01 <N_BYTES> <MSB> ... <LSB> - info
* 0x20 <MSB> <LSB> - scalar target set
* 0x21 <MSB> <LSB> - temp slope set
* 0x22 <DUT> - duty cycle set
### Learning
* the controller can tune the pid itself if told to.
* other learnings (max slope) can be steered by the daemon.
#### Old Version
broken up into:
* Power Learning Phase: `updateRelearnedPower()`
 * initially ramps up to a particular temperature
 * trying to maintain a constant temperature
  - if over temperature, turn off, decrease duty cycle (base power we are learning)
  - if under temperature or equal temperature, enable heating
* Inertia Learning Phase: `InertiaStage::update()`
 * time (ms) it takes to ramp from a starting temperature (120) to an ending temperature (150)
* Insulation Learning Phase: `InsulationStage::update()`
 * time (ms) it takes to cool down from starting temp (150) to ending temp (120)

`learnedPower` and `learnedInsulation` are used to calculate the baseline duty cycle (`getBaseOutputPower()`).
`learnedInertia` is used to compute the derivative parameter. since learned inertia actually is like the derivative.
