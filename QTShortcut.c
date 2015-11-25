//////////
//
//	File:		QTShortcut.c
//
//	Contains:	Sample code for creating a shortcut to a QuickTime movie.
//
//	Written by:	Tim Monroe
//				Based on the Windows version of MakeRefMovie.c and the code in the Ice Floe Dispatch 15;
//				some comments based on release notes by Chris Flick.
//
//	Copyright:	� 1998 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <1>	 	11/20/98	rtm		first file
//	 
//	A shortcut is a QuickTime reference movie that references a single other movie. A typical reference
//	movie references more than one other movie, and the target of the reference movie is determined at
//	runtime depending on certain features of the user's machine, such as the actual components that are
//	available on the machine or the speed of the user's connection to the internet. (Reference movies
//	that select a target depending on the speed of the internet connection are "multiple data rate movies".)
//	A shortcut is essentially the simplest possible reference movie; all it does is refer to some target,
//	which is opened (or imported) when the reference movie is opened (or imported).
//
//	Shortcut movies can be useful as a cross-platform, media-centric aliasing mechanism. The data
//	reference contained in a shortcut can be any one of the data reference types supported by QuickTime
//	(an alias to a file, a URL to an ftp or http movie, and so forth) and that reference can address any
//	kind of data that can be opened by QuickTime. Thus, you could put shortcut movies on a server that
//	point to files elsewhere on the server, on another server, or even on the user's local machine. When
//	a user opens a shortcut movie, its target data is automatically opened. This indirection allows a
//	server administrator to move the files targetted by some shortcut movies to new locations and then
//	update the shortcuts. Any client references to the shortcuts (HTML embeddings, explicit URLs used by
//	an application, and so forth) would be unaffected by this move.
//	
//	Shortcut movies have been supported by QuickTime beginning with version 3.0. QuickTime version 4.0
//	introduced the Movie Toolbox function CreateShortcutMovieFile, which you can use to create a shortcut
//	movie given a data reference, its type, and a specification of the output shortcut movie file. This
//	sample code illustrates how to call the CreateShortcutMovieFile function.
//
//	If you want to create shortcut movies under versions of QuickTime prior to 4.0, then you'll need to
//	build a file containing the reference data yourself. The format of reference movies is currently
//	undocumented, but it's quite simple: the shortcut file should consist entirely of a single movie atom,
//	which in turn contains a movie data reference alias atom (of type MovieDataRefAliasAID). This atom
//	contains a single data reference atom. Finally, the data reference atom should contain the type of the
//	data reference followed immediately by the data reference itself. In short:
//
//					movie atom -> movie data reference alias atom -> data reference atom
//
//	(where "->" means: "contains exactly one"). This sample code also shows how to assemble a shortcut
//	movie yourself by building these atoms.
//	
//	IMPORTANT: The atoms that we'll create here (for instance, reference movie descriptor atoms)
//	are unrelated to QTAtom and QTAtomContainer structures introduced in QuickTime 2.1. The atoms
//	we need to work with here are more akin to "chunks" as used in AIFF files: an atom is simply
//	a 4-byte atom length, a 4-byte atom type, and some associated data (which itself may be other
//	atoms). All this stuff (length, type, and data) must be in big-endian format.
//
//////////


#include "QTShortCut.h"


//////////
//
// QTShortCut_CreateShortcutMovieFile
// Create a movie file that is a shortcut to the specified data reference.
//
//////////

