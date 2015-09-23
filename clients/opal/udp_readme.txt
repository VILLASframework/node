
To "models" folder of OPAL project folder copy:
folder: include
folder: src
file: s2ss.mk

----------------------------------------------

.llm file should contain the following:
note: path to libOpalAsyncApiCore.a depends on version of RT-Lab  

[ExtraPutFilesComp]
C:\OPAL-RT\RT-LAB\v10.7.7.506\common\lib\redhawk\libOpalAsyncApiCore.a=Binary
include\config.h=Ascii
include\msg.h=Ascii
include\msg_format.h=Ascii
include\socket.h=Ascii
include\utils.h=Ascii
s2ss.mk=Ascii
src\msg.c=Ascii
src\s2ss.c=Ascii
src\socket.c=Ascii
src\utils.c=Ascii


----------------------------------------------------

In RT-Lab under Files tab, we should see the files listed above for .llm file

--------------------------------------------------

Development tab -> Compiler -> Compiler Command (makefile) add the following command
/usr/bin/make -f /usr/opalrt/common/bin/opalmodelmk


----------------------------------------------------
max umber of values in UDP packets:
there’s a „#define“ inside the AsyncIP implementation which must be changed accordingly.
The #define is in file: model_directory/include/config.h There you will find a directive called MAX_VALUES.
---------------------------------------------------


