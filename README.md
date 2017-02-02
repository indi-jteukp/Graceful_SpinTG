# Graceful_SpinTG
Task Graph implementation on SpiNNaker

## Directory structure:
- aplx: source codes for SpiNNaker program
- host: source codes for python program
- xl-stage: program that generates task graph (from York)

## General scenario:
1. Chip <0,0> will be considered as the the "Regional" supervisor program (REGSVP). It has:
  - core-1: monitor
  - core-2: profiler
  - core-3: source/sink
  - core-4..17: optimizer: dynamic mapper, etc
2. Other chips are for TG processing. Each chip has:
  - core-1: monitor
  - core-2: profiler
  - core-3..17: tgsdp, a.k.a processing elements (PE)
3. Host-PC will interact with the REGSVP:
  - Host-PC sends network topology to REGSVP (Monitor-node) along with the dependency list (as a data structure)
  - REGSVP confirms if it has enough resources (#Nodes). If it has, then start mapping according to the mapping algorithm: GA, etc. Then report the resulting map back to host-PC.
  - Host-PC sends data to SOURCE-node (core-3). SOURCE-node delivers data to corresponding nodes.
  - REGSVP might receives final output via its SINK-node (from one or more TG node(s)). The SINK-node then delivers the output to Host-PC.

## Possible scenario:
1. Multiple TG running on the same machine (only if resource, a.k.a #Nodes, is available)

## REGSVP components:
1. Monitor: 
   - receives network structure from host-PC and do initial mapping
   - invokes optimizer to do dynamic mapping later on, do process migration, etc.
2. Profiler:
   - generates global clock syncronizer
   - Measure temperature, frequency
   - receives info from node's profiler, combine all information, and send to host-PC
3. Source/Sink:
   - receives/sends packet from/to host-PC

## Each node components:
1. Monitor:
   - count the P2P traffic and, if possible in the future, do the traffic management:
     it will consider the load of other node before modifying the P2P routing table
   - report to REGSVP about the number of available PE, so that the REGSVP can decide
     which task node will be assigned to this chip
2. Profiler:
   - Measure temperature
   - Measure and alter chip's frequency
   - Synchronize clock (from Master Profiler in chip REGSVP)
   - Measure CPU load
   - Send all information to Master Profiler
3. Tgsdp, a.k.a PE (processing elements):
   - each core corresponds to one task output; hence, a task node has a max 15 output

## Contributors:
- indarsugiarto: aplx
- indi-jteukp: host, xl-stage

