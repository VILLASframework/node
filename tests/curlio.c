#define FGETSFILE "fgets.test"
#define FREADFILE "fread.test"
#define REWINDFILE "rewind.test"

/* Small main program to retrive from a url using fgets and fread saving the
 * output to two test files (note the fgets method will corrupt binary files if
 * they contain 0 chars */
Test(curlio, main)
{
  URL_FILE *handle;
  FILE *outf;

  size_t nread;
  char buffer[256];
  const char *url;

  if(argc < 2)
    url="http://192.168.7.3/testfile";/* default to testurl */
  else
    url=argv[1];/* use passed url */

  /* copy from url line by line with fgets */
  outf=fopen(FGETSFILE, "wb+");
  if(!outf) {
    perror("couldn't open fgets output file\n");
    return 1;
  }

  handle = url_fopen(url, "r");
  if(!handle) {
    printf("couldn't url_fopen() %s\n", url);
    fclose(outf);
    return 2;
  }

  while(!url_feof(handle)) {
    url_fgets(buffer, sizeof(buffer), handle);
    fwrite(buffer, 1, strlen(buffer), outf);
  }

  url_fclose(handle);

  fclose(outf);


  /* Copy from url with fread */
  outf=fopen(FREADFILE, "wb+");
  if(!outf) {
    perror("couldn't open fread output file\n");
    return 1;
  }

  handle = url_fopen("testfile", "r");
  if(!handle) {
    printf("couldn't url_fopen() testfile\n");
    fclose(outf);
    return 2;
  }

  do {
    nread = url_fread(buffer, 1, sizeof(buffer), handle);
    fwrite(buffer, 1, nread, outf);
  } while(nread);

  url_fclose(handle);

  fclose(outf);


  /* Test rewind */
  outf=fopen(REWINDFILE, "wb+");
  if(!outf) {
    perror("couldn't open fread output file\n");
    return 1;
  }

  handle = url_fopen("testfile", "r");
  if(!handle) {
    printf("couldn't url_fopen() testfile\n");
    fclose(outf);
    return 2;
  }

  nread = url_fread(buffer, 1, sizeof(buffer), handle);
  fwrite(buffer, 1, nread, outf);
  url_rewind(handle);

  buffer[0]='\n';
  fwrite(buffer, 1, 1, outf);

  nread = url_fread(buffer, 1, sizeof(buffer), handle);
  fwrite(buffer, 1, nread, outf);

  url_fclose(handle);

  fclose(outf);

  return 0;/* all done */
}