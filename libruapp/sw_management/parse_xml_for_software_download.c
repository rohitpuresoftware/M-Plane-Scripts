#ifndef __SW_MANAGEMENT__
#include"sw_management.h"
#endif
/* change_file_date : change the date/time of a file
    filename : the filename of the file where date/time must be modified
    dosdate : the new date at the MSDos format (4 bytes)
    tmu_date : the SAME new date at the tm_unz format */


const char *filename_to_extract=NULL;
int md5sum_fun(char *filename,char *out)
{
	FILE * file_ptr;
	char *ptr=malloc(255);
	file_ptr = fopen(filename, "r");
   if (file_ptr == NULL) {
      perror("Error opening file");
      fflush(stdout);
      return 1;
   }

   int n;
   MD5_CTX c;
   char buf[512];
   ssize_t bytes;
   unsigned char out1[MD5_DIGEST_LENGTH];

   MD5_Init( & c);
   do {
      bytes = fread(buf, 1, 512, file_ptr);
      MD5_Update( & c, buf, bytes);
   } while (bytes > 0);

   MD5_Final(out1, & c);

   for (n = 0; n < MD5_DIGEST_LENGTH; n++)
   {
	   sprintf(ptr,"%02x",out1[n]);
	   strcat(out,ptr);
   }   

   out=strdup((const char*)out1);
//   printf("=>%s\n",out1);

    return 0;
}

void change_file_date(filename,dosdate,tmu_date)
    const char *filename;
    uLong dosdate;
    tm_unz tmu_date;
{
  struct utimbuf ut;
  struct tm newdate;
  newdate.tm_sec = tmu_date.tm_sec;
  newdate.tm_min=tmu_date.tm_min;
  newdate.tm_hour=tmu_date.tm_hour;
  newdate.tm_mday=tmu_date.tm_mday;
  newdate.tm_mon=tmu_date.tm_mon;
  if (tmu_date.tm_year > 1900)
      newdate.tm_year=tmu_date.tm_year - 1900;
  else
      newdate.tm_year=tmu_date.tm_year ;
  newdate.tm_isdst=-1;

  ut.actime=ut.modtime=mktime(&newdate);
  utime(filename,&ut);
}
/* mymkdir and change_file_date are not 100 % portable
   As I don't know well Unix, I wait feedback for the unix portion */

int mymkdir(dirname)
    const char* dirname;
{
    int ret=0;
#ifdef _WIN32
    ret = _mkdir(dirname);
#elif unix
    ret = mkdir (dirname,0775);
#elif __APPLE__
    ret = mkdir (dirname,0775);
#endif
    return ret;
}

int makedir (newdir)
    char *newdir;
{
  char *buffer ;
  char *p; 
  int  len = (int)strlen(newdir);

  if (len <= 0)
    return 0;
  
  buffer = (char*)malloc(len+1);
        if (buffer==NULL)
        {
                printf("Error allocating memory\n");
                return UNZ_INTERNALERROR;
        }
  strcpy(buffer,newdir);
  
  if (buffer[len-1] == '/') {
    buffer[len-1] = '\0';
  }
  if (mymkdir(buffer) == 0)
    {
      free(buffer);
      return 1;
    }

  p = buffer+1;
  while (1)
    {
      char hold;

      while(*p && *p != '\\' && *p != '/')
        p++; 
      hold = *p;
      *p = 0;
      if ((mymkdir(buffer) == -1) && (errno == ENOENT))
        {
          printf("couldn't create directory %s\n",buffer);
          free(buffer);
          return 0;
        }
      if (hold == 0)
        break;
      *p++ = hold;
    }
  free(buffer);
  return 1;
}







