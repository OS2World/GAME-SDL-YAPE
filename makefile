#                      -- Makefile to create EXE file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
exename=yape
object_files=cpu.obj diskio.obj interface.obj keyboard.obj main.obj sound.obj tape.obj tedmem.obj archdep.obj

# Create debug build or not?
#debug_build=defined

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
#has_resource_file=defined
#rcname=WaWE-Resources

# The result can be LXLite'd too
#create_packed_exe=defined

#-----------------------------------------------------------------------------
# The next part is somewhat general, for creation of EXE files.
#-----------------------------------------------------------------------------

!ifdef debug_build
debugflags = -d2 -dDEBUG_BUILD
linkfilename = yape_Debug
!else
debugflags =
linkfilename = yape_NoDebug
!endif

cflags = $(debugflags) -bm -bt=OS2 -5 -fpi -sg -ei -otexan

.before
    set include=$(%os2tk)\h;$(%include)

.extensions:
.extensions: .exe .obj .cpp

all : $(exename).exe

$(exename).exe: $(object_files)

.cpp.obj : .AUTODEPEND
    wpp386 $[* $(cflags)

$(exename).exe: $(object_files)
    wlink @$(linkfilename)
!ifdef has_resource_file
    rc $(rcname) $(exename).exe
!endif
!ifdef create_packed_exe
    lxlite $(exename).exe
!endif
clean : .SYMBOLIC
        @if exist $(exename).exe del $(exename).exe
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
