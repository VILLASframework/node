# Asynchronous Process interface to S2SS

To "models" folder of OPAL project folder copy:
folder: include
folder: src
file: s2ss.mk

----------------------------------------------

.llm file should contain the following:
note: path to libOpalAsyncApiCore.a depends on version of RT-Lab  

```
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
```

--------------------------------------------------

In RT-Lab under Files tab, we should see the files listed above for .llm file

--------------------------------------------------

Development tab -> Compiler -> Compiler Command (makefile) add the following command
/usr/bin/make -f /usr/opalrt/common/bin/opalmodelmk

--------------------------------------------------

max umber of values in UDP packets:
there’s a „#define“ inside the implementation which must be changed accordingly.
The #define is in file: model_directory/include/config.h There you will find a directive called MAX_VALUES.

# Troubleshooting

## S2SS executable still running on OPAL-RT target

- During ***Build the model*** a message should be printed:
```
### Created executable: s2ss
```

- After the simulation stop
	`s2ss` may still stay alive after the simulation stop. You have to remove it manually because the next simulation start will not be able to start the new AsyncIP.

# Kill running s2ss on OPAL

1. Start putty.exe

2. Connect to OPAL by using the existing profiles
	- make sure that you are in the proper folder by  
		$ ll
   
3. Kill all running processes with name 's2ss'

	$ killall s2ss

4. Logout from OPAL

	$ exit

## Problem occurs when there are multiple subsystems (SM_, SS_, ...)

Even there is no OpAsyncIPCtrl in all subsystems, RT-Lab during building process wants to generate AsyncIP exe, if there is no OpAsyncIPCtrl it shows error

Workaround for now: place fake OpAsyncIPCtrl in each subsystem

Additional problem: After Load, only AsyncIP in SM_ is started

Zapravo: this helps that you do not need fake blocks:

.llm file should contain the following:
note: path to libOpalAsyncApiCore.a depends on version of RT-Lab  

[ExtraPutFilesComp]
C:\OPAL-RT\RT-LAB\v10.7.7.506\common\lib\redhawk\libOpalAsyncApiCore.a=Binary

but it seems that still you can you it only in SM_...

## Complex models

Source: http://www.opal-rt.com/kb-article/arinc-and-asyncserial-processes-complex-models

Question:

> I am working with a model that has multiple subsystems (2 or more) and I am using multiple asynchronous processes such as ARINC429 and AsyncSerial. Every time I compile and load my model, some files are either not transferred, transferred twice and I also get warnings such as File(s) not found. How can I make sure my settings for transferring files are good and make my model as efficient as possible?

Answer:

> This is quite a difficult issue to solve for everyone. It is a case by case study but there are some things you need to know and some guidelines to follow that will improve your file transfer process. This article is divided in two sections: ARINC429 and AsyncSerial as both asynchronous processes behave differently in their file transfer.

### ARINC 429

Baseline: The ARINC process does not have any automation system regarding the file transfers. Everything must be done manually (ie via MainControl/Configuration/Advanced/Files&Commands).

No mather where the source files are located on the host, either in the model's directory or somewhere else, it does not have any impact on how the transfer must be done. The source files are the .mk and .c files related to an ARINC process.

#### Case A: Model with one subsystem (SM)

In this case, follow these steps to transfer the files and generate everything:

1. Add the compilation command "make -f".
2. Add the source files to be tranferred to target during compilation (.c and .mk) in ASCII mode.
3. Add the executable to be retrieved from target at the end of compilation in binary mode.
4. Add the executable to be transferred to the target at load in binary mode.

Steps 2 to 4 must be done for each ARINC process used in the model.

#### Case B: Model with more than one subsystem (SM, SS, etc)

In this case, a few more things must be done to make it work:

