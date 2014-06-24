findpg
======

This is an Windbg extension to find kernel pages allocated by PatchGuard. This program allows us to know how many PatchGuard contexts are running on a target environment and will help security researchers who want to analyze PatchGuard on their own.

Installation
-----------------

1. Make sure that [Visual C++ Redistributable Packages for Visual Studio 2013](http://www.microsoft.com/en-ca/download/details.aspx?id=40784) has already been installed.
2. Start WinDbg (only x64 version of WinDbg is supported)
2. Either attach a target kernel, open a crash dump file or start local kernel debugging session using livekd.
3. Load the extension by the following command.

    > .load \<fullpath_to_the_DLL_file\>

   If you copied findpg.dll into a \<WINDBG_DIR\>/winext folder, you can omit a path.

    > .load findpg

4. Use !findpg to display base addresses of pages allocated for PatchGuard.
    or !help to display usage of this extension.

    > !findpg
    
Sample Output
-----------------
![sample_output](/img/sample.png)

- The first field shows a type of memory region
- Base address is the address of beginning of the pages allocated for a PatchGuard context. The contents will be encrypted.
- Size is a size of the region. Apparently, it should always be page align when it is PatchGuard's page.
- The first field of randomness is the number of 0x00 or 0xff in the first 100 bytes of the page. If the page is really  encrypted, it should be relatively low number such as less than 5.
- The second field of randomness is the number of unique bytes in the first 100 bytes of the page. When the page is really encrypted, it should be relatively high number such as greater than 70.
- The remaining texts are description of the Pooltag. If the pooltag seems to be some third party related one, it will not be a PatchGuard page. On the other hand, if it seems to be a legitimate tag, it does NOT mean that it is NOT a PatchGuard page.

Supported Platforms
-----------------
Host:
- Windows 7 SP1 x64 and later 
- x64 Debugger

Target:
- Windows Vista SP2 x64 and later

License
-----------------
This software is released under the MIT License, see LICENSE.    
