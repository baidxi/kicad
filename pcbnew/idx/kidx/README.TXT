
IDX is by definition a cross-software protocol.
Given the open source nature of KiCad, we can share this feature with other softwares.

For now, KiDX matures inside the KiCad codebase.
However, if other license-compatible software want to implement IDX, it might be worth to organize it as a library.
Therefore, inside this folder, we should not use KiCad classes or its dependencies ( like wx )