int do_extract_currentfile(uf,popt_extract_without_path,popt_overwrite,password)
    unzFile uf;
    const int* popt_extract_without_path;
    int* popt_overwrite;
    const char* password;
{
    char filename_inzip[256];
    char* filename_withoutpath;
    char* p;
    int err=UNZ_OK;
    FILE *fout=NULL;
    void* buf;
    uInt size_buf;

    unz_file_info64 file_info;
    err = unzGetCurrentFileInfo64(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

    if (err!=UNZ_OK)
    {
        printf("error %d with zipfile in unzGetCurrentFileInfo\n",err);
        return err;
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void*)malloc(size_buf);
    if (buf==NULL)
    {
        printf("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }

    p = filename_withoutpath = filename_inzip;
    while ((*p) != '\0')
    {
        if (((*p)=='/') || ((*p)=='\\'))
            filename_withoutpath = p+1;
        p++;
    }

    if ((*filename_withoutpath)=='\0')
    {
        if ((*popt_extract_without_path)==0)
        {
//            printf("creating directory: %s\n",filename_inzip);
            mymkdir(filename_inzip);
        }
    }
    else
    {
        const char* write_filename;
        int skip=0;
                        if ((*popt_extract_without_path)==0)
            write_filename = filename_inzip;
        else
            write_filename = filename_withoutpath;

        err = unzOpenCurrentFilePassword(uf,password);
        if (err!=UNZ_OK)
        {
            printf("error %d with zipfile in unzOpenCurrentFilePassword\n",err);
        }

        if (((*popt_overwrite)==0) && (err==UNZ_OK))
        {
            char rep=0;
            FILE* ftestexist;
            ftestexist = FOPEN_FUNC(write_filename,"rb");
            if (ftestexist!=NULL)
            {
                fclose(ftestexist);
                do
                {
                    char answer[128];
                    int ret;

                    printf("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ",write_filename);
                    ret = scanf("%1s",answer);
                    if (ret != 1)
                    {
                       exit(EXIT_FAILURE);
                    }
                    rep = answer[0] ;
                    if ((rep>='a') && (rep<='z'))
                        rep -= 0x20;
                }
                while ((rep!='Y') && (rep!='N') && (rep!='A'));
            }

            if (rep == 'N')
                skip = 1;

            if (rep == 'A')
                *popt_overwrite=1;
        }

        if ((skip==0) && (err==UNZ_OK))
        {
            fout=FOPEN_FUNC(write_filename,"wb");
            /* some zipfile don't contain directory alone before file */
            if ((fout==NULL) && ((*popt_extract_without_path)==0) &&
                                (filename_withoutpath!=(char*)filename_inzip))
            {
                    char c=*(filename_withoutpath-1);
                *(filename_withoutpath-1)='\0';
                makedir(write_filename);
                *(filename_withoutpath-1)=c;
                fout=FOPEN_FUNC(write_filename,"wb");
            }

            if (fout==NULL)
            {
                printf("error opening %s\n",write_filename);
            }
        }

        if (fout!=NULL)
        {
            //printf(" extracting: %s\n",write_filename);

            do
            {
                err = unzReadCurrentFile(uf,buf,size_buf);
                if (err<0)
                {
                    printf("error %d with zipfile in unzReadCurrentFile\n",err);
                    break;
                }
                if (err>0)
                    if (fwrite(buf,err,1,fout)!=1)
                    {
                        printf("error in writing extracted file\n");
                        err=UNZ_ERRNO;
                        break;
                    }
            }
            while (err>0);
            if (fout)
                    fclose(fout);

            if (err==0)
                change_file_date(write_filename,file_info.dosDate,
                                 file_info.tmu_date);
        }

        if (err==UNZ_OK)
        {
            err = unzCloseCurrentFile (uf);
            if (err!=UNZ_OK)
            {
                printf("error %d with zipfile in unzCloseCurrentFile\n",err);
            }
        }
        else
            unzCloseCurrentFile(uf); /* don't lose the error */
              }

    free(buf);
    return err;
}



int do_extract(uf,opt_extract_without_path,opt_overwrite,password)
    unzFile uf; 
    int opt_extract_without_path;
    int opt_overwrite;
    const char* password;
{
    uLong i;
    unz_global_info64 gi; 
    int err;
//    FILE* fout=NULL;

    err = unzGetGlobalInfo64(uf,&gi);
    if (err!=UNZ_OK)
        printf("error %d with zipfile in unzGetGlobalInfo \n",err);

    for (i=0;i<gi.number_entry;i++)
    {   
        if (do_extract_currentfile(uf,&opt_extract_without_path,
                                      &opt_overwrite,
                                      password) != UNZ_OK)
            break;

        if ((i+1)<gi.number_entry)
        {   
            err = unzGoToNextFile(uf);
            if (err!=UNZ_OK)
            {   
                printf("error %d with zipfile in unzGoToNextFile\n",err);
                break;
            }   
        }   
    }   

    return 0;
}

int unzip(char *filename,char*dest)
{
	int rc=0;
	int opt_do_extract_withoutpath=0;
//	int opt_overwrite=0;
	char filename_try[MAXFILENAME+16] = "";
	unzFile uf=NULL;

	if (filename!=NULL)
	{
		strncpy(filename_try, filename,MAXFILENAME-1);
		/* strncpy doesnt append the trailing NULL, of the string is too long. */
		filename_try[ MAXFILENAME ] = '\0';


		uf = unzOpen64(filename);
		if (uf==NULL)
		{
			strcat(filename_try,".zip");
			uf = unzOpen64(filename_try);
		}

	}
	if (uf==NULL)                                                                                    
	{                                                                                                
		printf("Cannot open %s or %s.zip\n",filename,filename);                                
		return 1;                                                                                    
	}

	if (chdir(dest))                                                        
	{                                                                                            
		printf("Error changing into %s, aborting\n", dest);
		return 1;
	}
	rc = do_extract(uf, opt_do_extract_withoutpath, 1, NULL);
	unzClose(uf); 
	return rc;
}

int remove_directory(const char *path) {
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d) {
      struct dirent *p;

      r = 0;
      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          /* Skip the names "." and ".." as we don't want to recurse on them. */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
             continue;

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf) {
             struct stat statbuf;

             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode))
                   r2 = remove_directory(buf);
                else
                   r2 = unlink(buf);
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }

   if (!r)
      r = rmdir(path);

   return r;
}



