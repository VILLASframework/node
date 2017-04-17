# Advanced IO {#advio}

__This page is intended for developers only__

Implements an fopen() abstraction allowing reading from URIs.

This file introduces a c library buffered I/O interface to URI reads it supports fopen(), fread(), fgets(), feof(), fclose(), rewind(). Supported functions have identical prototypes to their normal libc namesakes and are preceaded by 'a'.

Using this code you can replace your program's fopen() with afopen() and fread() with afread() and it become possible to read remote streams instead of (only) local files. Local files (ie those that can be directly fopened) will drop back to using the underlying libc implementations.

Advanced IO (ADVIO) is an extension of the standard IO features (STDIO) which can operate on files stored remotely via a variety of different protocols.
As ADVIO is built upon libcurl, it supports all of [libcurl protocols](https://curl.haxx.se/libcurl/c/CURLOPT_PROTOCOLS.html).

When openeing a file with ADVIO, the file is requested from the remote server and stored as a local temporary file on the disk.
Subsequent operations use this local copy of the file.
When closing or flushing the file, the local copy gets checked for modifications and uploaded to the remote.
 
 There are a few points to keep in mind:
 
 - `afflush()` uploads the file contents to its origin URI if it was changed locally.
 - `afclose()` flushes (and hence uploads) the file only if it has been modified. Modifications are detected by comparing the files SHA1 hashes.
 
## Extensions

The following functions have been added:

- `aupload()` uploads the file irrespectively if it was changed or not.
- `adownload()` downloads the file and discards any local changes.
- `auri()` returns a pointer to the URI.
- `ahash()` returns a pointer to a
 
## Example
 
    #include <advio.h>
    
    AFILE *f;
    char buf[1024];
    
    f = afopen("ftp://user:passwd@server/path/to/file.txt", "r");
    if (!f)
    	serror("Failed to open file");
    
    afread(buf, 1, 1024, f);
    
    printf("%s", buf);
    
    afclose(f);

## Usage for VILLASnode

VILLASnode uses ADVIO currently for:

- Fetchting the configuration file. This means VILLASnode can be started like `villas-node http://server/config.conf`
- As source and destination file for the `file` node-type. Please have a look at the example configuration file `etc/advio.conf`.