OSErr QTShortCut_CreateShortcutMovieFile (Handle theDataRef, OSType theDataRefType, FSSpecPtr theFSSpecPtr)
{
	long	 	myVersion = 0L;
	OSErr		myErr = noErr;
	
	myErr = Gestalt(gestaltQuickTime, &myVersion);
	if (myErr != noErr)
		goto bail;

	if (((myVersion >> 16) & 0xffff) >= 0x0400) {
		// we're running under QuickTime 4.0 or greater; we can use the function CreateShortcutMovieFile

		myErr = CreateShortcutMovieFile(theFSSpecPtr,
										kShortcutFileCreator,
								 		smCurrentScript,
										createMovieFileDeleteCurFile | createMovieFileDontCreateResFile,
										theDataRef,
										theDataRefType);
										
	} else {
		// we're running under a version of QuickTime prior to 4.0; do the grunt work ourselves
		
		OSType							myDataRefType;
		unsigned long					myAtomHeaderSize;		
		Ptr								myData = NULL;
		Handle							myMoovAtom = NULL;

		//////////
		//
		// create the atom data that goes into a data reference atom (we will create this atom's
		// header when we create the movie atom that contains it); the atom data is the data reference
		// type followed by the data reference itself
		//
		//////////

		myDataRefType = EndianU32_NtoB(theDataRefType);
		myAtomHeaderSize = 2 * sizeof(long);		

		// allocate a data block and copy the data reference type and data reference into it
		myData = NewPtrClear(sizeof(OSType) + GetHandleSize(theDataRef));
		if (myData == NULL)
			goto bail;
				
		BlockMove(&myDataRefType, myData, sizeof(OSType));
		BlockMove(*theDataRef, (Ptr)(myData + sizeof(OSType)), GetHandleSize(theDataRef));

		//////////
		//
		// create a handle to contain the size and type fields of the movie atom, as well as
		// the size and type fields of the movie data reference alias atom contained in it
		// and of the data reference atom contained in the movie data reference alias atom
		//
		//////////
		
		myMoovAtom = NewHandleClear(3 * myAtomHeaderSize);
		if (myMoovAtom == NULL)
			goto bail;
		
		// fill in the size and type fields of the three atoms
		*((long *)(*myMoovAtom + 0x00)) = EndianU32_NtoB((3 * myAtomHeaderSize) + GetPtrSize(myData));
		*((long *)(*myMoovAtom + 0x04)) = EndianU32_NtoB(MovieAID);
		*((long *)(*myMoovAtom + 0x08)) = EndianU32_NtoB((2 * myAtomHeaderSize) + GetPtrSize(myData));
		*((long *)(*myMoovAtom + 0x0C)) = EndianU32_NtoB(MovieDataRefAliasAID);
		*((long *)(*myMoovAtom + 0x10)) = EndianU32_NtoB((1 * myAtomHeaderSize) + GetPtrSize(myData));
		*((long *)(*myMoovAtom + 0x14)) = EndianU32_NtoB(DataRefAID);

		// concatenate the data in myData onto the end of the movie atom
		myErr = PtrAndHand(myData, myMoovAtom, GetPtrSize(myData));
		if (myErr != noErr)
			goto bail;

		//////////
		//
		// create the shortcut movie file
		//
		//////////
		
		myErr = QTShortCut_WriteHandleToFile(myMoovAtom, theFSSpecPtr);
		
bail:
		if (myData != NULL)	
			DisposePtr(myData);

		if (myMoovAtom != NULL)
			DisposeHandle(myMoovAtom);
	}

	return(myErr);
}


//////////
//
// QTShortCut_WriteHandleToFile
// Write the data in the specified handle into the specified file;
// if the file already exists, it is overwritten.
//
//////////

OSErr QTShortCut_WriteHandleToFile (Handle theHandle, FSSpecPtr theFSSpecPtr)
{
	short			myRefNum = 0;
	short			myVolNum;
	long			mySize = 0;
	OSErr			myErr = paramErr;

	if (theHandle == NULL)
		goto bail;

	mySize = GetHandleSize(theHandle);
	if (mySize == 0)
		goto bail;

	HLock(theHandle);
	
	// delete the file;
	// if it doesn't exist yet, we'll get an error (fnfErr), which we just ignore
	myErr = FSpDelete(theFSSpecPtr);
	
	// create and open the file
	myErr = FSpCreate(theFSSpecPtr, kShortcutFileCreator, kShortcutFileType, smSystemScript);

	if (myErr == noErr)
		myErr = FSpOpenDF(theFSSpecPtr, fsRdWrPerm, &myRefNum);
	
	// position the file mark to the beginning of the file and write the data
	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, 0);

	if (myErr == noErr)
		myErr = FSWrite(myRefNum, &mySize, *theHandle);

	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, mySize);

	// resize the file to the number of bytes written
	if (myErr == noErr)
		myErr = SetEOF(myRefNum, mySize);
				
	// close the file			 
	if (myErr == noErr)		
		myErr = FSClose(myRefNum);

#if TARGET_OS_MAC	
	// flush the volume
	if (myErr == noErr)		
		myErr = GetVRefNum(myRefNum, &myVolNum);

	if (myErr == noErr)		
		myErr = FlushVol(NULL, myVolNum);
#endif	// TARGET_OS_MAC	

bail:
	HUnlock(theHandle);

	return(myErr);
}