int sw_install(char *filename,char *dest,char *error)	
{

	if(filename==NULL ){
		printf("Filename is NULL\n");
		error=strdup("Filename is NULL");
		return 1;
	}
	else if(dest==NULL){
		printf("Destination is NULL\n");
		error=strdup("Destination is NULL");
		return 1;
	}	
	else
	{
		DIR* dir = opendir(dest);
		if (dir) {
		    /* Directory exists. */
			    closedir(dir);
		} else if (ENOENT == errno) {
		    /* Directory does not exist. */

			if(mkdir(dest,S_IRWXU))
			{
				error=strdup("slot memory creation failed\n");
				return 1;
			}

		} else {
		    /* opendir() failed for some other reason. */

				error=strdup("slot memory access failed\n");
				return 1;
		}


	//	char sys_cmd[100]={0};
		char template[255]="/tmp/here.XXXXXX";
		char *temp_dir=mkdtemp(template);
		
		char *dir_name=strdup(temp_dir);
		
//		unzip(argv[1],temp_dir);

	if(unzip(filename,temp_dir)==0)
#if 1
		{
			sprintf(template,"%s/manifest.xml",temp_dir);

			FILE *fp=fopen(template,"r");
			if(fp==NULL)
			{
				printf("manifest.xml file open failed\n");
				error=strdup("manifest.xml file didn't found");
				return 1;

			}
			char *line_data=NULL;
			size_t n=0;

			char *path=(char *)malloc(255);
			char *md5sum=malloc(255);
			char *cal_md5sum=malloc(255);

			while(feof(fp)==0)
			{
				getline(&line_data,&n,fp);

				char *path_name=strstr(line_data,"path");

				if(path_name!=NULL)
				{
					memset(path,0,255);
					memset(cal_md5sum,0,255);
					path_name=strchr(path_name,'"')+1;

					strncpy(path,path_name,strchr(path_name,'"')-path_name);

					path_name=strstr(path_name,"=\"")+2;

					strncpy(md5sum,path_name,strchr(path_name,'"')-path_name);
					
//					printf("path:%s md5sum:%s\n",path,md5sum);
					md5sum_fun(path,&cal_md5sum[0]);
//					printf("--->%s\n",cal_md5sum);

					if(strstr(md5sum,cal_md5sum)!=0){
				//	if(strcmp(md5sum,cal_md5sum)==0){
					//	printf("match for file %s\n",path);
					}
					else{
				//		printf("doesn't match %s\n",path);
						error=strdup("md5sum miss match");
						fclose(fp);
						free(line_data);
						free(path);
						return 1;
					}
					
				}
			}
			fclose(fp);
			free(line_data);
			free(path);
			unzip(filename,dest);
			remove_directory(dir_name);
			perror("==>");
			free(dir_name);

		}
#endif
		return 0;
	}
		return 0;
}

