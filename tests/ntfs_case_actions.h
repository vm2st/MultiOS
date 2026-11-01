/* Real mutations of existing case-distinct NTFS siblings. Shared undo model. */
static int action(u32 which){static u8 large[(MULTIOS_UNDO_MAX_RECORDS+1)*512];
 switch(which){
 case 0:return fs_put("/file.txt",(const u8*)"replacement",11,1,0);
 case 1:return fs_write64("/CaseDir/item.bin",509,(const u8*)"PATCH",5);
 case 2:return fs_stream_change("/Stream.bin","notes",MULTIOS_STREAM_REPLACE,0,(const u8*)"updated",7);
 case 3:return fs_put("/Copied.bin",0,0,4,"/File.txt");
 case 4:return fs_link("/file.txt","/Alias.bin");
 case 5:return fs_rename("/File.txt","Renamed.txt");
 case 6:return fs_remove("/file.txt",0);
 case 7:return fs_resize64("/casedir/item.bin",16385);
 default:return fs_put("/too-large.bin",large,(budget+1)*512,0,0);
 }
}
