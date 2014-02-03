This is **libmftp**, a complete FTP access library written in C. Its goal is to provide an easy way to connect to FTP servers, read and manipulate their content.

I wrote this library with modern standards and features in mind, but also tried to ensure backwards compatibility.

# Features
* IPv6 support
* FTPS (TLS) support (using OpenSSL)
* Compliance to modern FTP standards (RFC 2428 and 3659)
* Separate listener threads (using pthreads)
* Multiple simultaneous connections (will be established automatically when needed)

# Development
This library is currently in development and is **not** ready for productive use yet. I'm currently looking for testers who sacrifice their FTP servers to ensure compatibility. Unfortunately, testing against just one server software does not do the job as their behavior may differ. See **TESTING.md** for a list of already tested server software.

# How to test
Thanks in advance for your help. I have written a program that tests most of the functions of libmftp - all you need is an *empty* folder on your server. If you are not comfortable with a not fully tested library fiddling with your remote files (and you shouldn't be), you may will want to create a separate FTP user and home directory.
Compile libmftp and the test program like this:
```
cmake .
make
```
Then, please run the test like this:
```
test/libmftp_test.out fulltls
```
If you get a warning about missing TLS support, run the test like this instead:
```
test/libmftp_test.out
```
You will be asked for a host URL, a remote working directory, an username and a password.
If the test fails, please create a [New Issue](https://github.com/nkreipke/libmftp/issues/new) with the program output (minus the credentials you entered) and the server software you used (although I don't mind if you don't know this). You can use this mask if you like:

    Server Software: unknown
    Test output:
    ```
    [program output]
    ```

Please "fence" the server output like in the example, otherwise very ugly Markdown would be produced. Please ensure that no private information is included in the log.

# Contributing
There is still enough to do, please see **TODO.md** for a list. Unfortunately, at this time there is no documentation about implementation details, so see ```src/ftpfunctions.h``` to get an overview of the functions you can use.