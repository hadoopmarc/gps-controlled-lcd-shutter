# Custom Footprints for the PCB

This folder contains custom footprints for producing the PCB of the GPS LCD shutter. They should be imported in KiCad. When using KiCad in Ubuntu they are put at the ${KICAD8_3RD_PARTY} location, which is short for ~/snap/kicad/current/.local/share/kicad/8.0/3rdparty.

The footprints for the Neo M8N module and the SDcard reader do not have associated symbols. If you want them to use in other projects, you can simply take a standard header pin connector from the KiCAD library as symbol.

In addition to the custom footprints in this git repository, you need to download and import the custom model and footprint of the H-bridge module from: [https://www.snapeda.com/parts/ROB-14450/SparkFun%20Electronics/view-part/](https://www.snapeda.com/parts/ROB-14450/SparkFun%20Electronics/view-part/).

After importing all footprints in KiCad, the relevant part of your directory structure looks like:

```bash
~/snap/kicad/current/.local/share/kicad/8.0/3rdparty$ tree .
.
├── Neo-M8N_Module.pretty
│   └── Neo-M8N_1x05_P2.50mm_Horizontal.kicad_mod
├── SDcard_IPC_breakout.pretty
│   └── SDcard_IPC_breakout_1x06_P2.54mm_Vertical.kicad_mod
├── TB6612_Module_ROB-144450.pretty
│   └── TB6612_Module_ROB-14450.kicad_mod
├── Variants.pretty
│   └── TerminalBlock_RND_1x02_P5.00mm_Horizontal_Small_Courtyard.kicad_mod
└── TB6612_module_ROB-14450
    ├── how-to-import.htm
    ├── ROB-14450.kicad_sym
    └── ROB-14450.step

5 directories, 6 files
```






