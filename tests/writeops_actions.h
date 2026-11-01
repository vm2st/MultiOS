/* Extra operations run through the unchanged real-FS persistence model. */
static int action(u32 which){static u8 large[(MULTIOS_UNDO_MAX_RECORDS+1)*512];
 switch(which){
 case 0:return fs_put("/ReadonlyCopy.bin",0,0,4,"/ReadOnly.txt");
 case 1:return fs_rename("/MixedCase.txt","mixedcase.TXT");
 case 2:return fs_rename("/MixedDir","mixeddir");
 case 3:return fs_put("/NormalCopy.txt",0,0,4,"/old.txt");
 case 4:return fs_put("/MixedCase.txt",(const u8*)"append",6,2,0);
 case 5:return fs_write64("/MixedCase.txt",2,(const u8*)"PATCH",5);
 case 6:return fs_resize64("/MixedCase.txt",8193);
 case 7:return fs_put("/MixedDir/ReadonlyCopy.bin",0,0,4,"/ReadOnly.txt");
 default:return fs_put("/too-large.bin",large,(budget+1)*512,0,0);
 }
}