1. As the librairies needed to compile asynchronous processes are transferred to SM only, you must add them to be transferred to target during compilation so all subsystems can be built without errors. The libs to transfer are: libOpal_429.a and libOpalAsyncApiCore.a Those librairies are available under C:\Opal-rt\RT-LABx.x.x\Common\lib\qnx6 and should be transferred in binary mode. If you do not transfer those librairies an error will occur during compilation (see KB article: http://www.opal-rt.com/kb-article/asyncproc-429-demo-junk-error-during-c...)
2. Add the compilation command "make -f".
3. Add the source files to be tranferred to target during compilation (.c and .mk) in ASCII mode.
4. Add the executable to be retrieved from target at the end of compilation in binary mode.
5. Add the executable to be transferred to the target at load in binary mode.

Steps 3 to 5 must be done for each ARINC process used in the model.

Bug found: In case B, a bug was found in RT-LAB 8.3.2 (and older versions). The executable created on the target is retrieved twice from one subsystem. This bug has been reported and is under investigation by the R&D team.

### ASYNCSERIAL

Baseline: The AsyncSerial process has an automation system regarding the file transfers. Some transfers must be done manually (ie via MainControl/Configuration/Advanced/Files&Commands), while some other files are automatically transferred by RT-LAB.

Note: The AsyncSerial process provided by RT-LAB contains a generic word structure, functional with the example models. The provided structure comprises a 8 bytes header followed by the data on 8 bytes each. The header is composed of a device ID (2 bytes), a message ID (4 bytes) and the message length (2 bytes). The data can contain up to 64 elements, each one on 8 bytes. If the user's device expects to send/receive data in another format, cast or size, the user must modify the asynchronous process source files accordingly. This is mainly done by modifying sections labeled as "FORMAT TO SPECIFIC PROTOCOL HERE".

The source files location is very important regarding the automatic transfer process. The source files are the .mk, .c and .h files related to the AsyncSerial process.

#### Case A: All source files are located in the RT-LAB model's directory (not a subdirectory or another external directory).

In this case, the .c and .mk files files are automatically transferred during compilation and the executable is retrieved at the end. The executable is also automatically transferred to target at load.

1. Add the .h file to be transferred to target during compilation in ASCII mode. This has been reported to R&D too as it should be automatic (just like it is for the .c and .mk file).
2. There is no need to add the "make -f /usr/opalrt/common/bin/opalmodelmk" command as it is done automatically. Compile and load. This works fine with multiple subsystems models and with multiple AsyncSerial processes as long as all the source files for all the processes are located in the model's directory.

#### Case B: The source files are located somewhere else on the host computer.

In this case, the transfer is more complicated and raises warnings.

1. Add the compilation command "make -f /usr/opalrt/common/bin/opalmodelmk".
2. Add the source files to be transferred to target during compilation (.c, .mk and .h) in ASCII mode.
3. The executable is retrieved automatically and it is transferred automatically at load.
4. In the compilation log, do not bother with the warnings about "File(s) not found". The reason for their appearance is that the automatic file transfer process search for those files in the model's directory but as they are somewhere else, they cannot be found. Those warnings usually come in pair, one for the .c file and one for the .mk file. Therefore, the number of AsyncSerial processes used in a model times 2 should give the number of warnings about File(s) not found. This has been reported to R&D.
5. When using multiple AsyncSerial processes, make sure you add the .c and .mk files for the second, third, fourth, etc processes because no warning or error will be displayed in the compilation log about the process not being generated, as RT-LAB has been able to build the first one. The only error you might encounter if you do not add those files for the subsequent processes is that the Name_of_executable_for_process_2 is not found when automatically retrieved at the end of compilation.
6. When using multiple AsyncSerial processes in multiple subsystems, it is possible that one of the AsyncSerial process found in subsystem 2 only is generated in subsystem 1 and subsystem 2 (and potentially all subsystems). The executable is therefore retrieved from all subsystems but transferred at load only in subsystem 2, which is good. This behavior has been reported to R&D too.

In any case, it is always possible to manually transfer or retrieve files to/from target at compilation or load (via telnet and ftp) but it is not recommended and from our tests it is not required at all unless there is a problem.

### How to add a compilation command in RT-LAB

Before compiling, go to the Development tab of your model. Reach for the Compiler tab inside the Development tab. Under Compiler Command, add your compilation command ( make -f /usr/opalrt/common/bin/opalmodelmk )
